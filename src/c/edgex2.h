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
#include "parson.h"

/* Definitions of Data Transfer Objects for v2 REST API */

typedef struct
{
  char *requestId;
} edgex_baserequest;

typedef struct
{
  const char *apiVersion;
  const char *requestId;
  uint64_t statusCode;
  const char *message;
} edgex_baseresponse;

typedef edgex_baseresponse edgex_errorresponse;

typedef struct
{
  edgex_baseresponse base;
  long timestamp;
} edgex_pingresponse;

typedef struct
{
  edgex_baseresponse base;
  JSON_Value *config;
} edgex_configresponse;

typedef struct
{
  edgex_baseresponse base;
  int alloc;
  int totalloc;
  double loadavg;
  double cputime;
  double cpuavg;
} edgex_metricsresponse;

#endif
