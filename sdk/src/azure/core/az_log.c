// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include "az_span_private.h"
#include <azure/core/az_config.h>
#include <azure/core/az_http.h>
#include <azure/core/az_http_transport.h>
#include <azure/core/az_log.h>
#include <azure/core/az_span.h>
#include <azure/core/internal/az_http_internal.h>
#include <azure/core/internal/az_log_internal.h>

#include <stddef.h>

#include <azure/core/_az_cfg.h>

#ifndef AZ_NO_LOGGING

// Only using volatile here, not for thread safety, but so that the compiler does not optimize what
// it falsely thinks are stale reads.
static az_log_message_fn volatile _az_log_message_callback = NULL;
static az_log_should_write_fn volatile _az_log_should_write_callback = NULL;

void az_log_set_message_callback(az_log_message_fn az_log_message_callback)
{
  // We assume assignments are atomic for the supported platforms and compilers.
  _az_log_message_callback = az_log_message_callback;
}

void az_log_set_filter_callback(az_log_should_write_fn az_log_should_write_callback)
{
  // We assume assignments are atomic for the supported platforms and compilers.
  _az_log_should_write_callback = az_log_should_write_callback;
}

// _az_log_write_engine is a function private to this .c file; it contains the code to handle
// _az_log_should_write & _az_log_write.
//
// If log_it is false, then the function returns true or false indicating whether the message
// should be logged (without actually logging it).
//
// If log_it is true, then the function logs the message (if it should) and returns true or
// false indicating whether it was logged.
static bool _az_log_write_engine(bool log_it, az_log_classification classification, az_span message)
{
  _az_PRECONDITION(classification > 0);
  _az_PRECONDITION_VALID_SPAN(message, 0, true);

  // Copy the volatile fields to local variables so that they don't change within this function.
  az_log_message_fn const message_callback = _az_log_message_callback;
  az_log_should_write_fn const should_write_callback = _az_log_should_write_callback;

  // If the user hasn't registered a should_write_callback, then we log everything, as long as a
  // message_callback metho was provided.
  if (message_callback != NULL
      && (should_write_callback == NULL || should_write_callback(classification)))
  {
    if (log_it)
    {
      message_callback(classification, message);
    }
    return true;
  }
  return false;
}

// This function returns whether or not the passed-in message should be logged.
bool _az_log_should_write(az_log_classification classification)
{
  return _az_log_write_engine(false, classification, AZ_SPAN_EMPTY);
}

// This function attempts to log the passed-in message.
void _az_log_write(az_log_classification classification, az_span message)
{
  (void)_az_log_write_engine(true, classification, message);
}

#endif // AZ_NO_LOGGING
