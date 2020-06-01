/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "rest-server.h"
#include "edgex-rest.h"
#include "microhttpd.h"
#include "correlation.h"
#include "errorlist.h"

#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#define STR_BLK_SIZE 512
#define EDGEX_DS_PREFIX "ds-"

typedef struct handler_list
{
  const char *url;
  uint32_t methods;
  void *ctx;
  edgex_http_handler_fn handler;
  struct handler_list *next;
} handler_list;

struct edgex_rest_server
{
  iot_logger_t *lc;
  struct MHD_Daemon *daemon;
  handler_list *handlers;
  pthread_mutex_t lock;
};

typedef struct http_context_s
{
  char *m_data;
  size_t m_size;
} http_context_t;

static edgex_http_method method_from_string (const char *str)
{
  if (strcmp (str, "GET") == 0)
  { return GET; }
  if (strcmp (str, "POST") == 0)
  { return POST; }
  if (strcmp (str, "PUT") == 0)
  { return PUT; }
  if (strcmp (str, "PATCH") == 0)
  { return PATCH; }
  if (strcmp (str, "DELETE") == 0)
  { return DELETE; }
  return UNKNOWN;
}

static char *normalizeUrl (const char *url)
{
  /* Only deduplication of '/' is performed */

  char *res = malloc (strlen (url) + 1);
  const char *upos = url;
  char *rpos = res;
  while (*upos)
  {
    if ((*rpos++ = *upos++) == '/')
    {
      while (*upos == '/')
      {
        upos++;
      }
    }
  }
  *rpos = '\0';
  return res;
}

static int queryIterator (void *p, enum MHD_ValueKind kind, const char *key, const char *value)
{
  if (strncmp (key, EDGEX_DS_PREFIX, strlen (EDGEX_DS_PREFIX)) == 0)
  {
    return MHD_YES;
  }

  devsdk_nvpairs **list = (devsdk_nvpairs **)p;
  *list = devsdk_nvpairs_new (key, value ? value : "", *list);

  return MHD_YES;
}

static int http_handler
(
  void *this,
  struct MHD_Connection *conn,
  const char *url,
  const char *methodname,
  const char *version,
  const char *upload_data,
  size_t *upload_data_size,
  void **context
)
{
  int status = MHD_HTTP_OK;
  http_context_t *ctx = (http_context_t *) *context;
  edgex_rest_server *svr = (edgex_rest_server *) this;
  struct MHD_Response *response = NULL;
  void *reply = NULL;
  size_t reply_size = 0;
  const char *reply_type = NULL;
  handler_list *h;

  /* First call used to create call context */

  if (ctx == 0)
  {
    ctx = (http_context_t *) malloc (sizeof (*ctx));
    ctx->m_size = 0;
    ctx->m_data = NULL;
    *context = (void *) ctx;
    return MHD_YES;
  }

  /* Subsequent calls transfer data */

  if (*upload_data_size)
  {
    ctx->m_data = (char *) realloc
      (ctx->m_data, ctx->m_size + (*upload_data_size) + 1);
    memcpy (ctx->m_data + ctx->m_size, upload_data, (*upload_data_size) + 1);
    ctx->m_size += *upload_data_size;
    *upload_data_size = 0;
    return MHD_YES;
  }
  *context = 0;

  /* Last call with no data handles request */

  edgex_device_alloc_crlid
    (MHD_lookup_connection_value (conn, MHD_HEADER_KIND, EDGEX_CRLID_HDR));

  edgex_http_method method = method_from_string (methodname);

  if (strlen (url) == 0 || strcmp (url, "/") == 0)
  {
    if (method == GET)
    {
      /* List available handlers */
      reply_size = 0;
      pthread_mutex_lock (&svr->lock);
      for (h = svr->handlers; h; h = h->next)
      {
        reply_size += strlen (h->url) + 1;
      }
      char *buff = malloc (reply_size + 1);
      buff[0] = '\0';
      for (h = svr->handlers; h; h = h->next)
      {
        strcat (buff, h->url);
        strcat (buff, "\n");
      }
      reply = buff;
      pthread_mutex_unlock (&svr->lock);
    }
    else
    {
      status = MHD_HTTP_METHOD_NOT_ALLOWED;
    }
  }
  else
  {
    status = MHD_HTTP_NOT_FOUND;
    char *nurl = normalizeUrl (url);
    pthread_mutex_lock (&svr->lock);
    for (h = svr->handlers; h; h = h->next)
    {
      if
      (
        (h->url[strlen (h->url) - 1] == '/') ?
        (strncmp (nurl, h->url, strlen (h->url)) == 0) :
        (strcmp (nurl, h->url) == 0)
      )
      {
        break;
      }
    }
    pthread_mutex_unlock (&svr->lock);
    if (h)
    {
      if (method & h->methods)
      {
        devsdk_nvpairs *qparams = NULL;
        MHD_get_connection_values (conn, MHD_GET_ARGUMENT_KIND, queryIterator, &qparams);
        status = h->handler
          (h->ctx, nurl + strlen (h->url), qparams, method, ctx->m_data, ctx->m_size, &reply, &reply_size, &reply_type);
        devsdk_nvpairs_free (qparams);
      }
      else
      {
        status = MHD_HTTP_METHOD_NOT_ALLOWED;
      }
    }
    free (nurl);
  }

  /* Send reply */

  if (reply_type == NULL)
  {
    reply_type = CONTENT_PLAINTEXT;
  }
  if (reply == NULL)
  {
    reply = strdup ("");
    reply_size = 0;
  }
  response = MHD_create_response_from_buffer (reply_size, reply, MHD_RESPMEM_MUST_FREE);
  MHD_add_response_header (response, "Content-Type", reply_type);
  MHD_queue_response (conn, status, response);
  MHD_destroy_response (response);

  /* Clean up */

  free (ctx->m_data);
  free (ctx);
  edgex_device_free_crlid ();
  return MHD_YES;
}

edgex_rest_server *edgex_rest_server_create
  (iot_logger_t *lc, uint16_t port, devsdk_error *err)
{
  edgex_rest_server *svr;
  uint16_t flags = MHD_USE_THREAD_PER_CONNECTION;
  /* config: flags |= MHD_USE_IPv6 ? */

  svr = malloc (sizeof (edgex_rest_server));
  svr->lc = lc;
  svr->handlers = NULL;
  pthread_mutex_init (&svr->lock, NULL);

  /* Start http server */

  iot_log_debug (lc, "Starting HTTP server on port %d", port);
  svr->daemon =
    MHD_start_daemon (flags, port, 0, 0, http_handler, svr, MHD_OPTION_END);
  if (svr->daemon == NULL)
  {
    *err = EDGEX_HTTP_SERVER_FAIL;
    iot_log_debug (lc, "MHD_start_daemon failed");
    edgex_rest_server_destroy (svr);
    return NULL;
  }
  else
  {
    return svr;
  }
}

static bool urlClash (const char *u1, const char *u2)
{
  unsigned i;
  for (i = 0; u1[i] && u2[i]; i++)
  {
    if (u1[i] != u2[i])
    {
      return false;
    }
  }
  return (u1[i] == '/') || (u2[i] == '/');
}

bool edgex_rest_server_register_handler
(
  edgex_rest_server *svr,
  const char *url,
  uint32_t methods,
  void *context,
  edgex_http_handler_fn handler
)
{
  bool result = true;
  handler_list *entry = malloc (sizeof (handler_list));
  entry->handler = handler;
  entry->url = url;
  entry->methods = methods;
  entry->ctx = context;
  pthread_mutex_lock (&svr->lock);
  for (handler_list *match = svr->handlers; match; match = match->next)
  {
    if (urlClash (match->url, url))
    {
      result = false;
      iot_log_error (svr->lc, "Register handler: \"%s\" conflicts with \"%s\", skipping", url, match->url);
      free (entry);
      break;
    }
  }
  if (result)
  {
    entry->next = svr->handlers;
    svr->handlers = entry;
  }
  pthread_mutex_unlock (&svr->lock);
  return result;
}

void edgex_rest_server_destroy (edgex_rest_server *svr)
{
  handler_list *tmp;
  if (svr->daemon)
  {
    MHD_stop_daemon (svr->daemon);
  }
  while (svr->handlers)
  {
    tmp = svr->handlers->next;
    free (svr->handlers);
    svr->handlers = tmp;
  }
  pthread_mutex_destroy (&svr->lock);
  free (svr);
}
