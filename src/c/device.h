/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_DEVICE_H_
#define _EDGEX_DEVICE_DEVICE_H_ 1

#include "edgex/devsdk.h"

extern int edgex_device_handler_device
(
  void *ctx,
  char *url,
  edgex_http_method method,
  const char *upload_data,
  size_t upload_data_size,
  char **reply,
  const char **reply_type
);

extern char *edgex_value_tostring
(
  edgex_device_resultvalue value,
  bool xform,
  edgex_propertyvalue *props,
  edgex_nvpairs *mappings
);

#endif
