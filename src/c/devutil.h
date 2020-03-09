/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVUTIL_H
#define _EDGEX_DEVUTIL_H 1

#include "devsdk/devsdk-base.h"
#include "edgex/edgex.h"

struct devsdk_protocols
{
  char *name;
  devsdk_nvpairs *properties;
  struct devsdk_protocols *next;
};

extern bool devsdk_protocols_equal (const devsdk_protocols *p1, const devsdk_protocols *p2);

extern bool edgex_device_autoevents_equal (const edgex_device_autoevents *e1, const edgex_device_autoevents *e2);

#endif

