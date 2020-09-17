// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef _az_LOG_INTERNAL_H
#define _az_LOG_INTERNAL_H

#include <azure/core/az_log.h>
#include <azure/core/az_span.h>

#include <stdbool.h>

#include <azure/core/_az_cfg_prefix.h>

// If the user hasn't registered any classifications, then we log everything.

#ifndef AZ_NO_LOGGING

bool _az_log_should_write(az_log_classification classification);
void _az_log_write(az_log_classification classification, az_span message);

#define _az_LOG_SHOULD_WRITE(classification) _az_log_should_write((az_log_classification)classification)
#define _az_LOG_WRITE(classification, message) \
  _az_log_write((az_log_classification)classification, message)

#else

#define _az_LOG_SHOULD_WRITE(classification) false

#define _az_LOG_WRITE(classification, message)

#endif // AZ_NO_LOGGING

#include <azure/core/_az_cfg_suffix.h>

#endif // _az_LOG_INTERNAL_H
