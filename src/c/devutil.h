/*
 * Copyright (c) 2019-2021
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
  iot_data_t *properties;
  struct devsdk_protocols *next;
};

extern bool devsdk_protocols_equal (const devsdk_protocols *p1, const devsdk_protocols *p2);

extern bool edgex_device_autoevents_equal (const edgex_device_autoevents *e1, const edgex_device_autoevents *e2);

extern void devsdk_free_resources (devsdk_device_resources *r);

extern uint64_t edgex_parsetime (const char *spec);

#endif
