// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include <azure/core/az_platform.h>
#include <azure/core/internal/az_config_internal.h>
#include <azure/core/internal/az_precondition_internal.h>

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <azure/core/_az_cfg.h>

#ifdef __linux__
  #define _az_PLATFORM_POSIX_CLOCK_ID CLOCK_BOOTTIME
#else
  #define _az_PLATFORM_POSIX_CLOCK_ID CLOCK_MONOTONIC
#endif

static void _timer_callback_handler(union sigval sv)
{
  _az_platform_timer* timer_handle = sv.sival_ptr;

  _az_PRECONDITION_NOT_NULL(timer_handle);
  _az_PRECONDITION_NOT_NULL(timer_handle->_internal.platform_timer._internal.callback);

  timer_handle->_internal.platform_timer._internal.callback(
      timer_handle->_internal.platform_timer._internal.sdk_data);
}

AZ_NODISCARD az_result az_platform_clock_msec(int64_t* out_clock_msec)
{
  _az_PRECONDITION_NOT_NULL(out_clock_msec);
  struct timespec curr_time;

  if (clock_getres(_az_PLATFORM_POSIX_CLOCK_ID, &curr_time) == 0) // Check if high-res timer is available
  {
    clock_gettime(_az_PLATFORM_POSIX_CLOCK_ID, &curr_time);
    *out_clock_msec = ((int64_t)curr_time.tv_sec * _az_TIME_MILLISECONDS_PER_SECOND)
        + ((int64_t)curr_time.tv_nsec / _az_TIME_NANOSECONDS_PER_MILLISECOND);
  }
  else
  {
    // NOLINTNEXTLINE(bugprone-misplaced-widening-cast)
    *out_clock_msec = (int64_t)((time(NULL)) * _az_TIME_MILLISECONDS_PER_SECOND);
  }

  return AZ_OK;
}

AZ_NODISCARD az_result az_platform_sleep_msec(int32_t milliseconds)
{
  (void)usleep((useconds_t)milliseconds * _az_TIME_MICROSECONDS_PER_MILLISECOND);
  return AZ_OK;
}

AZ_NODISCARD az_result az_platform_get_random(int32_t* out_random)
{
  _az_PRECONDITION_NOT_NULL(out_random);

  *out_random = (int32_t)random();
  return AZ_OK;
}

AZ_NODISCARD az_result az_platform_timer_create(
    _az_platform_timer* timer_handle,
    _az_platform_timer_callback callback,
    void* sdk_data)
{
  _az_PRECONDITION_NOT_NULL(timer_handle);
  _az_PRECONDITION_NOT_NULL(callback);

  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
  memset(timer_handle, 0, sizeof(_az_platform_timer));

  timer_handle->_internal.platform_timer._internal.callback = callback;
  timer_handle->_internal.platform_timer._internal.sdk_data = sdk_data;

  timer_handle->_internal.sev.sigev_notify = SIGEV_THREAD;
  timer_handle->_internal.sev.sigev_notify_function = &_timer_callback_handler;
  timer_handle->_internal.sev.sigev_value.sival_ptr = timer_handle;

  if (0
      != timer_create(
          _az_PLATFORM_POSIX_CLOCK_ID, &timer_handle->_internal.sev, &timer_handle->_internal.timerid))
  {
    if (EAGAIN == errno)
      return AZ_ERROR_RESOURCE_UNAVAILABLE;
    else if (EINVAL == errno)
      return AZ_ERROR_ARG;
    else if (ENOMEM == errno)
      return AZ_ERROR_OUT_OF_MEMORY;
    else if (ENOTSUP == errno)
      return AZ_ERROR_NOT_SUPPORTED;
    else
      return AZ_ERROR_ARG;
  }

  return AZ_OK;
}

AZ_NODISCARD az_result
az_platform_timer_start(_az_platform_timer* timer_handle, int32_t milliseconds)
{
  _az_PRECONDITION_NOT_NULL(timer_handle);

  timer_handle->_internal.trigger.it_value.tv_sec = milliseconds / _az_TIME_MILLISECONDS_PER_SECOND;
  timer_handle->_internal.trigger.it_value.tv_nsec
      = (milliseconds % _az_TIME_MILLISECONDS_PER_SECOND) * _az_TIME_NANOSECONDS_PER_MILLISECOND;

  if (0
      != timer_settime(timer_handle->_internal.timerid, 0, &timer_handle->_internal.trigger, NULL))
  {
    if (EFAULT == errno)
      return AZ_ERROR_ARG;
    else if (EINVAL == errno)
      return AZ_ERROR_ARG;
    else
      return AZ_ERROR_ARG;
  }

  return AZ_OK;
}

AZ_NODISCARD az_result az_platform_timer_destroy(_az_platform_timer* timer_handle)
{
  _az_PRECONDITION_NOT_NULL(timer_handle);

  if (0 != timer_delete(timer_handle->_internal.timerid))
  {
    return AZ_ERROR_ARG;
  }

  return AZ_OK;
}

AZ_NODISCARD az_result az_platform_mutex_init(az_platform_mutex* mutex_handle)
{
  _az_PRECONDITION_NOT_NULL(mutex_handle);

  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

  int mutex_init_result = pthread_mutex_init(mutex_handle, &attr);

  if (EAGAIN == mutex_init_result)
    return AZ_ERROR_RESOURCE_UNAVAILABLE;
  else if (ENOMEM == mutex_init_result)
    return AZ_ERROR_OUT_OF_MEMORY;
  else if (EPERM == mutex_init_result)
    return AZ_ERROR_PERMISSION;
  else if (EBUSY == mutex_init_result)
    return AZ_ERROR_REINITIALIZATION;
  else if (EINVAL == mutex_init_result)
    return AZ_ERROR_ARG;
  else if (0 != mutex_init_result)
    return AZ_ERROR_ARG;

  return AZ_OK;
}

AZ_NODISCARD az_result az_platform_mutex_acquire(az_platform_mutex* mutex_handle)
{
  _az_PRECONDITION_NOT_NULL(mutex_handle);

  int mutex_lock_result = pthread_mutex_lock(mutex_handle);

  if (EINVAL == mutex_lock_result)
    return AZ_ERROR_ARG;
  else if (EBUSY == mutex_lock_result)
    return AZ_ERROR_MUTEX_BUSY;
  else if (EAGAIN == mutex_lock_result)
    return AZ_ERROR_MUTEX_MAX_RECURSIVE_LOCKS;
  else if (EDEADLK == mutex_lock_result)
    return AZ_ERROR_DEADLOCK;
  else if (0 != mutex_lock_result)
    return AZ_ERROR_ARG;

  return AZ_OK;
}

AZ_NODISCARD az_result az_platform_mutex_release(az_platform_mutex* mutex_handle)
{
  _az_PRECONDITION_NOT_NULL(mutex_handle);

  int mutex_unlock_result = pthread_mutex_unlock(mutex_handle);

  if (EINVAL == mutex_unlock_result)
    return AZ_ERROR_ARG;
  else if (EAGAIN == mutex_unlock_result)
    return AZ_ERROR_MUTEX_MAX_RECURSIVE_LOCKS;
  else if (EPERM == mutex_unlock_result)
    return AZ_ERROR_PERMISSION;
  else if (0 != mutex_unlock_result)
    return AZ_ERROR_ARG;

  return AZ_OK;
}

AZ_NODISCARD az_result az_platform_mutex_destroy(az_platform_mutex* mutex_handle)
{
  _az_PRECONDITION_NOT_NULL(mutex_handle);

  int mutex_destroy_result = pthread_mutex_destroy(mutex_handle);

  if (EBUSY == mutex_destroy_result)
    return AZ_ERROR_MUTEX_BUSY;
  else if (EINVAL == mutex_destroy_result)
    return AZ_ERROR_ARG;
  else if (0 != mutex_destroy_result)
    return AZ_ERROR_ARG;

  return AZ_OK;
}
