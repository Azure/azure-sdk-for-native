// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

/**
 * @file
 *
 * @brief #az_result definition.
 *
 * @note You MUST NOT use any symbols (macros, functions, structures, enums, etc.)
 * prefixed with an underscore ('_') directly in your application code. These symbols
 * are part of Azure SDK's internal implementation; we do not document these symbols
 * and they are subject to change in future versions of the SDK which would break your code.
 */

#ifndef _az_RESULT_H
#define _az_RESULT_H

#include <stdbool.h>
#include <stdint.h>

#include <azure/core/_az_cfg_prefix.h>

enum
{
  _az_FACILITY_CORE = 0x1,
  _az_FACILITY_PLATFORM = 0x2,
  _az_FACILITY_JSON = 0x3,
  _az_FACILITY_HTTP = 0x4,
  _az_FACILITY_MQTT = 0x5,
  _az_FACILITY_IOT = 0x6,
};

enum
{
  _az_ERROR_FLAG = (int32_t)0x80000000,
};

#define _az_RESULT_MAKE_ERROR(facility, code) \
  ((int32_t)(_az_ERROR_FLAG | ((int32_t)(facility) << 16) | (int32_t)(code)))

#define _az_RESULT_MAKE_SUCCESS(facility, code) \
  ((int32_t)(((int32_t)(facility) << 16) | (int32_t)(code)))

#define AZ_RETURN_IF_FAILED(exp) \
  do \
  { \
    az_result const _result = (exp); \
    if (az_failed(_result)) \
    { \
      return _result; \
    } \
  } while (0)

#define AZ_RETURN_IF_NOT_ENOUGH_SIZE(span, required_size) \
  do \
  { \
    if (az_span_size(span) < required_size || required_size < 0) \
    { \
      return AZ_ERROR_INSUFFICIENT_SPAN_SIZE; \
    } \
  } while (0)

// az_result Bits:
//   - 31 Severity (0 - success, 1 - failure).
//   - 16..30 Facility.
//   - 0..15 Code.

/// @brief The type represents success and error conditions.
typedef enum
{
  // === Core: Success results ====
  /// Success.
  AZ_OK = _az_RESULT_MAKE_SUCCESS(_az_FACILITY_CORE, 0),

  // === Core: Error results ===
  /// A context was canceled, and a function had to return before result was ready.
  AZ_ERROR_CANCELED = _az_RESULT_MAKE_ERROR(_az_FACILITY_CORE, 0),

  /// Input argument does not comply with the requested range of values.
  AZ_ERROR_ARG = _az_RESULT_MAKE_ERROR(_az_FACILITY_CORE, 1),

  /// The size of the provided span is too small.
  AZ_ERROR_INSUFFICIENT_SPAN_SIZE = _az_RESULT_MAKE_ERROR(_az_FACILITY_CORE, 2),

  /// Requested functionality is not implemented.
  AZ_ERROR_NOT_IMPLEMENTED = _az_RESULT_MAKE_ERROR(_az_FACILITY_CORE, 3),

  /// Requested item was not found.
  AZ_ERROR_ITEM_NOT_FOUND = _az_RESULT_MAKE_ERROR(_az_FACILITY_CORE, 4),

  /// Input can't be successfully parsed.
  AZ_ERROR_UNEXPECTED_CHAR = _az_RESULT_MAKE_ERROR(_az_FACILITY_CORE, 5),

  /// Unexpected end of the input data.
  AZ_ERROR_UNEXPECTED_END = _az_RESULT_MAKE_ERROR(_az_FACILITY_CORE, 6),

  /// Not supported.
  AZ_ERROR_NOT_SUPPORTED = _az_RESULT_MAKE_ERROR(_az_FACILITY_CORE, 7),

  // === Platform ===
  /// Dynamic memory allocation request was not successful.
  AZ_ERROR_OUT_OF_MEMORY = _az_RESULT_MAKE_ERROR(_az_FACILITY_PLATFORM, 1),

  // === JSON error codes ===

  /// The kind of the token being read is not compatible with the type of the value attempted being
  /// read.
  AZ_ERROR_JSON_INVALID_STATE = _az_RESULT_MAKE_ERROR(_az_FACILITY_JSON, 1),

  /// The JSON depth is too large.
  AZ_ERROR_JSON_NESTING_OVERFLOW = _az_RESULT_MAKE_ERROR(_az_FACILITY_JSON, 2),

  /// No more JSON text left to process.
  AZ_ERROR_JSON_READER_DONE = _az_RESULT_MAKE_ERROR(_az_FACILITY_JSON, 3),

  // === HTTP error codes ===
  /// The #az_http_response instance is in an invalid state.
  AZ_ERROR_HTTP_INVALID_STATE = _az_RESULT_MAKE_ERROR(_az_FACILITY_HTTP, 1),

  /// HTTP pipeline is malformed.
  AZ_ERROR_HTTP_PIPELINE_INVALID_POLICY = _az_RESULT_MAKE_ERROR(_az_FACILITY_HTTP, 2),

  /// Unknown HTTP method verb.
  AZ_ERROR_HTTP_INVALID_METHOD_VERB = _az_RESULT_MAKE_ERROR(_az_FACILITY_HTTP, 3),

  /// Authentication failed.
  AZ_ERROR_HTTP_AUTHENTICATION_FAILED = _az_RESULT_MAKE_ERROR(_az_FACILITY_HTTP, 4),

  /// HTTP response overflow.
  AZ_ERROR_HTTP_RESPONSE_OVERFLOW = _az_RESULT_MAKE_ERROR(_az_FACILITY_HTTP, 5),

  /// Couldn't reolve host.
  AZ_ERROR_HTTP_RESPONSE_COULDNT_RESOLVE_HOST = _az_RESULT_MAKE_ERROR(_az_FACILITY_HTTP, 6),

  /// Error while parsing HTTP response header.
  AZ_ERROR_HTTP_CORRUPT_RESPONSE_HEADER = _az_RESULT_MAKE_ERROR(_az_FACILITY_HTTP, 7),

  // === HTTP Adapter error codes ===
  /// Generic error in the HTTP transport adapter implementation.
  AZ_ERROR_HTTP_ADAPTER = _az_RESULT_MAKE_ERROR(_az_FACILITY_HTTP, 8),

  // === IoT error codes ===
  /// The IoT topic is not matching the expected format.
  AZ_ERROR_IOT_TOPIC_NO_MATCH = _az_RESULT_MAKE_ERROR(_az_FACILITY_IOT, 1),
} az_result;

/**
 * @brief Checks whether the \p result provided indicates a failure.
 *
 * @param[in] result Result value to check for failure.
 *
 * @retval true \p result indicates a failure.
 * @retval false \p result is successful.
 */
AZ_NODISCARD AZ_INLINE bool az_failed(az_result result)
{
  return ((int32_t)result & (int32_t)_az_ERROR_FLAG) != 0;
}

/**
 * @brief Checks whether the \p result provided indicates a success.
 *
 * @param[in] result Result value to check for success.
 *
 * @retval true \p result indicates success.
 * @retval false \p result is a failure.
 */
AZ_NODISCARD AZ_INLINE bool az_succeeded(az_result result) { return !az_failed(result); }

#include <azure/core/_az_cfg_suffix.h>

#endif // _az_RESULT_H
