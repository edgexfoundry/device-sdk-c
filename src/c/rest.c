/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

/* Based on code from https://github.com/cisco/libacvp */

/* Copyright (c) 2016, Cisco Systems, Inc.
   All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <curl/curl.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "errorlist.h"
#include "rest.h"

#if (LIBCURL_VERSION_NUM >= 0x073800)
#define USE_CURL_MIME
#endif

#define HTTP_UNAUTH 401

#define MAX_TOKEN_LEN 600

static struct curl_slist *edgex_add_auth_hdr
  (iot_logging_client *lc, edgex_ctx *ctx, struct curl_slist *slist)
{
  int bearer_size;
  char *bearer;

  /*
   * Create the Authorization header if needed
   */
  if (ctx->jwt_token)
  {
    bearer_size = strnlen (ctx->jwt_token, MAX_TOKEN_LEN) + 23;
    bearer = calloc (1, bearer_size);
    if (!bearer)
    {
      iot_log_error (lc, "unable to allocate memory.");
      return slist;
    }
    snprintf
      (bearer, bearer_size + 1, "Authorization: Bearer %s", ctx->jwt_token);
    slist = curl_slist_append (slist, bearer);
    free (bearer);
  }
  return slist;
}


static void edgex_log_peer_cert
  (iot_logging_client *lc, edgex_ctx *ctx, CURL *hnd)
{
  int rv;
  union
  {
    struct curl_slist *to_info;
    struct curl_certinfo *to_certinfo;
  } ptr;
  int i;
  struct curl_slist *slist;

  ptr.to_info = NULL;

  rv = curl_easy_getinfo(hnd, CURLINFO_CERTINFO, &ptr.to_info);

  if (!rv && ptr.to_info)
  {
    iot_log_info (lc, "TLS peer presented the following %d certificates...",
                  ptr.to_certinfo->num_of_certs);
    for (i = 0; i < ptr.to_certinfo->num_of_certs; i++)
    {
      for (slist = ptr.to_certinfo->certinfo[i]; slist; slist = slist->next)
      {
        iot_log_info (lc, "%s", slist->data);
      }
    }
  }
}

/*
 * This function uses libcurl to send a simple HTTP GET
 * request with no Content-Type header.
 * TLS peer verification is enabled, but not HTTP authentication.
 * The parameters are:
 *
 * ctx: Ptr to edgex_ctx, which contains the server name
 * url: URL to use for the GET request
 * writefunc: Function pointer to handle writing the data
 *            from the HTTP body received from the server.
 *
 * Return value is the HTTP status value from the server
 *	    (e.g. 200 for HTTP OK)
 */
long edgex_http_get (iot_logging_client *lc, edgex_ctx *ctx, const char *url,
                     void *writefunc, edgex_error *err)
{
  long http_code = 0;
  CURL *hnd;
  struct curl_slist *slist;
  CURLcode rc;

  slist = NULL;
  /*
   * Create the Authorization header if needed
   */
  slist = edgex_add_auth_hdr (lc, ctx, slist);

  ctx->buff = malloc (1);
  ctx->size = 0;

  /*
   * Setup Curl
   */
  hnd = curl_easy_init ();
  curl_easy_setopt(hnd, CURLOPT_URL, url);
  curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(hnd, CURLOPT_USERAGENT, "edgex");
  if (slist)
  {
    curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist);
  }
  if (ctx->verify_peer && ctx->cacerts_file)
  {
    curl_easy_setopt(hnd, CURLOPT_CAINFO, ctx->cacerts_file);
    curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(hnd, CURLOPT_CERTINFO, 1L);
  }
  else
  {
    curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 0L);
  }
  curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
  if (ctx->tls_cert && ctx->tls_key)
  {
    curl_easy_setopt(hnd, CURLOPT_SSLCERTTYPE, "PEM");
    curl_easy_setopt(hnd, CURLOPT_SSLCERT, ctx->tls_cert);
    curl_easy_setopt(hnd, CURLOPT_SSLKEYTYPE, "PEM");
    curl_easy_setopt(hnd, CURLOPT_SSLKEY, ctx->tls_key);
  }
  /*
   * If the caller wants the HTTP data from the server
   * set the callback function
   */
  if (writefunc)
  {
    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, ctx);
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, writefunc);
  }

  /*
   * Send the HTTP GET request
   */
  rc = curl_easy_perform (hnd);
  if (rc != CURLE_OK)
  {
    iot_log_error (lc, "curl_easy_perform returned: %d\n", (int) rc);
    *err = EDGEX_HTTP_GET_ERROR;
    return 0;
  }

  /*
   * Get the cert info from the TLS peer
   */
  if (ctx->verify_peer)
  {
    edgex_log_peer_cert (lc, ctx, hnd);
  }

  /*
   * Get the HTTP reponse status code from the server
   */
  curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &http_code);

  if (http_code < 200 || http_code >= 300)
  {
    iot_log_info (lc, "HTTP response: %d\n", (int) http_code);
    *err = EDGEX_HTTP_GET_ERROR;
  }
  else
  {
    *err = EDGEX_OK;
  }

  curl_easy_cleanup (hnd);
  hnd = NULL;
  if (slist)
  {
    curl_slist_free_all (slist);
    slist = NULL;
  }

  return http_code;
}

/*
 * This function uses libcurl to send a simple HTTP POST
 * request with JSON data.
 * TLS peer verification is enabled, but not HTTP authentication.
 * The parameters are:
 *
 * ctx: Ptr to edgex_ctx, which contains the server name
 * url: URL to use for the GET request
 * data: data to POST to the server
 * writefunc: Function pointer to handle writing the data
 *            from the HTTP body received from the server.
 *
 * Return value is the HTTP status value from the server
 *	    (e.g. 200 for HTTP OK)
 */
long edgex_http_post
(
  iot_logging_client *lc,
  edgex_ctx *ctx,
  const char *url,
  const char *data,
  void *writefunc,
  edgex_error *err
)
{
  long http_code = 0;
  CURL *hnd;
  CURLcode crv;
  struct curl_slist *slist;

  /*
   * Set the Content-Type header in the HTTP request
   */
  slist = NULL;
  slist = curl_slist_append (slist, "Content-Type:application/json");

  /*
   * Create the Authorization header if needed
   */
  slist = edgex_add_auth_hdr (lc, ctx, slist);

  ctx->buff = malloc (1);
  ctx->size = 0;

  /*
   * Setup Curl
   */
  hnd = curl_easy_init ();
  curl_easy_setopt(hnd, CURLOPT_URL, url);
  curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(hnd, CURLOPT_USERAGENT, "edgex");
  curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist);
  curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
  curl_easy_setopt(hnd, CURLOPT_POST, 1L);
  curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, data);
  curl_easy_setopt
    (hnd, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t) strlen (data));
  //FIXME: we should always to TLS peer auth
  if (ctx->verify_peer && ctx->cacerts_file)
  {
    curl_easy_setopt(hnd, CURLOPT_CAINFO, ctx->cacerts_file);
    curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(hnd, CURLOPT_CERTINFO, 1L);
  }
  else
  {
    curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 0L);
  }
  curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
  if (ctx->tls_cert && ctx->tls_key)
  {
    curl_easy_setopt(hnd, CURLOPT_SSLCERTTYPE, "PEM");
    curl_easy_setopt(hnd, CURLOPT_SSLCERT, ctx->tls_cert);
    curl_easy_setopt(hnd, CURLOPT_SSLKEYTYPE, "PEM");
    curl_easy_setopt(hnd, CURLOPT_SSLKEY, ctx->tls_key);
  }

  /*
   * If the caller wants the HTTP data from the server
   * set the callback function
   */
  if (writefunc)
  {
    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, ctx);
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, writefunc);
  }

  /*
   * Send the HTTP POST request
   */
  crv = curl_easy_perform (hnd);
  if (crv != CURLE_OK)
  {
    iot_log_error
      (lc, "Curl failed with code %d (%s)\n", crv, curl_easy_strerror (crv));
    *err = EDGEX_HTTP_POST_ERROR;
    return 0;
  }

  /*
   * Get the cert info from the TLS peer
   */
  if (ctx->verify_peer)
  {
    edgex_log_peer_cert (lc, ctx, hnd);
  }

  /*
   * Get the HTTP reponse status code from the server
   */
  curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &http_code);

  if (http_code < 200 || http_code >= 300)
  {
    iot_log_error (lc, "HTTP response: %d\n", (int) http_code);
    *err = EDGEX_HTTP_POST_ERROR;
  }
  else
  {
    *err = EDGEX_OK;
  }

  curl_easy_cleanup (hnd);
  hnd = NULL;
  curl_slist_free_all (slist);
  slist = NULL;

  return http_code;
}

/*
 * This function uses libcurl to send a HTTP POST
 * request with a form-based file upload
 * TLS peer verification is enabled, but not HTTP authentication.
 * The parameters are:
 *
 * ctx: Ptr to edgex_ctx, which contains the server name
 * url: URL to use for the GET request
 * fname: name of file to POST to the server
 * writefunc: Function pointer to handle writing the data
 *            from the HTTP body received from the server.
 *
 * Return value is the HTTP status value from the server
 *	    (e.g. 200 for HTTP OK)
 */
long
edgex_http_postfile (iot_logging_client *lc, edgex_ctx *ctx, const char *url,
                     const char *fname, void *writefunc, edgex_error *err)
{
  long http_code = 0;
  CURL *hnd;
  CURLcode crv;
  struct curl_slist *slist;
#ifdef USE_CURL_MIME
  curl_mime *form = NULL;
  curl_mimepart *field = NULL;
#else
  struct curl_httppost *form = NULL;
  struct curl_httppost *lastptr = NULL;
#endif

  /*
   * Create the Authorization header if needed
   */
  slist = NULL;
  slist = edgex_add_auth_hdr (lc, ctx, slist);

  ctx->buff = malloc (1);
  ctx->size = 0;

  /*
   * Setup Curl
   */
  hnd = curl_easy_init ();

#ifdef USE_CURL_MIME
  form = curl_mime_init (hnd);
  field = curl_mime_addpart (form);
  curl_mime_name (field, "file");
  curl_mime_filedata (field, fname);
  field = curl_mime_addpart (form);
  curl_mime_name (field, "filename");
  curl_mime_data (field, fname, CURL_ZERO_TERMINATED);
  field = curl_mime_addpart (form);
  curl_mime_name (field, "submit");
  curl_mime_data (field, "send", CURL_ZERO_TERMINATED);
  curl_easy_setopt(hnd, CURLOPT_MIMEPOST, form);
#else
  curl_formadd
  (
    &form, &lastptr,
    CURLFORM_COPYNAME, "file", CURLFORM_FILE, fname, CURLFORM_END
  );
  curl_formadd
  (
    &form, &lastptr,
    CURLFORM_COPYNAME, "filename", CURLFORM_COPYCONTENTS, fname, CURLFORM_END
  );
  curl_formadd
  (
    &form, &lastptr,
    CURLFORM_COPYNAME, "submit", CURLFORM_COPYCONTENTS, "send", CURLFORM_END
  );
  curl_easy_setopt(hnd, CURLOPT_HTTPPOST, form);
#endif

  curl_easy_setopt(hnd, CURLOPT_URL, url);
  curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(hnd, CURLOPT_USERAGENT, "edgex");
  curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist);
  //FIXME: we should always to TLS peer auth
  if (ctx->verify_peer && ctx->cacerts_file)
  {
    curl_easy_setopt(hnd, CURLOPT_CAINFO, ctx->cacerts_file);
    curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(hnd, CURLOPT_CERTINFO, 1L);
  }
  else
  {
    curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 0L);
  }
  curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
  if (ctx->tls_cert && ctx->tls_key)
  {
    curl_easy_setopt(hnd, CURLOPT_SSLCERTTYPE, "PEM");
    curl_easy_setopt(hnd, CURLOPT_SSLCERT, ctx->tls_cert);
    curl_easy_setopt(hnd, CURLOPT_SSLKEYTYPE, "PEM");
    curl_easy_setopt(hnd, CURLOPT_SSLKEY, ctx->tls_key);
  }

  /*
   * If the caller wants the HTTP data from the server
   * set the callback function
   */
  if (writefunc)
  {
    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, ctx);
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, writefunc);
  }

  /*
   * Send the HTTP POST request
   */
  crv = curl_easy_perform (hnd);
  if (crv != CURLE_OK)
  {
    iot_log_error
      (lc, "Curl failed with code %d (%s)\n", crv, curl_easy_strerror (crv));
    *err = EDGEX_HTTP_POSTFILE_ERROR;
    http_code = 0;
  }
  else
  {
    /*
     * Get the cert info from the TLS peer
     */
    if (ctx->verify_peer)
    {
      edgex_log_peer_cert (lc, ctx, hnd);
    }

    /*
     * Get the HTTP reponse status code from the server
     */
    curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code < 200 || http_code >= 300)
    {
      iot_log_error (lc, "HTTP response: %d\n", (int) http_code);
      *err = EDGEX_HTTP_POSTFILE_ERROR;
    }
    else
    {
      *err = EDGEX_OK;
    }
  }

  curl_easy_cleanup (hnd);
  hnd = NULL;
#ifdef USE_CURL_MIME
  curl_mime_free (form);
#else
  curl_formfree(form);
#endif
  curl_slist_free_all (slist);
  slist = NULL;

  return http_code;
}

struct put_data
{
  const char *data;
  size_t offset;
  size_t remaining;
};

static size_t read_callback
  (char *buffer, size_t size, size_t nitems, void *userdata)
{
  struct put_data *pd = (struct put_data *) userdata;
  size_t max_size = size * nitems;
  size_t transferred = pd->remaining < max_size ? pd->remaining : max_size;

  if (transferred > 0)
  {
    memcpy (buffer, pd->data + pd->offset, transferred);
    pd->offset += transferred;
    pd->remaining -= transferred;
  }

  return transferred;
}

long edgex_http_put
(
  iot_logging_client *lc,
  edgex_ctx *ctx,
  const char *url,
  const char *data,
  void *writefunc,
  edgex_error *err
)
{
  long http_code = 0;
  CURL *hnd;
  CURLcode crv;
  struct curl_slist *slist;
  struct put_data cb_data;

  /*
   * Set the Content-Type header in the HTTP request
   */
  slist = NULL;
  slist = curl_slist_append (slist, "Content-Type:application/json");

  /*
   * Create the Authorization header if needed
   */
  slist = edgex_add_auth_hdr (lc, ctx, slist);

  ctx->buff = malloc (1);
  ctx->size = 0;

  /*
   * Setup Curl
   */
  hnd = curl_easy_init ();
  curl_easy_setopt(hnd, CURLOPT_URL, url);
  curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(hnd, CURLOPT_USERAGENT, "edgex");
  curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist);
  curl_easy_setopt(hnd, CURLOPT_UPLOAD, 1L);

  if (data)
  {
    cb_data.data = data;
    cb_data.offset = 0;
    cb_data.remaining = strlen (data);
    curl_easy_setopt(hnd, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(hnd, CURLOPT_READDATA, &cb_data);
    curl_easy_setopt(hnd, CURLOPT_INFILESIZE, cb_data.remaining);
  }

  //FIXME: we should always to TLS peer auth
  if (ctx->verify_peer && ctx->cacerts_file)
  {
    curl_easy_setopt(hnd, CURLOPT_CAINFO, ctx->cacerts_file);
    curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(hnd, CURLOPT_CERTINFO, 1L);
  }
  else
  {
    curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 0L);
  }
  curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
  if (ctx->tls_cert && ctx->tls_key)
  {
    curl_easy_setopt(hnd, CURLOPT_SSLCERTTYPE, "PEM");
    curl_easy_setopt(hnd, CURLOPT_SSLCERT, ctx->tls_cert);
    curl_easy_setopt(hnd, CURLOPT_SSLKEYTYPE, "PEM");
    curl_easy_setopt(hnd, CURLOPT_SSLKEY, ctx->tls_key);
  }

  /*
   * If the caller wants the HTTP data from the server
   * set the callback function
   */
  if (writefunc)
  {
    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, ctx);
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, writefunc);
  }

  /*
   * Send the HTTP PUT request
   */
  crv = curl_easy_perform (hnd);
  if (crv != CURLE_OK)
  {
    iot_log_error (lc, "Curl failed with code %d (%s)\n", crv,
                   curl_easy_strerror (crv));
    *err = EDGEX_HTTP_PUT_ERROR;
    return 0;
  }

  /*
   * Get the cert info from the TLS peer
   */
  if (ctx->verify_peer)
  {
    edgex_log_peer_cert (lc, ctx, hnd);
  }

  /*
   * Get the HTTP reponse status code from the server
   */
  curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &http_code);

  if (http_code < 200 || http_code >= 300)
  {
    iot_log_error (lc, "HTTP response: %d\n", (int) http_code);
    *err = EDGEX_HTTP_PUT_ERROR;
  }
  else
  {
    *err = EDGEX_OK;
  }

  curl_easy_cleanup (hnd);
  hnd = NULL;
  curl_slist_free_all (slist);
  slist = NULL;

  return http_code;
}
