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

extern void edgex_device_handler_discovery (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply);

extern void *edgex_device_periodic_discovery (void *svc);

#endif
