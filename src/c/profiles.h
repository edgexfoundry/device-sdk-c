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
  devsdk_service_t *svc,
  devsdk_error *err
);

extern const edgex_deviceprofile *edgex_deviceprofile_get_internal (devsdk_service_t *svc, const char *name, devsdk_error *err);

#endif
