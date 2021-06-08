/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_REST_H_
#define _EDGEX_DEVICE_REST_H_ 1

#include "edgex/edgex-base.h"
#include "devsdk/devsdk-base.h"
#include "iot/logger.h"

typedef struct edgex_ctx
{
  char *cacerts_file;     // Location of CA certificates Curl will use to verify peer
  int verify_peer;        // enables TLS peer verification via Curl
  char *tls_cert;         // Location of PEM encoded X509 cert to use for TLS client auth
  char *tls_key;          // Location of PEM encoded priv key to use for TLS client auth
  char *jwt_token;        // access_token provided by server for authenticating REST calls
  devsdk_nvpairs *reqhdrs; // headers to be sent with request
  devsdk_nvpairs *rsphdrs; // headers to be retrieved from response
  atomic_bool *aborter;   // if non-null, can kill a request by setting to true
  char *buff;             // data returned from the request
  size_t size;            // current buffer size
} edgex_ctx;

#define URL_BUF_SIZE 512

/*
 * If this function is specified as the writefunc to a request, the returned data will be copied
 * to the buffer in the edgex_ctx
 */
size_t edgex_http_write_cb (void *contents, size_t size, size_t nmemb, void *userp);

/*
 * These functions use libcurl to send http get/post/put/delete requests.
 * TLS peer verification is enabled, but not HTTP authentication.
 * The POST and PUT functions assume JSON data and set the request Content-Type accordingly.
 * The common parameters are:
 *
 * lc: Logs will be written to this logging client.
 * ctx: Ptr to edgex_ctx, which contains various processing parameters as detailed above.
 * url: URL to use for the request.
 * writefunc: Function pointer to handle writing the data from the HTTP body received from the server.
 * err: Used to return error codes to the caller.
 *
 * The post and put functions take in addition a data parameter, this is the content that will be sent to the server.
 * edgex_http_postfile performs a post operation which uploads a file, this is specified by filename.
 *
 * Return value is the HTTP status value from the server (e.g. 200 for HTTP OK)
 */

long edgex_http_get
  (iot_logger_t *lc, edgex_ctx *ctx, const char *url, void *writefunc, devsdk_error *err);

long edgex_http_delete
  (iot_logger_t *lc, edgex_ctx *ctx, const char *url, void *writefunc, devsdk_error *err);

long edgex_http_post
  (iot_logger_t *lc, edgex_ctx *ctx, const char *url, const char *data, void *writefunc, devsdk_error *err);

long edgex_http_postbin
  (iot_logger_t *lc, edgex_ctx *ctx, const char *url, void *data, size_t length, const char *mime, void *writefunc, devsdk_error *err);

long edgex_http_postfile
  (iot_logger_t *lc, edgex_ctx *ctx, const char *url, const char *filename, void *writefunc, devsdk_error *err);

long edgex_http_put
  (iot_logger_t *lc, edgex_ctx *ctx, const char *url, const char *data, void *writefunc, devsdk_error *err);

long edgex_http_patch
  (iot_logger_t *lc, edgex_ctx *ctx, const char *url, const char *data, void *writefunc, devsdk_error *err);

#endif
