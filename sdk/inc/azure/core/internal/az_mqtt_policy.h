// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

/**
 * @file
 *
 * @brief This file defines the API to the Azure MQTT Policy and its related types.
 *
 * @note You MUST NOT use any symbols (macros, functions, structures, enums, etc.)
 * prefixed with an underscore ('_') directly in your application code. These symbols
 * are part of Azure SDK's internal implementation; we do not document these symbols
 * and they are subject to change in future versions of the SDK which would break your code.
 */

#ifndef _az_MQTT_POLICY
#define _az_MQTT_POLICY

#include <azure/core/az_context.h>
#include <azure/core/az_event_policy.h>
#include <azure/core/az_mqtt.h>
#include <azure/core/az_result.h>
#include <azure/core/az_span.h>

#include <azure/core/_az_cfg_prefix.h>

typedef struct
{
  az_event_policy policy;
  az_context* context;
  az_mqtt* mqtt;
} _az_mqtt_policy;

/**
 * @brief Initializes an MQTT Policy.
 *
 * @param mqtt_policy The #_az_mqtt_policy to initialize.
 * @param mqtt The #az_mqtt to use for the policy.
 * @param context The #az_context to use for the policy.
 * @param outbound_policy The #az_event_policy to use for outbound events.
 * @param inbound_policy The #az_event_policy to use for inbound events.
 * @return An #az_result value indicating the result of the operation.
 */
AZ_NODISCARD az_result _az_mqtt_policy_init(
    _az_mqtt_policy* mqtt_policy,
    az_mqtt* mqtt,
    az_context* context,
    az_event_policy* outbound_policy,
    az_event_policy* inbound_policy);

#include <azure/core/_az_cfg_suffix.h>

#endif // _az_MQTT_POLICY
