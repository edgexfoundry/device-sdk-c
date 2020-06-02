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

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  GET = 1,
  POST = 2,
  PUT = 4,
  PATCH = 8,
  DELETE = 16,
  UNKNOWN = 1024
} edgex_http_method;

#define CONTENT_JSON "application/json"
#define CONTENT_CBOR "application/cbor"
#define CONTENT_PLAINTEXT "text/plain"

/**
 * @brief Function called to handle an http request.
 * @param context The context data passed in when the handler was registered.
 * @param url The trailing part of the requested url, if present.
 * @param qparams Any parameters specified in the query string of the url.
 * @param method The HTTP method requested.
 * @param upload_data Any payload passed in the request. For REST this should be a JSON string.
 * @param upload_data_size The size of upload_data.
 * @param reply Should be set to a block of data to return. This will be free'd once delivered.
 * @param reply_size The size of the reply data.
 * @param reply_type The MIME type of the returned data. This will not be free'd.
 * @return The HTTP return code to pass back.
 */

typedef int (*edgex_http_handler_fn)
(
  void *context,
  char *url,
  const devsdk_nvpairs *qparams,
  edgex_http_method method,
  const char *upload_data,
  size_t upload_data_size,
  void **reply,
  size_t *reply_size,
  const char **reply_type
);

/**
 * @brief Register an http handler function.
 * @param svc The device service.
 * @param url The URL to handle. URLs ending in '/' are considered as a prefix to match against.
 * @param method The HTTP methods to accept, these should be logical-OR'd together.
 * @param context Context data to pass to the handler on each call.
 * @param handler The handler function to call when the URL is matched.
 * @param e Nonzero reason codes will be set here in the event of errors.
 */

extern void devsdk_register_http_handler
(
  devsdk_service_t *svc,
  const char *url,
  edgex_http_method method,
  void *context,
  edgex_http_handler_fn handler,
  devsdk_error *e
);

#ifdef __cplusplus
}
#endif

#endif
