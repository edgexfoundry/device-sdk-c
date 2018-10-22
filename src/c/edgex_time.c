/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <time.h>
#include "edgex_time.h"

uint64_t edgex_device_millitime()
{
  return (uint64_t)time (NULL) * EDGEX_MILLIS;
}
