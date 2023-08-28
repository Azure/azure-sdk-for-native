// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include <azure/core/az_mqtt5_rpc_client.h>
#include <azure/core/az_result.h>
#include <azure/core/internal/az_log_internal.h>
#include <stdio.h>
#include <stdlib.h>

#include <azure/core/_az_cfg.h>

AZ_NODISCARD az_mqtt5_rpc_client_options az_mqtt5_rpc_client_options_default()
{
  return (az_mqtt5_rpc_client_options){ .subscribe_timeout_in_seconds
                                        = AZ_MQTT5_RPC_DEFAULT_TIMEOUT_SECONDS };
}

// "vehicles/dtmi:rpc:samples:vehicle;1/commands/+/unlock/__for_mobile-app"
AZ_NODISCARD az_result
az_rpc_client_get_subscription_topic(az_mqtt5_rpc_client* client, az_span* out_subscription_topic)
{
  // #ifndef AZ_NO_PRECONDITION_CHECKING
  _az_PRECONDITION_VALID_SPAN(client->_internal.model_id, 1, false);
  _az_PRECONDITION_VALID_SPAN(client->_internal.client_id, 1, false);
  _az_PRECONDITION_VALID_SPAN(client->_internal.command_name, 1, false);
  int32_t subscribe_topic_min_length = az_span_size(client->_internal.model_id)
      + az_span_size(client->_internal.client_id) + az_span_size(client->_internal.command_name)
      + 28;
  _az_PRECONDITION_VALID_SPAN(*out_subscription_topic, subscribe_topic_min_length, true);
  // #endif

  az_span temp_span = *out_subscription_topic;
  temp_span = az_span_copy(temp_span, AZ_SPAN_FROM_STR("vehicles/"));
  temp_span = az_span_copy(temp_span, client->_internal.model_id);
  temp_span = az_span_copy(temp_span, AZ_SPAN_FROM_STR("/commands/+/"));
  temp_span = az_span_copy(temp_span, client->_internal.command_name);
  temp_span = az_span_copy(temp_span, AZ_SPAN_FROM_STR("/__for_"));
  temp_span = az_span_copy(temp_span, client->_internal.client_id);
  temp_span = az_span_copy_u8(temp_span, '\0');

  *out_subscription_topic = az_span_slice(*out_subscription_topic, 0, subscribe_topic_min_length);

  return AZ_OK;
}

// "vehicles/dtmi:rpc:samples:vehicle;1/commands/vehicle03/unlock/__for_mobile-app"
AZ_NODISCARD az_result
az_rpc_client_get_response_topic(az_mqtt5_rpc_client* client, az_span server_client_id, az_span out_response_topic)
{
  // #ifndef AZ_NO_PRECONDITION_CHECKING
  _az_PRECONDITION_VALID_SPAN(client->_internal.subscription_topic, 1, false);
  _az_PRECONDITION_VALID_SPAN(server_client_id, 1, false);
  int32_t response_topic_min_length = az_span_size(client->_internal.subscription_topic)
      + az_span_size(server_client_id) - 1;
  _az_PRECONDITION_VALID_SPAN(out_response_topic, response_topic_min_length, true);
  // #endif

  int32_t index = az_span_find(client->_internal.subscription_topic, AZ_SPAN_FROM_STR("+"));
  if (index > 0)
  {
    az_span temp_span = out_response_topic;
    temp_span = az_span_copy(temp_span, az_span_slice(client->_internal.subscription_topic, 0, index));
    temp_span = az_span_copy(temp_span, server_client_id);
    temp_span = az_span_copy(temp_span, az_span_slice_to_end(client->_internal.subscription_topic, index + 1));
  }
  else
  {
    return AZ_ERROR_ITEM_NOT_FOUND;
  }

  return AZ_OK;
}

// "vehicles/dtmi:rpc:samples:vehicle;1/commands/vehicle03/unlock"
AZ_NODISCARD az_result
az_rpc_client_get_request_topic(az_mqtt5_rpc_client* client, az_span server_client_id, az_span out_request_topic)
{
#ifndef AZ_NO_PRECONDITION_CHECKING
  _az_PRECONDITION_VALID_SPAN(client->_internal.model_id, 1, false);
  _az_PRECONDITION_VALID_SPAN(client->_internal.command_name, 1, false);
  int32_t request_topic_min_length = az_span_size(client->_internal.model_id)
      + az_span_size(server_client_id) + az_span_size(client->_internal.command_name) + 23;
  _az_PRECONDITION_VALID_SPAN(out_request_topic, request_topic_min_length, true);
#endif

  az_span temp_span = out_request_topic;
  temp_span = az_span_copy(temp_span, AZ_SPAN_FROM_STR("vehicles/"));
  temp_span = az_span_copy(temp_span, client->_internal.model_id);
  temp_span = az_span_copy(temp_span, AZ_SPAN_FROM_STR("/commands/"));
  temp_span = az_span_copy(temp_span, server_client_id);
  temp_span = az_span_copy_u8(temp_span, '/');
  temp_span = az_span_copy(temp_span, client->_internal.command_name);
  temp_span = az_span_copy_u8(temp_span, '\0');

  return AZ_OK;
}

AZ_NODISCARD az_result az_rpc_client_init(
    az_mqtt5_rpc_client* client,
    az_span client_id,
    az_span model_id,
    az_span command_name,
    az_span response_topic_buffer,
    az_span request_topic_buffer,
    az_span subscribe_topic_buffer,
    az_mqtt5_rpc_client_options* options)
{
  _az_PRECONDITION_NOT_NULL(client);
  client->_internal.options = options == NULL ? az_mqtt5_rpc_client_options_default() : *options;

  if (client->_internal.options.subscribe_timeout_in_seconds <= 0)
  {
    return AZ_ERROR_ARG;
  }

  _az_PRECONDITION_VALID_SPAN(client_id, 1, false);
  _az_PRECONDITION_VALID_SPAN(model_id, 1, false);
  _az_PRECONDITION_VALID_SPAN(command_name, 1, false);

  client->_internal.client_id = client_id;
  client->_internal.model_id = model_id;
  client->_internal.command_name = command_name;
  client->_internal.response_topic_buffer = response_topic_buffer;
  client->_internal.request_topic_buffer = request_topic_buffer;

  _az_RETURN_IF_FAILED(az_rpc_client_get_subscription_topic(client, &subscribe_topic_buffer));
  client->_internal.subscription_topic = subscribe_topic_buffer;

  return AZ_OK;
}
