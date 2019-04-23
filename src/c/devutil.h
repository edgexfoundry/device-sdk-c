/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVUTIL_H
#define _EDGEX_DEVUTIL_H 1

#include "edgex/edgex.h"

extern bool edgex_protocols_equal
  (const edgex_protocols *p1, const edgex_protocols *p2);

extern bool edgex_device_autoevents_equal
  (const edgex_device_autoevents *e1, const edgex_device_autoevents *e2);

#endif

