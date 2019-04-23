/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_AUTOEVENT_H
#define _EDGEX_DEVICE_AUTOEVENT_H 1

#include "parson.h"
#include "cmdinfo.h"

typedef struct edgex_autoimpl
{
  struct json_value_t *last;
  uint64_t interval;
  edgex_cmdinfo *resource;
} edgex_autoimpl;

#endif
