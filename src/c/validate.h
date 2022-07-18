/*
 * Copyright (c) 2022
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_VALIDATE_H_
#define _EDGEX_DEVICE_VALIDATE_H_ 1

#include "rest-server.h"

extern void edgex_device_handler_validate_addr (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply);

#endif
