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
#include "correlation.h"
#include "rest.h"

#if (LIBCURL_VERSION_NUM >= 0x073800)
#define USE_CURL_MIME
#endif

#define MAX_TOKEN_LEN 600
#define EDGEX_AUTH_HDR "Authorization: Bearer "

/* Add a request header to the list */

static struct curl_slist *edgex_add_hdr (struct curl_slist *slist, const char *name, const char *value)
{
  char *bearer = malloc (strlen (name) + strlen (value) + 3);
  strcpy (bearer, name);
  strcat (bearer, ": ");
  strcat (bearer, value);
  slist = curl_slist_append (slist, bearer);
  free (bearer);
  return slist;
}

/* Add a header for correlation ID if we have one */

static struct curl_slist *edgex_add_crlid_hdr (struct curl_slist *slist)
{
  const char *id = edgex_device_get_crlid ();
  return id ? edgex_add_hdr (slist, EDGEX_CRLID_HDR, id) : slist;
}

/* Populate request headers from a list */

static struct curl_slist *edgex_add_other_hdrs (struct curl_slist *slist, const devsdk_nvpairs *hdrs)
{
  struct curl_slist *s = slist;
  for (const devsdk_nvpairs *h = hdrs; h; h = h->next)
  {
    s = edgex_add_hdr (s, h->name, h->value);
  }
  return s;
}

/* Create the Authorization header if needed */

static struct curl_slist *edgex_add_auth_hdr (edgex_ctx *ctx, struct curl_slist *slist)
{
  if (ctx->jwt_token)
  {
    char *bearer = malloc (sizeof (EDGEX_AUTH_HDR) + strnlen (ctx->jwt_token, MAX_TOKEN_LEN));
    strcpy (bearer, EDGEX_AUTH_HDR);
    strncat (bearer, ctx->jwt_token, MAX_TOKEN_LEN);
    slist = curl_slist_append (slist, bearer);
    free (bearer);
  }
  return slist;
}

/* Callback: inspect a returned header to see if it matches any that we want */

static size_t edgex_check_rsp_hdr (char *buffer, size_t size, size_t nitems, void *v)
{
  char *end = buffer + size * nitems - 1;
  for (devsdk_nvpairs *i = (devsdk_nvpairs *)v; i; i = i->next)
  {
    int len = strlen (i->name);
    if ((end - buffer > len) && (strncmp (buffer, i->name, len) == 0) && (buffer[len] == ':'))
    {
      char *start = buffer + len + 1;
      while (start < end && *start == ' ') start++;
      while (end > start && (*end == '\n' || *end == ' ' || *end == '\r' )) end--;
      i->value = strndup (start, end - start + 1);
    }
  }
  return size * nitems;
}

/* Inspect returned certificates and log their contents */

static void edgex_log_peer_cert (iot_logger_t *lc, edgex_ctx *ctx, CURL *hnd)
{
  struct curl_slist *slist;
  struct curl_certinfo *certs = NULL;

  int rv = curl_easy_getinfo (hnd, CURLINFO_CERTINFO, &certs);

  if (!rv && certs)
  {
    iot_log_info (lc, "TLS peer presented the following %d certificates...", certs->num_of_certs);
    for (int i = 0; i < certs->num_of_certs; i++)
    {
      for (slist = certs->certinfo[i]; slist; slist = slist->next)
      {
        iot_log_info (lc, "%s", slist->data);
      }
    }
  }
}

/* Append a chunk of returned data to the buffer */

size_t edgex_http_write_cb (void *contents, size_t size, size_t nmemb, void *userp)
{
  edgex_ctx *ctx = (edgex_ctx *) userp;
  size *= nmemb;
  ctx->buff = realloc (ctx->buff, ctx->size + size + 1);
  memcpy (&(ctx->buff[ctx->size]), contents, size);
  ctx->size += size;
  ctx->buff[ctx->size] = 0;

  return size;
}

/* Discard incoming data */

static size_t edgex_discarder (void *contents, size_t size, size_t nmemb, void *userp)
{
  return size * nmemb;
}

/* Kill an ongoing request if our flag becomes set */

static int abort_callback (void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
  atomic_bool *flag = (atomic_bool *)clientp;
  return (*flag) ? 1 : 0;
}

/*
 * Set up common curl options and headers, perform the http request, process the results and clean up.
 * Additional options may be set by calling curl_easy_setopt before this.
 * Extra headers may be added by passing non-null slist_in.
 */

static long edgex_run_curl
(
  iot_logger_t *lc,
  edgex_ctx *ctx,
  CURL *hnd,
  const char *url,
  void *writefunc,
  struct curl_slist *slist_in,
  devsdk_error *err
)
{
  struct curl_slist *slist;
  CURLcode rc;
  long http_code = 0;

  /* Init buffer */
  ctx->buff = NULL;
  ctx->size = 0;

  /* Setup Curl */
  curl_easy_setopt(hnd, CURLOPT_URL, url);
  curl_easy_setopt(hnd, CURLOPT_USERAGENT, "edgex");
  curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);

  /* TLS options */
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
  if (ctx->tls_cert && ctx->tls_key)
  {
    curl_easy_setopt(hnd, CURLOPT_SSLCERTTYPE, "PEM");
    curl_easy_setopt(hnd, CURLOPT_SSLCERT, ctx->tls_cert);
    curl_easy_setopt(hnd, CURLOPT_SSLKEYTYPE, "PEM");
    curl_easy_setopt(hnd, CURLOPT_SSLKEY, ctx->tls_key);
  }

  /* Setup for response header processing */
  if (ctx->rsphdrs)
  {
    curl_easy_setopt(hnd, CURLOPT_HEADERFUNCTION, edgex_check_rsp_hdr);
    curl_easy_setopt(hnd, CURLOPT_HEADERDATA, (void *)ctx->rsphdrs);
  }

  /* Use the progress callback to abort a request on demand */
  if (ctx->aborter)
  {
    curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(hnd, CURLOPT_XFERINFOFUNCTION, abort_callback);
    curl_easy_setopt(hnd, CURLOPT_XFERINFODATA, (void *)ctx->aborter);
  }
  else
  {
    curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
  }

  /* If the caller wants the HTTP data from the server set the callback function */
  if (writefunc)
  {
    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, (void *)ctx);
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, writefunc);
  }
  else
  {
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, edgex_discarder);
  }

  /* Setup header list */
  slist = edgex_add_auth_hdr (ctx, slist_in);
  slist = edgex_add_crlid_hdr (slist);
  slist = edgex_add_other_hdrs (slist, ctx->reqhdrs);
  if (slist)
  {
    curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist);
  }

  /* Make the call */
  rc = curl_easy_perform (hnd);

  if (rc == CURLE_OK)
  {
    /* Get the cert info from the TLS peer */
    if (ctx->verify_peer)
    {
      edgex_log_peer_cert (lc, ctx, hnd);
    }

    /* Get the HTTP reponse status code from the server */
    curl_easy_getinfo (hnd, CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code == 409)
    {
      iot_log_debug (lc, "HTTP response 409 - Conflict");
      *err = EDGEX_HTTP_CONFLICT;
    }
    else if (http_code < 200 || http_code >= 300)
    {
      iot_log_debug (lc, "HTTP response: %ld", http_code);
      *err = EDGEX_HTTP_ERROR;
    }
    else
    {
      *err = EDGEX_OK;
    }
  }
  else if (rc == CURLE_ABORTED_BY_CALLBACK)
  {
    iot_log_debug (lc, "HTTP operation aborted via callback");
    *err = EDGEX_OK;
  }
  else
  {
    iot_log_error (lc, "Curl failed with code %d (%s)", rc, curl_easy_strerror (rc));
    *err = EDGEX_HTTP_ERROR;
  }

  curl_easy_cleanup (hnd);
  curl_slist_free_all (slist);
  return http_code;
}

long edgex_http_get (iot_logger_t *lc, edgex_ctx *ctx, const char *url, void *writefunc, devsdk_error *err)
{
  CURL *hnd = curl_easy_init ();
  return edgex_run_curl (lc, ctx, hnd, url, writefunc, NULL, err);
}

long edgex_http_delete (iot_logger_t *lc, edgex_ctx *ctx, const char *url, void *writefunc, devsdk_error *err)
{
  CURL *hnd = curl_easy_init ();
  curl_easy_setopt (hnd, CURLOPT_CUSTOMREQUEST, "DELETE");

  return edgex_run_curl (lc, ctx, hnd, url, writefunc, NULL, err);
}

long edgex_http_post
  (iot_logger_t *lc, edgex_ctx *ctx, const char *url, const char *data, void *writefunc, devsdk_error *err)
{
  struct curl_slist *slist;
  CURL *hnd = curl_easy_init ();

  curl_easy_setopt (hnd, CURLOPT_CUSTOMREQUEST, "POST");
  curl_easy_setopt (hnd, CURLOPT_POST, 1L);
  curl_easy_setopt (hnd, CURLOPT_POSTFIELDS, data);

  slist = edgex_add_hdr (NULL, "Content-Type", "application/json");
  return edgex_run_curl (lc, ctx, hnd, url, writefunc, slist, err);
}

long edgex_http_postbin
  (iot_logger_t *lc, edgex_ctx *ctx, const char *url, void *data, size_t length, const char *mime, void *writefunc, devsdk_error *err)
{
  struct curl_slist *slist;
  CURL *hnd = curl_easy_init ();

  curl_easy_setopt (hnd, CURLOPT_CUSTOMREQUEST, "POST");
  curl_easy_setopt (hnd, CURLOPT_POST, 1L);
  curl_easy_setopt (hnd, CURLOPT_POSTFIELDS, data);
  curl_easy_setopt (hnd, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)length);

  slist = edgex_add_hdr (NULL, "Content-Type", mime);
  return edgex_run_curl (lc, ctx, hnd, url, writefunc, slist, err);
}

long edgex_http_postfile
  (iot_logger_t *lc, edgex_ctx *ctx, const char *url, const char *fname, void *writefunc, devsdk_error *err)
{
  long http_code = 0;
  CURL *hnd;
#ifdef USE_CURL_MIME
  curl_mime *form = NULL;
  curl_mimepart *field = NULL;
#else
  struct curl_httppost *form = NULL;
  struct curl_httppost *lastptr = NULL;
#endif

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
  curl_formadd (&form, &lastptr, CURLFORM_COPYNAME, "file", CURLFORM_FILE, fname, CURLFORM_END);
  curl_formadd (&form, &lastptr, CURLFORM_COPYNAME, "filename", CURLFORM_COPYCONTENTS, fname, CURLFORM_END);
  curl_formadd (&form, &lastptr, CURLFORM_COPYNAME, "submit", CURLFORM_COPYCONTENTS, "send", CURLFORM_END);
  curl_easy_setopt(hnd, CURLOPT_HTTPPOST, form);
#endif

  http_code = edgex_run_curl (lc, ctx, hnd, url, writefunc, NULL, err);

#ifdef USE_CURL_MIME
  curl_mime_free (form);
#else
  curl_formfree(form);
#endif

  return http_code;
}

struct put_data
{
  const char *data;
  size_t offset;
  size_t remaining;
};

static size_t read_callback (char *buffer, size_t size, size_t nitems, void *userdata)
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
  (iot_logger_t *lc, edgex_ctx *ctx, const char *url, const char *data, void *writefunc, devsdk_error *err)
{
  struct curl_slist *slist;
  struct put_data cb_data;
  CURL *hnd = curl_easy_init ();

  curl_easy_setopt(hnd, CURLOPT_UPLOAD, 1L);

  if (data)
  {
    cb_data.data = data;
    cb_data.offset = 0;
    cb_data.remaining = strlen (data);
    curl_easy_setopt (hnd, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt (hnd, CURLOPT_READDATA, &cb_data);
    curl_easy_setopt (hnd, CURLOPT_INFILESIZE, (long)cb_data.remaining);
  }

  slist = edgex_add_hdr (NULL, "Content-Type", "application/json");
  return edgex_run_curl (lc, ctx, hnd, url, writefunc, slist, err);
}
