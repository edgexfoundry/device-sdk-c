/*
 * Copyright (c) 2018, 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <time.h>
#include <stdatomic.h>

#include "edgex-time.h"

uint64_t edgex_device_nanotime (void)
{
  struct timespec ts;
  if (clock_gettime (CLOCK_REALTIME, &ts) == 0)
  {
    return (uint64_t)ts.tv_sec * EDGEX_NANOS + ts.tv_nsec;
  }
  else
  {
    return (uint64_t)time (NULL) * EDGEX_NANOS;
  }
}

uint64_t edgex_device_millitime ()
{
  return edgex_device_nanotime () / (EDGEX_NANOS / EDGEX_MILLIS);
}

uint64_t edgex_device_nanotime_monotonic()
{
  static _Atomic(uint64_t) lasttime = 0;
  uint64_t prev;
  uint64_t result = edgex_device_nanotime ();
  prev = lasttime;
  do
  {
    if (result <= prev)
    {
      result = prev + 1;
    }
  } while (!atomic_compare_exchange_weak (&lasttime, &prev, result));
  return result;
}
