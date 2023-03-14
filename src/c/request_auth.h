/*
 * Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_REQUEST_AUTH_H_
#define _EDGEX_DEVICE_REQUEST_AUTH_H_ 1

#include "rest-server.h"
#include "secrets.h"

typedef struct auth_wrapper_t
{
  void*                    h_ctx;
  edgex_secret_provider_t* secretprovider;
  devsdk_http_handler_fn   h;
} auth_wrapper_t;

// http_auth_wrapper expects ctx is &auth_wrapper_t
void http_auth_wrapper (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply);

// request_is_authenticated with authenticate requst with the secret provider
bool request_is_authenticated (edgex_secret_provider_t * secretprovider, const devsdk_http_request *req, devsdk_http_reply *reply);

#endif
