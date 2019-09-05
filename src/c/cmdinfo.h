/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EGDEX_CMDINFO_H_
#define _EGDEX_CMDINFO_H_ 1

#include "edgex/edgex.h"
#include "edgex/devsdk.h"

typedef struct edgex_cmdinfo
{
  char *name;
  bool isget;
  unsigned nreqs;
  edgex_device_commandrequest *reqs;
  edgex_propertyvalue **pvals;
  edgex_nvpairs **maps;
  char **dfls;
  struct edgex_cmdinfo *next;
} edgex_cmdinfo;

#endif
