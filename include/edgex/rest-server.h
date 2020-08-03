/*
 * Copyright (c) 2018-2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_REST_SERVER_H_
#define _EDGEX_REST_SERVER_H_ 1

#include "devsdk/devsdk-base.h"
#include "iot/logger.h"

#define CONTENT_JSON "application/json"
#define CONTENT_CBOR "application/cbor"
#define CONTENT_PLAINTEXT "text/plain"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  DevSDK_Get = 1,
  DevSDK_Post = 2,
  DevSDK_Put = 4,
  DevSDK_Patch = 8,
  DevSDK_Delete = 16,
  DevSDK_Unknown = 1024
} devsdk_http_method;

typedef struct
{
  void *bytes;
  size_t size;
} devsdk_http_data;

typedef struct
{
  const devsdk_nvpairs *params;
  devsdk_http_method method;
  devsdk_http_data data;
  const char *content_type;
} devsdk_http_request;

typedef struct
{
  int code;
  devsdk_http_data data;
  const char *content_type;
} devsdk_http_reply;

typedef void (*devsdk_http_handler_fn)
(
  void *context,
  const devsdk_http_request *req,
  devsdk_http_reply *reply
);

/**
 * @brief Register an http handler function.
 * @param svc The device service.
 * @param url The URL path to handle. Path elements of the form {xxx} are wildcard matches,
 *            and the handler will be given a parameter xxx=value containing the matched text.
 * @param method The HTTP methods to accept, these should be logical-OR'd together.
 * @param context Context data to pass to the handler on each call.
 * @param handler The handler function to call when the URL is matched.
 * @param e Nonzero reason codes will be set here in the event of errors.
 */

extern void devsdk_register_http_handler
(
  devsdk_service_t *svc,
  const char *url,
  devsdk_http_method method,
  void *context,
  devsdk_http_handler_fn handler,
  devsdk_error *e
);

#ifdef __cplusplus
}
#endif

#endif
