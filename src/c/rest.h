/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_REST_H_
#define _EDGEX_DEVICE_REST_H_ 1

#include "edgex/edgex_logging.h"
#include "edgex/error.h"

typedef struct edgex_ctx
{
  char *cacerts_file;   // Location of CA certificates Curl will use to verify peer
  int verify_peer;      // enables TLS peer verification via Curl
  char *tls_cert;       // Location of PEM encoded X509 cert to use for TLS client auth
  char *tls_key;        // Location of PEM encoded priv key to use for TLS client auth
  char *jwt_token;      // access_token provided by server for authenticating REST calls
  char *buff;           // used during curl processing
  size_t size;
} edgex_ctx;

long edgex_http_get
(
  iot_logging_client *lc,
  edgex_ctx *ctx,
  const char *url,
  void *writefunc,
  edgex_error *err
);
long edgex_http_post
(
  iot_logging_client *lc,
  edgex_ctx *ctx,
  const char *url,
  const char *data,
  void *writefunc,
  edgex_error *err
);
long edgex_http_postfile
(
  iot_logging_client *lc,
  edgex_ctx *ctx,
  const char *url,
  const char *fname,
  void *writefunc,
  edgex_error *err
);
long edgex_http_put
(
  iot_logging_client *lc,
  edgex_ctx *ctx,
  const char *url,
  const char *data,
  void *writefunc,
  edgex_error *err
);

#endif
