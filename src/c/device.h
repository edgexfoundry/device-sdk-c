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
#include "edgex/edgex.h"
#include "rest_server.h"
#include "parson.h"
#include "cmdinfo.h"

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

extern char *edgex_value_tostring (const edgex_device_commandresult *value, bool binfloat);

extern const struct edgex_cmdinfo *edgex_deviceprofile_findcommand
  (const char *name, edgex_deviceprofile *prof, bool forGet);

extern int edgex_device_runget
(
  edgex_device_service *svc,
  edgex_device *dev,
  const edgex_cmdinfo *commandinfo,
  const JSON_Value *lastval,
  JSON_Value **reply
);

#endif
