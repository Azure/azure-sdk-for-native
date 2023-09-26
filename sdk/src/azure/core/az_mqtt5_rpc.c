// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include <azure/core/az_mqtt5_rpc.h>
#include <azure/core/az_result.h>
#include <azure/core/az_span.h>
#include <azure/core/internal/az_log_internal.h>
#include <azure/core/internal/az_precondition_internal.h>
#include <stdio.h>

#if defined(TRANSPORT_MOSQUITTO)
#include <mosquitto.h>
#endif

#include <azure/core/_az_cfg.h>

static const az_span model_id_key = AZ_SPAN_LITERAL_FROM_STR("{serviceId}");
static const az_span command_name_key = AZ_SPAN_LITERAL_FROM_STR("{name}");
static const az_span executor_client_id_key = AZ_SPAN_LITERAL_FROM_STR("{executorId}");
static const az_span invoker_client_id_key = AZ_SPAN_LITERAL_FROM_STR("{invokerId}");

AZ_NODISCARD static int32_t _az_rpc_calculate_substitution_length(
    az_span format,
    az_span key,
    az_span value);
AZ_NODISCARD static az_result _az_rpc_substitute_key_for_value(
    az_span format,
    uint8_t* format_buf,
    az_span key,
    az_span value,
    az_span out_topic);

/**
 * @brief Helper function to obtain the next level of a topic.
 *
 * @param topic Topic to get the next level of.
 * @return An #az_span value containing the next level of the topic including the backslash.
 */
AZ_NODISCARD AZ_INLINE az_span _get_next_topic_level(az_span topic)
{
  int32_t pos = az_span_find(topic, AZ_SPAN_FROM_STR("/"));
  if (pos == -1)
  {
    int32_t pos_null_char = az_span_find(topic, AZ_SPAN_FROM_STR("\0"));
    if (pos_null_char == -1)
    {
      return topic;
    }
    return az_span_slice(topic, 0, pos_null_char);
  }

  return az_span_slice(topic, 0, pos + 1);
}

/**
 * @brief Helper function to check if a topic has a backslash at the end of it.
 *
 * @param topic Topic to check.
 * @return True if the topic has a backslash at the end of it, false otherwise.
 */
AZ_NODISCARD AZ_INLINE bool _has_backslash_in_topic(az_span topic)
{
  if (az_span_size(topic) == 0)
  {
    return false;
  }
  return az_span_find(topic, AZ_SPAN_FROM_STR("/")) == az_span_size(topic) - 1;
}

AZ_NODISCARD bool _az_span_topic_matches_filter(az_span topic_filter, az_span topic)
{
if (az_span_size(topic_filter) == 0 || az_span_size(topic) == 0)
  {
    return false;
  }

  // Checking for invalid wildcard usage in topic.
  if (az_span_find(topic, AZ_SPAN_FROM_STR("#")) > 0
      || az_span_find(topic, AZ_SPAN_FROM_STR("+")) > 0)
  {
    return false;
  }

  az_span filter_remaining = topic_filter;
  az_span topic_remaining = topic;

  az_span filter_level = _get_next_topic_level(filter_remaining);
  az_span topic_level = _get_next_topic_level(topic_remaining);

  while (az_span_size(filter_level) != 0)
  {
    if (az_span_ptr(filter_level)[0] == '#')
    {
      if (az_span_size(filter_level) != 1)
      {
        return false;
      }
      return true;
    }
    else if (az_span_ptr(filter_level)[0] == '+')
    {
      if (az_span_size(filter_level) == 1)
      {
        if (_has_backslash_in_topic(topic_level))
        {
          return false;
        }
        return true;
      }
      else if (az_span_size(filter_level) == 2 && _has_backslash_in_topic(filter_level))
      {
        // Special case: Topic Filter = "foo/+/#" and Topic = "foo/bar"
        if (!_has_backslash_in_topic(topic_level))
        {
          filter_remaining = az_span_slice_to_end(filter_remaining, az_span_size(filter_level));
          filter_level = _get_next_topic_level(filter_remaining);
          if (az_span_size(filter_level) == 1 && az_span_ptr(filter_level)[0] == '#')
          {
            return true;
          }
          return false;
        }
      }
      else
      {
        return false;
      }
    }
    else
    {
      if (!az_span_is_content_equal(filter_level, topic_level))
      {
        // Special case: Topic Filter = "foo/#" and Topic = "foo"
        if (!_has_backslash_in_topic(topic_level) && _has_backslash_in_topic(filter_level))
        {
          az_span sub_no_backslash = az_span_slice(filter_level, 0, az_span_size(filter_level) - 1);
          if (az_span_is_content_equal(sub_no_backslash, topic_level))
          {
            filter_remaining = az_span_slice_to_end(filter_remaining, az_span_size(filter_level));
            filter_level = _get_next_topic_level(filter_remaining);
            if (az_span_size(filter_level) == 1 && az_span_ptr(filter_level)[0] == '#')
            {
              return true;
            }
          }
        }
        return false;
      }
    }

    filter_remaining = az_span_slice_to_end(filter_remaining, az_span_size(filter_level));
    topic_remaining = az_span_slice_to_end(topic_remaining, az_span_size(topic_level));

    filter_level = _get_next_topic_level(filter_remaining);
    topic_level = _get_next_topic_level(topic_remaining);
  }

  if (az_span_size(topic_level) != 0)
  {
    return false;
  }

  return true;
}

AZ_NODISCARD bool az_mqtt5_rpc_status_failed(az_mqtt5_rpc_status status)
{
  return (status < 200 || status >= 300);
}

AZ_NODISCARD static int32_t _az_rpc_calculate_substitution_length(
    az_span format,
    az_span key,
    az_span value)
{
  if (az_span_find(format, key) >= 0)
  {
    _az_PRECONDITION_VALID_SPAN(value, 1, false);
    return az_span_size(value) - az_span_size(key);
  }
  return 0;
}

AZ_NODISCARD static az_result _az_rpc_substitute_key_for_value(
    az_span format,
    uint8_t* format_buf,
    az_span key,
    az_span value,
    az_span out_topic)
{
  az_span temp_span = out_topic;

  int32_t index = az_span_find(format, key);
  if (index >= 0)
  {
    // make a copy of the format to copy from
    az_span temp_format_buf = az_span_create(format_buf, az_span_size(format));
    az_span_copy(temp_format_buf, format);

    temp_span = az_span_copy(temp_span, az_span_slice(temp_format_buf, 0, index));
    temp_span = az_span_copy(temp_span, value);
    temp_span
        = az_span_copy(temp_span, az_span_slice_to_end(temp_format_buf, index + az_span_size(key)));
    return AZ_OK;
  }
  return AZ_ERROR_ITEM_NOT_FOUND;
}

AZ_NODISCARD az_result az_rpc_get_topic_from_format(
    az_span format,
    az_span model_id,
    az_span executor_client_id,
    az_span invoker_client_id,
    az_span command_name,
    az_span out_topic,
    int32_t* out_topic_length)
{
  int32_t format_size = az_span_size(format);

  // Determine the length of the final topic and the max length of the topic while performing
  // substitutions
  int32_t topic_length = format_size + 1; // + 1 for the null terminator
  int32_t max_temp_length = 0;
  if ((topic_length += _az_rpc_calculate_substitution_length(format, model_id_key, model_id))
      > max_temp_length)
  {
    max_temp_length = topic_length;
  }
  if ((topic_length
       += _az_rpc_calculate_substitution_length(format, command_name_key, command_name))
      > max_temp_length)
  {
    max_temp_length = topic_length;
  }
  if ((topic_length
       += _az_rpc_calculate_substitution_length(format, executor_client_id_key, executor_client_id))
      > max_temp_length)
  {
    max_temp_length = topic_length;
  }
  if ((topic_length
       += _az_rpc_calculate_substitution_length(format, invoker_client_id_key, invoker_client_id))
      > max_temp_length)
  {
    max_temp_length = topic_length;
  }

  // Must be large enough to fit the entire format as items are substituted throughout the function
  // even if that's larger than the output topic
  _az_PRECONDITION_VALID_SPAN(out_topic, max_temp_length, true);

  // Must be large enough to fit the entire format even if that's larger than the output topic
  int32_t format_buf_size
      = format_size > az_span_size(out_topic) ? format_size : az_span_size(out_topic);
  uint8_t format_buf[format_buf_size];

  if (az_result_succeeded(
          _az_rpc_substitute_key_for_value(format, format_buf, model_id_key, model_id, out_topic)))
  {
    format_size += az_span_size(model_id);
    format_size -= az_span_size(model_id_key);
    format = az_span_slice(out_topic, 0, format_size);
  }

  if (az_result_succeeded(_az_rpc_substitute_key_for_value(
          format, format_buf, command_name_key, command_name, out_topic)))
  {
    format_size += az_span_size(command_name);
    format_size -= az_span_size(command_name_key);
    format = az_span_slice(out_topic, 0, format_size);
  }

  if (az_result_succeeded(_az_rpc_substitute_key_for_value(
          format, format_buf, executor_client_id_key, executor_client_id, out_topic)))
  {
    format_size += az_span_size(executor_client_id);
    format_size -= az_span_size(executor_client_id_key);
    format = az_span_slice(out_topic, 0, format_size);
  }

  if (az_result_succeeded(_az_rpc_substitute_key_for_value(
          format, format_buf, invoker_client_id_key, invoker_client_id, out_topic)))
  {
    format_size += az_span_size(invoker_client_id);
    format_size -= az_span_size(invoker_client_id_key);
    format = az_span_slice(out_topic, 0, format_size);
  }

  // add null terminator to end of topic
  az_span temp_topic = out_topic;
  temp_topic = az_span_copy(temp_topic, format);
  az_span_copy_u8(temp_topic, '\0');

  if (out_topic_length != NULL)
  {
    *out_topic_length = topic_length;
  }

  return AZ_OK;
}
