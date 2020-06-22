/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_CALLBACK_H_
#define _EDGEX_DEVICE_CALLBACK_H_ 1

#include "rest-server.h"

extern void edgex_device_handler_callback (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply);

#endif
