/*
 * Copyright (c) 2018-2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "rest-server.h"
#include "api.h"
#include "edgex-rest.h"
#include "correlation.h"
#include "errorlist.h"

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <microhttpd.h>

#define EDGEX_ERRBUFSZ 1024

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
  uint64_t maxsize;
};

typedef struct http_context_s
{
  char *m_data;
  size_t m_size;
} http_context_t;

static const char *ds_paramlist[] = DS_PARAMLIST;

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
    entry = calloc (1, sizeof (devsdk_strings));
    entry->str = nextp ? strndup (p, nextp - p) : strdup (p);
    *nextcomp = entry;
    nextcomp = &(entry->next);
    p = nextp;
  }
  return result;
}

static int queryIterator (void *p, enum MHD_ValueKind kind, const char *key, const char *value)
{
  if (strncmp (key, DS_PREFIX, strlen (DS_PREFIX)) == 0)
  {
    unsigned i;
    for (i = 0; i < sizeof (ds_paramlist) / sizeof (*ds_paramlist); i++)
    {
      if (strcmp (key, ds_paramlist[i]) == 0)
      {
        break;
      }
    }
    if (i == sizeof (ds_paramlist) / sizeof (*ds_paramlist))
    {
      return MHD_YES;
    }
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
    size_t required = ctx->m_size + (*upload_data_size) + 1;
    if (svr->maxsize && required > svr->maxsize)
    {
      free (ctx->m_data);
      free (ctx);
      iot_log_error (svr->lc, "http: request size of %zu exceeds configured maximum", required);
      return MHD_NO;
    }
    ctx->m_data = (char *) realloc (ctx->m_data, required);
    memcpy (ctx->m_data + ctx->m_size, upload_data, (*upload_data_size));
    ctx->m_size += *upload_data_size;
    ctx->m_data[ctx->m_size] = 0;
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
    iot_log_trace (svr->lc, "Incoming %s request to %s%s%s", methodname, url, ctx->m_size ? ", data " : " (no data)", ctx->m_size ? ctx->m_data : "");
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
        req.content_type = MHD_lookup_connection_value (conn, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_TYPE);
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
  MHD_add_response_header (response, "X-Correlation-ID", edgex_device_get_crlid ());
  MHD_queue_response (conn, status, response);
  MHD_destroy_response (response);

  /* Clean up */

  free (ctx->m_data);
  free (ctx);
  edgex_device_free_crlid ();
  return MHD_YES;
}

static void edgex_rest_sa_out (char *res, const struct sockaddr *sa)
{
  switch (sa->sa_family)
  {
    case AF_INET:
    {
      struct sockaddr_in *sa4 = (struct sockaddr_in *)sa;
      inet_ntop (AF_INET, &sa4->sin_addr, res, INET6_ADDRSTRLEN);
      break;
    }
    case AF_INET6:
    {
      struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sa;
      inet_ntop (AF_INET6, &sa6->sin6_addr, res, INET6_ADDRSTRLEN);
      break;
    }
    default:
      sprintf (res, "(unknown family)");
  }
}

edgex_rest_server *edgex_rest_server_create
  (iot_logger_t *lc, const char *bindaddr, uint16_t port, uint64_t maxsize, devsdk_error *err)
{
  edgex_rest_server *svr;
  uint16_t flags = MHD_USE_THREAD_PER_CONNECTION;
  /* config: flags |= MHD_USE_IPv6 ? */

  svr = calloc (1, sizeof (edgex_rest_server));
  svr->lc = lc;
  svr->maxsize = maxsize;

  pthread_mutex_init (&svr->lock, NULL);

  /* Start http server */

  if (strcmp (bindaddr, "0.0.0.0"))
  {
    struct addrinfo *res;
    char svc[6];
    char resaddr[INET6_ADDRSTRLEN];
    sprintf (svc, "%" PRIu16, port);
    if (getaddrinfo (bindaddr, svc, NULL, &res) == 0)
    {
      iot_log_info (lc, "Starting HTTP server on interface %s, port %d", bindaddr, port);
      edgex_rest_sa_out (resaddr, res->ai_addr);
      iot_log_debug (lc, "Resolved interface is %s", resaddr);
      svr->daemon = MHD_start_daemon (flags, port, 0, 0, http_handler, svr, MHD_OPTION_SOCK_ADDR, res->ai_addr, MHD_OPTION_END);
      freeaddrinfo (res);
    }
    else
    {
      iot_log_error (lc, "HTTP server: unable to resolve bind address %s", bindaddr);
    }
  }
  else
  {
    iot_log_info (lc, "Starting HTTP server on port %d (all interfaces)", port);
    svr->daemon = MHD_start_daemon (flags, port, 0, 0, http_handler, svr, MHD_OPTION_END);
  }

  if (svr->daemon == NULL)
  {
    *err = EDGEX_HTTP_SERVER_FAIL;
    iot_log_error (lc, "Unable to start HTTP server");
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

void edgex_error_response (iot_logger_t *lc, devsdk_http_reply *reply, int code, char *msg, ...)
{
  char *buf = malloc (EDGEX_ERRBUFSZ);
  va_list args;
  va_start (args, msg);
  vsnprintf (buf, EDGEX_ERRBUFSZ, msg, args);
  va_end (args);

  iot_log_error (lc, buf);
  edgex_errorresponse *err = edgex_errorresponse_create (code, buf);
  edgex_errorresponse_write (err, reply);
  edgex_errorresponse_free (err);
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
