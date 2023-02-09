/*
 * Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_REQUEST_AUTH_H_
#define _EDGEX_DEVICE_REQUEST_AUTH_H_ 1

#include "rest-server.h"

bool request_is_authenticated (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply);

#endif
