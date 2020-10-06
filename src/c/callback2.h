/*
 * Copyright (c) 2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_CALLBACK2_H_
#define _EDGEX_DEVICE_CALLBACK2_H_ 1

#include "rest-server.h"

extern void edgex_device_handler_callback_device (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply);
extern void edgex_device_handler_callback_device_id (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply);

extern void edgex_device_handler_callback_profile (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply);
extern void edgex_device_handler_callback_profile_id (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply);

extern void edgex_device_handler_callback_watcher (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply);
extern void edgex_device_handler_callback_watcher_id (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply);

#endif
