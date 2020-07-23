/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_REST_SERVER_INT_H_
#define _EDGEX_REST_SERVER_INT_H_ 1

#include "edgex/rest-server.h"

struct edgex_rest_server;
typedef struct edgex_rest_server edgex_rest_server;

extern edgex_rest_server *edgex_rest_server_create
  (iot_logger_t *lc, const char *bindaddr, uint16_t port, devsdk_error *err);

extern bool edgex_rest_server_register_handler
(
  edgex_rest_server *svr,
  const char *url,
  edgex_http_method method,
  void *context,
  edgex_http_handler_fn handler
);

extern void edgex_rest_server_destroy (edgex_rest_server *svr);

#endif
