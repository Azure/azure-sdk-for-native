// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include <stdint.h>

#include "az_iot_hub_client.h"
#include <az_result.h>
#include <az_span.h>
#include <az_span_internal.h>
#include <az_iot_common_internal.h>

#include <az_log_internal.h>
#include <az_precondition_internal.h>

#include <_az_cfg.h>

static const uint8_t hashtag = '#';
static const uint8_t null_terminator = '\0';
static const az_span methods_topic_prefix = AZ_SPAN_LITERAL_FROM_STR("$iothub/methods/");
static const az_span methods_topic_filter_suffix = AZ_SPAN_LITERAL_FROM_STR("POST/");
static const az_span methods_response_topic_result = AZ_SPAN_LITERAL_FROM_STR("res/");
static const az_span methods_response_topic_properties = AZ_SPAN_LITERAL_FROM_STR("/?$rid=");

AZ_NODISCARD az_result az_iot_hub_client_methods_get_subscribe_topic_filter(
    az_iot_hub_client const* client,
    char* mqtt_topic_filter,
    size_t mqtt_topic_filter_size,
    size_t* out_mqtt_topic_filter_length)
{
  _az_PRECONDITION_NOT_NULL(client);
  _az_PRECONDITION_NOT_NULL(mqtt_topic_filter);
  _az_PRECONDITION(mqtt_topic_filter_size > 0);

  (void)client;

  az_span mqtt_topic_filter_span
      = az_span_init((uint8_t*)mqtt_topic_filter, (int32_t)mqtt_topic_filter_size);

  int32_t required_length = az_span_size(methods_topic_prefix)
      + az_span_size(methods_topic_filter_suffix) + (int32_t)sizeof(hashtag);

  AZ_RETURN_IF_NOT_ENOUGH_SIZE(
      mqtt_topic_filter_span, required_length + (int32_t)sizeof(null_terminator));

  // TODO: Merge these two calls into one since they are copying two strings literals.
  az_span remainder = az_span_copy(mqtt_topic_filter_span, methods_topic_prefix);
  remainder = az_span_copy(remainder, methods_topic_filter_suffix);
  remainder = az_span_copy_u8(remainder, hashtag);
  az_span_copy_u8(remainder, null_terminator);

  if (out_mqtt_topic_filter_length)
  {
    *out_mqtt_topic_filter_length = (size_t)required_length;
  }

  return AZ_OK;
}

AZ_NODISCARD az_result az_iot_hub_client_methods_parse_received_topic(
    az_iot_hub_client const* client,
    az_span received_topic,
    az_iot_hub_client_method_request* out_request)
{
  _az_PRECONDITION_NOT_NULL(client);
  _az_PRECONDITION_VALID_SPAN(received_topic, 1, false);
  _az_PRECONDITION_NOT_NULL(out_request);

  (void)client;

  int32_t index = az_span_find(received_topic, methods_topic_prefix);

  if (index == -1)
  {
    return AZ_ERROR_IOT_TOPIC_NO_MATCH;
  }

  _az_log_write(AZ_LOG_MQTT_RECEIVED_TOPIC, received_topic);

  received_topic = az_span_slice(
      received_topic, index + az_span_size(methods_topic_prefix), az_span_size(received_topic));

  index = az_span_find(received_topic, methods_topic_filter_suffix);

  if (index == -1)
  {
    return AZ_ERROR_IOT_TOPIC_NO_MATCH;
  }

  received_topic = az_span_slice(
      received_topic,
      index + az_span_size(methods_topic_filter_suffix),
      az_span_size(received_topic));

  index = az_span_find(received_topic, methods_response_topic_properties);

  if (index == -1)
  {
    return AZ_ERROR_IOT_TOPIC_NO_MATCH;
  }

  out_request->name = az_span_slice(received_topic, 0, index);
  AZ_RETURN_IF_FAILED(az_span_atou32(az_span_slice(
      received_topic,
      index + az_span_size(methods_response_topic_properties),
      az_span_size(received_topic)), &out_request->request_id));

  return AZ_OK;
}

AZ_NODISCARD az_result az_iot_hub_client_methods_response_get_publish_topic(
    az_iot_hub_client const* client,
    uint32_t request_id,
    uint16_t status,
    char* mqtt_topic,
    size_t mqtt_topic_size,
    size_t* out_mqtt_topic_length)
{
  _az_PRECONDITION_NOT_NULL(client);
  _az_PRECONDITION_NOT_NULL(mqtt_topic);
  _az_PRECONDITION(mqtt_topic_size);

  (void)client;

  az_span mqtt_topic_span = az_span_init((uint8_t*)mqtt_topic, (int32_t)mqtt_topic_size);
  int32_t required_length = az_span_size(methods_topic_prefix)
      + az_span_size(methods_response_topic_result) + _az_iot_u32toa_size(status)
      + az_span_size(methods_response_topic_properties) + _az_iot_u32toa_size(request_id);

  AZ_RETURN_IF_NOT_ENOUGH_SIZE(mqtt_topic_span, required_length + (int32_t)sizeof(null_terminator));

  // Ignore the result value since we have checked that there is enough space
  az_result unused;

  az_span remainder = az_span_copy(mqtt_topic_span, methods_topic_prefix);
  remainder = az_span_copy(remainder, methods_response_topic_result);
  unused = az_span_u32toa(remainder, (uint32_t)status, &remainder);
  remainder = az_span_copy(remainder, methods_response_topic_properties);
  unused = az_span_u32toa(remainder, request_id, &remainder);
  az_span_copy_u8(remainder, null_terminator);

  (void)unused;

  if (out_mqtt_topic_length)
  {
    *out_mqtt_topic_length = (size_t)required_length;
  }

  return AZ_OK;
}
