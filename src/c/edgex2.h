/*
 * Copyright (c) 2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_EDGEX2_H_
#define _EDGEX_EDGEX2_H_

#include "edgex/edgex-base.h"

/* Definitions of Data Transfer Objects for v2 REST API */

typedef struct
{
  char *requestId;
} edgex_baserequest;

typedef struct
{
  const char *requestId;
  uint64_t statusCode;
  const char *message;
} edgex_baseresponse;

typedef struct
{
  edgex_baseresponse base;
  long timestamp;
} edgex_pingresponse;

typedef struct
{
  edgex_baseresponse base;
  char *config;
} edgex_configresponse;

#endif
