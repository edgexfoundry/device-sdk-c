/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_DISCOVERY_H_
#define _EDGEX_DEVICE_DISCOVERY_H_ 1

#include "edgex/devsdk.h"

#include <stddef.h>

extern int edgex_device_handler_discovery
(
  void *ctx,
  char *url,
  edgex_http_method method,
  const char *upload_data,
  size_t upload_data_size,
  char **reply,
  const char **reply_type
);

void edgex_device_handler_do_discovery (void *);

#endif
