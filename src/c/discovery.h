/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_DISCOVERY_H_
#define _EDGEX_DEVICE_DISCOVERY_H_ 1

#include "rest-server.h"

#include <stddef.h>

extern int edgex_device_handler_discovery
(
  void *ctx,
  char *url,
  const devsdk_nvpairs *qparams,
  edgex_http_method method,
  const char *upload_data,
  size_t upload_data_size,
  void **reply,
  size_t *reply_size,
  const char **reply_type
);

extern void *edgex_device_periodic_discovery (void *svc);

#endif
