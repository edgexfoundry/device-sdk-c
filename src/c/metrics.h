/*
 * Copyright (c) 2019-2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_METRICS_H_
#define _EDGEX_DEVICE_METRICS_H_ 1

#include <stdatomic.h>

typedef struct devsdk_metrics_t
{
  atomic_uint_fast64_t esent;
  atomic_uint_fast64_t rsent;
  atomic_uint_fast64_t rcexe;
  atomic_uint_fast64_t secrq;
  atomic_uint_fast64_t secsto;
} devsdk_metrics_t;

#endif
