/*
 * Copyright (c) 2019-2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EGDEX_CMDINFO_H_
#define _EGDEX_CMDINFO_H_ 1

#include "edgex/edgex.h"
#include "devsdk/devsdk.h"

typedef struct edgex_cmdinfo
{
  char *name;
  edgex_deviceprofile *profile;
  bool isget;
  unsigned nreqs;
  devsdk_commandrequest *reqs;
  edgex_propertyvalue **pvals;
  iot_data_t **maps;
  iot_data_t *tags;
  char **dfls;
  struct edgex_cmdinfo *next;
} edgex_cmdinfo;

#endif
