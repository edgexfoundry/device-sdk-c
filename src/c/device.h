/*
 * Copyright (c) 2018-2023
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

extern void edgex_device_handler_device_namev2 (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply);

extern int32_t edgex_device_handler_devicev3 (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply);

extern const struct edgex_cmdinfo *edgex_deviceprofile_findcommand
  (devsdk_service_t *svc, const char *name, edgex_deviceprofile *prof, bool forGet);

#endif
