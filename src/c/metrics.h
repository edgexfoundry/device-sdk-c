/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_METRICS_H_
#define _EDGEX_DEVICE_METRICS_H_ 1

#include "rest-server.h"

extern void edgex_device_handler_metricsv2 (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply);

#endif
