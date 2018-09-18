/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_PROFILES_H_
#define _EDGEX_DEVICE_PROFILES_H_ 1

#include "service.h"

extern void edgex_device_profiles_upload
(
  iot_logging_client *lc,
  const char *confDir,
  edgex_service_endpoints *endpoints
);

edgex_deviceprofile *edgex_deviceprofile_get
(
  edgex_device_service *svc,
  const char *name,
  edgex_error *err
);

#endif
