/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_REST_SERVER_H_
#define _EDGEX_REST_SERVER_H_ 1

#include "edgex/edgex.h"
#include "edgex/edgex-logging.h"
#include "edgex/error.h"

struct edgex_rest_server;
typedef struct edgex_rest_server edgex_rest_server;

typedef enum
{
  GET = 1,
  POST = 2,
  PUT = 4,
  PATCH = 8,
  DELETE = 16,
  UNKNOWN = 1024
} edgex_http_method;

typedef int (*http_method_handler_fn)
(
  void *context,
  char *url,
  char *querystr,
  edgex_http_method method,
  const char *upload_data,
  size_t upload_data_size,
  void **reply,
  size_t *reply_size,
  const char **reply_type
);

extern edgex_rest_server *edgex_rest_server_create
  (iot_logger_t *lc, uint16_t port, edgex_error *err);

extern void edgex_rest_server_register_handler
(
  edgex_rest_server *svr,
  const char *url,
  edgex_http_method method,
  void *context,
  http_method_handler_fn handler
);

extern void edgex_rest_server_destroy (edgex_rest_server *svr);

#endif
