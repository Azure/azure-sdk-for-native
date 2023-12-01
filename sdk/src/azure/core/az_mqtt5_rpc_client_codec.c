// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include <azure/core/az_mqtt5_rpc.h>
#include <azure/core/az_mqtt5_rpc_client_codec.h>
#include <azure/core/az_result.h>
#include <azure/core/internal/az_log_internal.h>
#include <azure/core/internal/az_mqtt5_topic_parser_internal.h>
#include <azure/core/internal/az_precondition_internal.h>
#include <azure/core/internal/az_result_internal.h>
#include <stdlib.h>

#include <azure/core/_az_cfg.h>

#define sizeofarray(arrayXYZ) \
  (sizeof(arrayXYZ)/sizeof(arrayXYZ[0]))

static const az_span _az_mqtt5_rpc_any_executor_id
    = AZ_SPAN_LITERAL_FROM_STR(_az_MQTT5_TOPIC_PARSER_ANY_EXECUTOR_ID);

static const az_span _az_mqtt5_rpc_cmd_phase_request
    = AZ_SPAN_LITERAL_FROM_STR(_az_MQTT5_TOPIC_PARSER_CMD_PHASE_REQUEST);

static const az_span _az_mqtt5_rpc_cmd_phase_response
    = AZ_SPAN_LITERAL_FROM_STR(_az_MQTT5_TOPIC_PARSER_CMD_PHASE_RESPONSE);

static const az_span _az_mqtt5_rpc_cmd_client_resp_format_prefix
    = AZ_SPAN_LITERAL_FROM_STR(_az_MQTT5_TOPIC_PARSER_RPC_CLIENT_RESPONSE_FORMAT_PREFIX);

AZ_NODISCARD az_mqtt5_rpc_client_codec_options az_mqtt5_rpc_client_codec_options_default()
{
  return (az_mqtt5_rpc_client_codec_options){
    .topic_format = AZ_SPAN_LITERAL_FROM_STR(AZ_MQTT5_RPC_DEFAULT_TOPIC_FORMAT)
  };
}

AZ_NODISCARD az_result az_mqtt5_rpc_client_codec_get_publish_topic(
    az_mqtt5_rpc_client_codec* client,
    az_span executor_id,
    az_span command_name,
    char* mqtt_topic,
    size_t mqtt_topic_size,
    size_t* out_mqtt_topic_length)
{
  _az_PRECONDITION_NOT_NULL(client);
  _az_PRECONDITION_NOT_NULL(mqtt_topic);
  _az_PRECONDITION_RANGE(1, mqtt_topic_size, INT32_MAX);

  az_result ret = AZ_OK;

  az_span mqtt_topic_span = az_span_create((uint8_t*)mqtt_topic, (int32_t)mqtt_topic_size);
  uint32_t required_length = 0;
  az_span topic_formats[] = { client->_internal.options.topic_format };

  ret = _az_mqtt5_topic_parser_replace_tokens_in_format(
      mqtt_topic_span,
      topic_formats,
      sizeofarray(topic_formats),
      AZ_SPAN_EMPTY,
      AZ_SPAN_EMPTY,
      client->_internal.model_id,
      az_span_is_content_equal(executor_id, AZ_SPAN_EMPTY) ? _az_mqtt5_rpc_any_executor_id
                                                           : executor_id,
      AZ_SPAN_EMPTY,
      command_name,
      _az_mqtt5_rpc_cmd_phase_request,
      &required_length);

  if (out_mqtt_topic_length)
  {
    *out_mqtt_topic_length = (size_t)required_length;
  }

  return ret;
}

AZ_NODISCARD az_result az_mqtt5_rpc_client_codec_get_response_property_topic(
    az_mqtt5_rpc_client_codec* client,
    az_span executor_id,
    az_span command_name,
    char* mqtt_topic,
    size_t mqtt_topic_size,
    size_t* out_mqtt_topic_length)
{
  _az_PRECONDITION_NOT_NULL(client);
  _az_PRECONDITION_NOT_NULL(mqtt_topic);
  _az_PRECONDITION_RANGE(1, mqtt_topic_size, INT32_MAX);

  az_result ret = AZ_OK;

  az_span mqtt_topic_span = az_span_create((uint8_t*)mqtt_topic, (int32_t)mqtt_topic_size);
  uint32_t required_length = 0;

  az_span topic_formats[] =
    { _az_mqtt5_rpc_cmd_client_resp_format_prefix, client->_internal.options.topic_format };

  ret = _az_mqtt5_topic_parser_replace_tokens_in_format(
      mqtt_topic_span,
      topic_formats,
      sizeofarray(topic_formats),
      AZ_SPAN_EMPTY,
      client->_internal.client_id,
      client->_internal.model_id,
      az_span_is_content_equal(executor_id, AZ_SPAN_EMPTY) ? _az_mqtt5_rpc_any_executor_id
                                                           : executor_id,
      AZ_SPAN_EMPTY,
      command_name,
      _az_mqtt5_rpc_cmd_phase_response,
      &required_length);

  if (out_mqtt_topic_length)
  {
    *out_mqtt_topic_length = (size_t)required_length;
  }

  return ret;
}

AZ_NODISCARD az_result az_mqtt5_rpc_client_codec_get_subscribe_topic(
    az_mqtt5_rpc_client_codec* client,
    char* mqtt_topic,
    size_t mqtt_topic_size,
    size_t* out_mqtt_topic_length)
{
  _az_PRECONDITION_NOT_NULL(client);
  _az_PRECONDITION_NOT_NULL(mqtt_topic);
  _az_PRECONDITION_RANGE(1, mqtt_topic_size, INT32_MAX);

  az_result ret = AZ_OK;

  az_span mqtt_topic_span = az_span_create((uint8_t*)mqtt_topic, (int32_t)mqtt_topic_size);
  az_span single_level_wildcard
      = AZ_SPAN_FROM_STR(_az_MQTT5_TOPIC_PARSER_SINGLE_LEVEL_WILDCARD_TOKEN);
  uint32_t required_length = 0;
  az_span topic_formats[] =
    { _az_mqtt5_rpc_cmd_client_resp_format_prefix, client->_internal.options.topic_format };

  ret = _az_mqtt5_topic_parser_replace_tokens_in_format(
      mqtt_topic_span,
      topic_formats,
      sizeofarray(topic_formats),
      AZ_SPAN_EMPTY,
      client->_internal.client_id,
      client->_internal.model_id,
      single_level_wildcard,
      AZ_SPAN_EMPTY,
      single_level_wildcard,
      _az_mqtt5_rpc_cmd_phase_response,
      &required_length);

  if (out_mqtt_topic_length)
  {
    *out_mqtt_topic_length = (size_t)required_length;
  }

  return ret;
}

AZ_NODISCARD az_result az_mqtt5_rpc_client_codec_parse_received_topic(
    az_mqtt5_rpc_client_codec* client,
    az_span received_topic,
    az_mqtt5_rpc_client_codec_request_response* out_response)
{
  _az_PRECONDITION_NOT_NULL(client);
  _az_PRECONDITION_VALID_SPAN(received_topic, 1, false);
  _az_PRECONDITION_NOT_NULL(out_response);

  az_span topic_formats[] =
    { _az_mqtt5_rpc_cmd_client_resp_format_prefix, client->_internal.options.topic_format };

  return _az_mqtt5_topic_parser_extract_tokens_from_topic(
      topic_formats,
      sizeofarray(topic_formats),
      received_topic,
      client->_internal.client_id,
      client->_internal.model_id,
      AZ_SPAN_EMPTY,
      AZ_SPAN_EMPTY,
      NULL,
      NULL,
      &out_response->executor_id,
      NULL,
      &out_response->command_name);
}

AZ_NODISCARD az_result az_mqtt5_rpc_client_codec_init(
    az_mqtt5_rpc_client_codec* client,
    az_span client_id,
    az_span model_id,
    az_mqtt5_rpc_client_codec_options* options)
{
  _az_PRECONDITION_NOT_NULL(client);

  if (options == NULL)
  {
    client->_internal.options = az_mqtt5_rpc_client_codec_options_default();
  }
  else if (
      _az_mqtt5_topic_parser_valid_topic_format(options->topic_format))
  {
    client->_internal.options = *options;
  }
  else
  {
    return AZ_ERROR_ARG;
  }

  client->_internal.client_id = client_id;
  client->_internal.model_id = model_id;

  return AZ_OK;
}
