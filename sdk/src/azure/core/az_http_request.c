// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include "az_http_header_validation_private.h"
#include "az_http_private.h"
#include "az_span_private.h"

#include <azure/core/az_http.h>
#include <azure/core/az_http_transport.h>
#include <azure/core/az_precondition.h>
#include <azure/core/internal/az_http_internal.h>
#include <azure/core/internal/az_precondition_internal.h>
#include <azure/core/internal/az_span_internal.h>

#include <assert.h>

#include <azure/core/_az_cfg.h>

static AZ_NODISCARD bool _az_is_question_mark(uint8_t ch) { return ch == '?'; }

AZ_NODISCARD az_result az_http_request_init(
    az_http_request* out_request,
    az_context* context,
    az_span method,
    az_span url,
    int32_t url_length,
    az_span headers_buffer,
    az_span body)
{
  _az_PRECONDITION_NOT_NULL(out_request);
  _az_PRECONDITION_VALID_SPAN(method, 1, false);
  _az_PRECONDITION_VALID_SPAN(url, 1, false);
  _az_PRECONDITION_VALID_SPAN(headers_buffer, 0, false);

  int32_t query_start = 0;
  az_result url_with_query = _az_span_scan_until(
      az_span_create(az_span_ptr(url), url_length), _az_is_question_mark, &query_start);

  *out_request
      = (az_http_request){ ._internal = {
                               .context = context,
                               .method = method,
                               .url = url,
                               .url_length = url_length,
                               /* query start is set to 0 if there is not a question mark so the
                                  next time query parameter is appended, a question mark will be
                                  added at url length. (+1 jumps the `?`) */
                               .query_start
                               = url_with_query == AZ_ERROR_ITEM_NOT_FOUND ? 0 : query_start + 1,
                               .headers = headers_buffer,
                               .headers_length = 0,
                               .max_headers
                               = az_span_size(headers_buffer) / (int32_t)sizeof(az_pair),
                               .retry_headers_start_byte_offset = 0,
                               .body = body,
                           } };

  return AZ_OK;
}

AZ_NODISCARD az_result
az_http_request_append_path(az_http_request* ref_request, az_span path, bool is_path_url_encoded)
{
  _az_PRECONDITION_NOT_NULL(ref_request);

  // get the query starting point.
  bool url_with_question_mark = ref_request->_internal.query_start > 0;
  int32_t query_start = url_with_question_mark ? ref_request->_internal.query_start - 1
                                               : ref_request->_internal.url_length;

  /* use replace twice. Yes, we will have 2 right shift (one on each replace), but we rely on
   * replace function for doing this movements only and avoid updating manually. We could also
   * create a temp buffer to join "/" and path and then use replace. But that will cost us more
   * stack memory.
   */
  AZ_RETURN_IF_FAILED(_az_span_replace(
      ref_request->_internal.url,
      ref_request->_internal.url_length,
      query_start,
      query_start,
      AZ_SPAN_FROM_STR("/"),
      false,
      NULL));
  query_start += 1; // a size of "/"
  ++ref_request->_internal.url_length;

  int32_t path_size = az_span_size(path);
  AZ_RETURN_IF_FAILED(_az_span_replace(
      ref_request->_internal.url,
      ref_request->_internal.url_length,
      query_start,
      query_start,
      path,
      !is_path_url_encoded,
      &path_size));

  query_start += path_size;
  ref_request->_internal.url_length += path_size;

  // update query start
  if (url_with_question_mark)
  {
    ref_request->_internal.query_start = query_start + 1;
  }

  return AZ_OK;
}

AZ_NODISCARD az_result az_http_request_set_query_parameter(
    az_http_request* ref_request,
    az_span name,
    az_span value,
    bool is_query_url_encoded)
{
  _az_PRECONDITION_NOT_NULL(ref_request);
  _az_PRECONDITION_VALID_SPAN(name, 1, false);
  _az_PRECONDITION_VALID_SPAN(value, 1, false);

  // name or value can't be empty
  _az_PRECONDITION(az_span_size(name) > 0 && az_span_size(value) > 0);

  az_span url_remainder
      = az_span_slice_to_end(ref_request->_internal.url, ref_request->_internal.url_length);

  // Adding query parameter. Adding +2 to required length to include extra required symbols `=`
  // and `?` or `&`.
  int32_t required_length
      = (is_query_url_encoded
             ? (az_span_size(name) + az_span_size(value))
             : (_az_span_url_encode_calc_length(name) + _az_span_url_encode_calc_length(value)))
      + 2;
  AZ_RETURN_IF_NOT_ENOUGH_SIZE(url_remainder, required_length);

  // Append either '?' or '&'
  uint8_t separator;
  if (ref_request->_internal.query_start == 0)
  {
    separator = '?';

    // update QPs starting position when it's 0
    ref_request->_internal.query_start = ref_request->_internal.url_length + 1;
  }
  else
  {
    separator = '&';
  }

  url_remainder = az_span_copy_u8(url_remainder, separator);

  int32_t encoding_size = 0;
  // Append parameter name
  if (is_query_url_encoded)
  {
    url_remainder = az_span_copy(url_remainder, name);
  }
  else
  {
    AZ_RETURN_IF_FAILED(_az_span_url_encode(url_remainder, name, &encoding_size));
    url_remainder = az_span_slice_to_end(url_remainder, encoding_size);
  }

  // Append equal sym
  url_remainder = az_span_copy_u8(url_remainder, '=');

  // Parameter value
  if (is_query_url_encoded)
  {
    url_remainder = az_span_copy(url_remainder, value);
  }
  else
  {
    AZ_RETURN_IF_FAILED(_az_span_url_encode(url_remainder, value, &encoding_size));
  }

  ref_request->_internal.url_length += required_length;

  return AZ_OK;
}

AZ_NODISCARD az_result
az_http_request_append_header(az_http_request* ref_request, az_span key, az_span value)
{
  _az_PRECONDITION_NOT_NULL(ref_request);

  // remove whitespace characters from key and value
  key = _az_span_trim_whitespace(key);
  value = _az_span_trim_whitespace(value);

  _az_PRECONDITION_VALID_SPAN(key, 1, false);

  // Make this function to only work with valid input for header name
  _az_PRECONDITION(az_http_is_valid_header_name(key));

  az_span headers = ref_request->_internal.headers;
  az_pair header_to_append = az_pair_init(key, value);

  AZ_RETURN_IF_NOT_ENOUGH_SIZE(headers, (int32_t)sizeof header_to_append);

  az_span_copy(
      az_span_slice_to_end(
          headers, (int32_t)sizeof(az_pair) * ref_request->_internal.headers_length),
      az_span_create((uint8_t*)&header_to_append, sizeof header_to_append));

  ref_request->_internal.headers_length++;

  return AZ_OK;
}

AZ_NODISCARD az_result
az_http_request_get_header(az_http_request const* request, int32_t index, az_pair* out_header)
{
  _az_PRECONDITION_NOT_NULL(request);
  _az_PRECONDITION_NOT_NULL(out_header);

  if (index >= az_http_request_headers_count(request))
  {
    return AZ_ERROR_ARG;
  }

  *out_header = ((az_pair*)az_span_ptr(request->_internal.headers))[index];
  return AZ_OK;
}

AZ_NODISCARD az_result
az_http_request_get_method(az_http_request const* request, az_http_method* out_method)
{
  _az_PRECONDITION_NOT_NULL(request);
  _az_PRECONDITION_NOT_NULL(out_method);

  *out_method = request->_internal.method;

  return AZ_OK;
}

AZ_NODISCARD az_result az_http_request_get_url(az_http_request const* request, az_span* out_url)
{
  _az_PRECONDITION_NOT_NULL(request);
  _az_PRECONDITION_NOT_NULL(out_url);

  *out_url = az_span_slice(request->_internal.url, 0, request->_internal.url_length);

  return AZ_OK;
}

AZ_NODISCARD az_result az_http_request_get_body(az_http_request const* request, az_span* out_body)
{
  _az_PRECONDITION_NOT_NULL(request);
  _az_PRECONDITION_NOT_NULL(out_body);

  *out_body = request->_internal.body;
  return AZ_OK;
}

AZ_NODISCARD int32_t az_http_request_headers_count(az_http_request const* request)
{
  return request->_internal.headers_length;
}
