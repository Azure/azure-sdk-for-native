// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include <azure/core/az_event_policy.h>
#include <azure/core/az_mqtt_config.h>
#include <azure/core/az_mqtt_connection.h>
#include <azure/core/az_platform.h>
#include <azure/core/az_result.h>
#include <azure/core/az_span.h>
#include <azure/core/internal/az_log_internal.h>

#include <azure/core/_az_cfg.h>

AZ_NODISCARD az_mqtt_connection_options az_mqtt_connection_options_default()
{
  return (az_mqtt_connection_options){
    .hostname = AZ_SPAN_EMPTY,
    .port = AZ_MQTT_DEFAULT_CONNECT_PORT,
    .connection_management = false,
    .client_id_buffer = AZ_SPAN_EMPTY,
    .username_buffer = AZ_SPAN_EMPTY,
    .password_buffer = AZ_SPAN_EMPTY,
  };
}

AZ_NODISCARD az_result az_mqtt_connection_init(
    az_mqtt_connection* client,
    az_context* context,
    az_mqtt* mqtt_client,
    az_mqtt_connection_callback event_callback,
    az_mqtt_connection_options* options)
{
  _az_PRECONDITION_NOT_NULL(client);
  _az_PRECONDITION_NOT_NULL(mqtt_client);
  _az_PRECONDITION_NOT_NULL(event_callback);

  client->_internal.options = options == NULL ? az_mqtt_connection_options_default() : *options;
  client->_internal.event_callback = event_callback;

  if (client->_internal.options.connection_management)
  {
    // The pipeline contains the connection_policy.
    // subclients_policy --> connection_policy --> az_mqtt_policy
    //    outbound                                   inbound

    _az_RETURN_IF_FAILED(_az_mqtt_policy_init(
        &client->_internal.mqtt_policy,
        mqtt_client,
        context,
        NULL,
        (az_event_policy*)&client->_internal.connection_policy));

    _az_RETURN_IF_FAILED(_az_mqtt_connection_policy_init(
        (_az_hfsm*)client,
        (az_event_policy*)&client->_internal.mqtt_policy,
        (az_event_policy*)&client->_internal.subclient_policy));

    _az_RETURN_IF_FAILED(_az_event_policy_subclients_init(
        &client->_internal.subclient_policy, (az_event_policy*)client, NULL));
  }
  else
  {
    // The pipeline does not contain the connection_policy.
    // subclients_policy --> az_mqtt_policy
    //    outbound              inbound
    _az_RETURN_IF_FAILED(_az_mqtt_policy_init(
        &client->_internal.mqtt_policy,
        mqtt_client,
        context,
        NULL,
        (az_event_policy*)&client->_internal.subclient_policy));

    // Unused. Initialize to NULL.
    client->_internal.connection_policy = (_az_hfsm){ 0 };

    _az_RETURN_IF_FAILED(_az_event_policy_subclients_init(
        &client->_internal.subclient_policy,
        (az_event_policy*)&client->_internal.mqtt_policy,
        NULL));
  }

  _az_RETURN_IF_FAILED(_az_event_pipeline_init(
      &client->_internal.event_pipeline,
      (az_event_policy*)&client->_internal.subclient_policy,
      (az_event_policy*)&client->_internal.mqtt_policy));

  // Attach the az_mqtt client to this pipeline.
  mqtt_client->_internal.platform_mqtt.pipeline = &client->_internal.event_pipeline;

  return AZ_OK;
}
