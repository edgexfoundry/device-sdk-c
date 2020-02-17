/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_DEVICE_H_
#define _EDGEX_DEVICE_DEVICE_H_ 1

#include "devsdk/devsdk-base.h"
#include "edgex/edgex.h"
#include "rest-server.h"
#include "cmdinfo.h"
#include "data.h"

extern int edgex_device_handler_device
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

extern char *edgex_value_tostring (const iot_data_t *value, bool binfloat);

extern const struct edgex_cmdinfo *edgex_deviceprofile_findcommand
  (const char *name, edgex_deviceprofile *prof, bool forGet);

#endif
