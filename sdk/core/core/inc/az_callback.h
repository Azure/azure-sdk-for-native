// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#ifndef AZ_CALLBACK_H
#define AZ_CALLBACK_H

#include <_az_cfg_prefix.h>

typedef void * az_callback_data;

#define AZ_CAT(A, B) A##B

#define AZ_CALLBACK_ARG(NAME) AZ_CAT(NAME, _arg)

/**
 * Defines a callback @NAME type which accepts an argument of type @ARG.
 */
#define AZ_CALLBACK_TYPE(NAME, ARG) \
  typedef ARG AZ_CALLBACK_ARG(NAME); \
  typedef struct { \
    az_result (*func)(az_callback_data const, ARG const); \
    az_callback_data data; \
  } NAME;

/**
 * Defines a function @NAME##_callback which creates a callback of type @CALLBACK
 * using the given @NAME function.
 */
#define AZ_CALLBACK_FUNC(NAME, DATA, CALLBACK) \
  AZ_STATIC_ASSERT(sizeof(DATA) <= sizeof(az_callback_data)) \
  az_result NAME(DATA const, AZ_CALLBACK_ARG(CALLBACK) const); \
  AZ_INLINE CALLBACK AZ_CAT(NAME, _callback)(DATA const data) { \
    return (CALLBACK) { \
      .func = (az_result(*)(az_callback_data, AZ_CALLBACK_ARG(CALLBACK)))NAME, \
      .data = (az_callback_data)data, \
    }; \
  }

#include <_az_cfg_suffix.h>

#endif
