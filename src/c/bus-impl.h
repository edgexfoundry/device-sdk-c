/*
 * Copyright (c) 2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_BUS_IMPL_H_
#define _EDGEX_BUS_IMPL_H_ 1

#include <iot/data.h>
#include <pthread.h>

typedef void (*edgex_bus_freefn) (void *ctx);
typedef void (*edgex_bus_postfn) (void *ctx, const char *path, const iot_data_t *envelope);
typedef void (*edgex_bus_subsfn) (void *ctx, const char *path);

struct edgex_bus_t
{
  void *ctx;
  edgex_bus_postfn postfn;
  edgex_bus_subsfn subsfn;
  edgex_bus_freefn freefn;
  iot_data_t *handlers;
  char *prefix;
  char *svcname;
  pthread_mutex_t mtx;
  bool msgb64payload;
};

void edgex_bus_init (edgex_bus_t *bus, const char *svcname, const iot_data_t *cfg);
void edgex_bus_handle_request (edgex_bus_t *bus, const char *path, const char *envelope);

#endif
