/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <time.h>
#include "edgex-time.h"

uint64_t edgex_device_millitime()
{
  struct timespec ts;
  if (clock_gettime (CLOCK_REALTIME, &ts) == 0)
  {
    return ts.tv_sec * EDGEX_MILLIS + ts.tv_nsec / (EDGEX_NANOS / EDGEX_MILLIS);
  }
  else
  {
    return (uint64_t)time (NULL) * EDGEX_MILLIS;
  }
}
