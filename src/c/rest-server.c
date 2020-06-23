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
  devsdk_strings *url;
  uint32_t methods;
  void *ctx;
  devsdk_http_handler_fn handler;
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

static devsdk_http_method method_from_string (const char *str)
{
  if (strcmp (str, "GET") == 0)
  { return DevSDK_Get; }
  if (strcmp (str, "POST") == 0)
  { return DevSDK_Post; }
  if (strcmp (str, "PUT") == 0)
  { return DevSDK_Put; }
  if (strcmp (str, "PATCH") == 0)
  { return DevSDK_Patch; }
  if (strcmp (str, "DELETE") == 0)
  { return DevSDK_Delete; }
  return DevSDK_Unknown;
}

static devsdk_strings *processUrl (const char *url)
{
  devsdk_strings *result = NULL;
  devsdk_strings *entry;
  devsdk_strings **nextcomp = &result;
  const char *p = url;
  const char *nextp;
  while (p)
  {
    while (*p == '/') p++;
    nextp = strchr (p, '/');
    *nextcomp =
    entry = malloc (sizeof (devsdk_strings));
    entry->str = nextp ? strndup (p, nextp - p) : strdup (p);
    entry->next = NULL;
    *nextcomp = entry;
    nextcomp = &(entry->next);
    p = nextp;
  }
  return result;
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

static bool match_url_pattern (const devsdk_strings *pattern, const devsdk_strings *url, devsdk_nvpairs **params)
{
  bool result = true;
  while (result && pattern && url)
  {
    if (strcmp (pattern->str, url->str))
    {
      size_t plen = strlen (pattern->str);
      if (plen >= 3 && *pattern->str == '{' && *(pattern->str + plen - 1) == '}')
      {
        char *name = alloca (plen - 1);
        strncpy (name, pattern->str + 1, plen - 2);
        name[plen - 2] = '\0';
        *params = devsdk_nvpairs_new (name, url->str, *params);
      }
      else
      {
        result = false;
      }
    }
    pattern = pattern->next;
    url = url->next;
  }
  return result && !(pattern || url);
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

  devsdk_http_method method = method_from_string (methodname);

  if (strlen (url) == 0 || strcmp (url, "/") == 0)
  {
    if (method == DevSDK_Get)
    {
      /* List available handlers */
      reply_size = 0;
      pthread_mutex_lock (&svr->lock);
      for (h = svr->handlers; h; h = h->next)
      {
        for (devsdk_strings *s = h->url; s; s = s->next)
        {
          reply_size += strlen (s->str) + 1;
        }
      }
      char *buff = malloc (reply_size + 1);
      buff[0] = '\0';
      for (h = svr->handlers; h; h = h->next)
      {
        for (devsdk_strings *s = h->url; s; s = s->next)
        {
          strcat (buff, s->str);
          strcat (buff, s->next ? "/" : "\n");
        }
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
    devsdk_nvpairs *params = NULL;
    devsdk_strings *elems = processUrl (url);
    status = MHD_HTTP_NOT_FOUND;
    pthread_mutex_lock (&svr->lock);
    for (h = svr->handlers; h; h = h->next)
    {
      if (match_url_pattern (h->url, elems, &params))
      {
        break;
      }
    }
    pthread_mutex_unlock (&svr->lock);
    if (h)
    {
      if (method & h->methods)
      {
        devsdk_http_request req;
        devsdk_http_reply rep;
        MHD_get_connection_values (conn, MHD_GET_ARGUMENT_KIND, queryIterator, &params);
        req.params = params;
        req.method = method;
        req.data.bytes = ctx->m_data;
        req.data.size = ctx->m_size;
        memset (&rep, 0, sizeof (devsdk_http_reply));
        h->handler (h->ctx, &req, &rep);
        status = rep.code;
        reply = rep.data.bytes;
        reply_size = rep.data.size;
        reply_type = rep.content_type;
      }
      else
      {
        status = MHD_HTTP_METHOD_NOT_ALLOWED;
      }
    }
    devsdk_nvpairs_free (params);
    devsdk_strings_free (elems);
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

bool edgex_rest_server_register_handler
(
  edgex_rest_server *svr,
  const char *url,
  uint32_t methods,
  void *context,
  devsdk_http_handler_fn handler
)
{
  bool result = true;
  handler_list *entry = malloc (sizeof (handler_list));
  entry->handler = handler;
  entry->url = processUrl (url);
  entry->methods = methods;
  entry->ctx = context;
  entry->next = NULL;
  pthread_mutex_lock (&svr->lock);
  handler_list **tail = &svr->handlers;
  while (*tail)
  {
    tail = &((*tail)->next);
  }
  *tail = entry;
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
    devsdk_strings_free (svr->handlers->url);
    free (svr->handlers);
    svr->handlers = tmp;
  }
  pthread_mutex_destroy (&svr->lock);
  free (svr);
}
