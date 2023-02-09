/*
 * Copyright (c) 2018-2023
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

#if MHD_VERSION > 0x00097001
#define EDGEX_MHD_RESULT enum MHD_Result
#else
#define EDGEX_MHD_RESULT int
#endif

typedef struct handler_list
{
  devsdk_strings *url;
  uint32_t methods;
  void *ctx;
  devsdk_http_handler_fn handler;
  struct handler_list *next;
} handler_list;

typedef struct cors_config
{
  const char *allowedorigin;
  const char *allowcreds;
  const char *allowmethods;
  const char *allowheaders;
  const char *exposeheaders;
  char *maxage;
  char **allowmethods_parsed;
  char **allowheaders_parsed;
  bool enabled;
} cors_config;

struct edgex_rest_server
{
  iot_logger_t *lc;
  struct MHD_Daemon *daemon;
  handler_list *handlers;
  pthread_mutex_t lock;
  uint64_t maxsize;
  cors_config cors;
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
  if (strcmp (str, "OPTIONS") == 0)
  { return DevSDK_Options; }
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

static EDGEX_MHD_RESULT queryIterator (void *p, enum MHD_ValueKind kind, const char *key, const char *value)
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

  iot_data_t *map = (iot_data_t *)p;
  iot_data_map_add (map, iot_data_alloc_string (key, IOT_DATA_COPY), iot_data_alloc_string (value ? value : "", IOT_DATA_COPY));

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

static bool cors_string_in_list (const char *s, char ** l)
{
  while (*l)
  {
    if (strcmp (*l++, s) == 0)
    {
      return true;
    }
  }
  return false;
}

static char **cors_string_to_list (const char *str)
{
  char **result;
  unsigned n = 0;
  char *s = strdup (str);
  char *sptr = NULL;
  char *elem = strtok_r (s, ", ", &sptr);
  while (elem)
  {
    n++;
    elem = strtok_r (NULL, ", ", &sptr);
  }
  result = malloc (sizeof (char *) * (n + 1));
  strcpy (s, str);
  sptr = NULL;
  elem = strtok_r (s, ", ", &sptr);
  for (unsigned i = 0; i < n; i++)
  {
    result[i] = strdup (elem);
    elem = strtok_r (NULL, ", ", &sptr);
  }
  result[n] = NULL;
  free (s);
  return result;
}

void edgex_rest_server_enable_cors
  (edgex_rest_server *svr, const char *origin, const char *methods, const char *headers, const char *expose, bool creds, int64_t maxage)
{
  svr->cors.enabled = true;
  svr->cors.allowedorigin = origin;
  svr->cors.allowmethods = methods;
  svr->cors.allowheaders = headers;
  svr->cors.exposeheaders = expose;
  svr->cors.allowcreds = creds ? "true" : "false";
  svr->cors.maxage = malloc (21);
  sprintf (svr->cors.maxage, "%" PRId64, maxage);
  svr->cors.allowmethods_parsed = cors_string_to_list (methods);
  svr->cors.allowheaders_parsed = cors_string_to_list (headers);
}

static bool cors_request_ok (struct MHD_Connection *conn, const char *method, const cors_config *cors)
{
  if (strcmp (cors->allowedorigin, "*"))
  {
    const char *origin = MHD_lookup_connection_value (conn, MHD_HEADER_KIND, MHD_HTTP_HEADER_ORIGIN);
    if (origin && strcmp (cors->allowedorigin, origin))
    {
      return false;
    }
  }
  const char *req_method = method ? method : MHD_lookup_connection_value (conn, MHD_HEADER_KIND, "Access-Control-Request-Method");
  if (req_method)
  {
    if (!cors_string_in_list (req_method, cors->allowmethods_parsed))
    {
      return false;
    }
  }
  const char *req_hdrs = MHD_lookup_connection_value (conn, MHD_HEADER_KIND, "Access-Control-Request-Headers");
  if (req_hdrs)
  {
    char *list = strdup (req_hdrs);
    char *sptr = NULL;
    char *elem = strtok_r (list, ", ", &sptr);
    while (elem)
    {
      if (!cors_string_in_list (elem, cors->allowheaders_parsed))
      {
        free (list);
        return false;
      }
      elem = strtok_r (NULL, ", ", &sptr);
    }
    free (list);
  }
  return true;
}

static void set_header_if_nonempty (struct MHD_Response *response, const char *header, const char *value)
{
  if (strlen (value))
  {
    MHD_add_response_header (response, header, value);
  }
}

static EDGEX_MHD_RESULT http_handler
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
  bool cors_passed = false;

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

  if (method == DevSDK_Options && svr->cors.enabled)
  {
    if (cors_request_ok (conn, NULL, &svr->cors))
    {
      response = MHD_create_response_from_buffer (0, "", MHD_RESPMEM_PERSISTENT);
      set_header_if_nonempty (response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, svr->cors.allowedorigin);
      MHD_add_response_header (response, "Access-Control-Allow-Credentials", svr->cors.allowcreds);
      set_header_if_nonempty (response, "Access-Control-Allow-Methods", svr->cors.allowmethods);
      set_header_if_nonempty (response, "Access-Control-Allow-Headers", svr->cors.allowheaders);
      MHD_add_response_header (response, "Access-Control-Max-Age", svr->cors.maxage);
      MHD_queue_response (conn, MHD_HTTP_NO_CONTENT, response);
      MHD_destroy_response (response);
      free (ctx->m_data);
      free (ctx);
      edgex_device_free_crlid ();
      return MHD_YES;
    }
    else
    {
      status = MHD_HTTP_METHOD_NOT_ALLOWED;
    }
  }
  else if (strlen (url) == 0 || strcmp (url, "/") == 0)
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
        if (svr->cors.enabled && !cors_request_ok (conn, methodname, &svr->cors))
        {
          status = MHD_HTTP_FORBIDDEN;
        }
        else
        {
          devsdk_http_request req;
          devsdk_http_reply rep;
          req.qparams = iot_data_alloc_map (IOT_DATA_STRING);
          MHD_get_connection_values (conn, MHD_GET_ARGUMENT_KIND, queryIterator, req.qparams);
          req.params = params;
          req.method = method;
          req.data.bytes = ctx->m_data;
          req.data.size = ctx->m_size;
          req.authorization_header_value = MHD_lookup_connection_value (conn, MHD_HEADER_KIND, MHD_HTTP_HEADER_AUTHORIZATION);
          req.content_type = MHD_lookup_connection_value (conn, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_TYPE);
          memset (&rep, 0, sizeof (devsdk_http_reply));
          h->handler (h->ctx, &req, &rep);
          status = rep.code;
          reply = rep.data.bytes;
          reply_size = rep.data.size;
          reply_type = rep.content_type;
          cors_passed = svr->cors.enabled;
          iot_data_free (req.qparams);
        }
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
  if (cors_passed)
  {
    set_header_if_nonempty (response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, svr->cors.allowedorigin);
    MHD_add_response_header (response, "Access-Control-Allow-Credentials", svr->cors.allowcreds);
    set_header_if_nonempty (response, "Access-Control-Expose-Headers", svr->cors.exposeheaders);
    MHD_add_response_header (response, MHD_HTTP_HEADER_VARY, MHD_HTTP_HEADER_ORIGIN);
  }
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

static void edgex_rest_server_log (void *ctx, const char * fmt, va_list ap)
{
  va_list ap2;
  char *msg;
  iot_logger_t *lc = (iot_logger_t *)ctx;
  va_copy (ap2, ap);
  msg = malloc (vsnprintf (NULL, 0, fmt, ap) + 1);
  vsprintf (msg, fmt, ap2);
  va_end (ap2);
  iot_log_error (lc, "microhttpd error: %s", msg);
  free (msg);
}

edgex_rest_server *edgex_rest_server_create
  (iot_logger_t *lc, const char *bindaddr, uint16_t port, uint64_t maxsize, devsdk_error *err)
{
  edgex_rest_server *svr;
  uint16_t flags = MHD_USE_THREAD_PER_CONNECTION | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG;

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
      if (res->ai_family == AF_INET6)
      {
        flags |= MHD_USE_IPv6;
      }
      svr->daemon = MHD_start_daemon (flags, port, 0, 0, http_handler, svr, MHD_OPTION_EXTERNAL_LOGGER, edgex_rest_server_log, lc, MHD_OPTION_SOCK_ADDR, res->ai_addr, MHD_OPTION_END);
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
    svr->daemon = MHD_start_daemon (flags, port, 0, 0, http_handler, svr, MHD_OPTION_EXTERNAL_LOGGER, edgex_rest_server_log, lc, MHD_OPTION_END);
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

iot_data_t *edgex_v3_error_response (iot_logger_t *lc, char *msg, ...)
{
  iot_data_t *result;
  char *buf = malloc (EDGEX_ERRBUFSZ);
  va_list args;
  va_start (args, msg);
  vsnprintf (buf, EDGEX_ERRBUFSZ, msg, args);
  va_end (args);

  iot_log_error (lc, buf);
  result = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_string_map_add (result, "ApiVersion", iot_data_alloc_string (EDGEX_API_VERSION, IOT_DATA_REF));
  iot_data_string_map_add (result, "message", iot_data_alloc_string (msg, IOT_DATA_TAKE));
  return result;
}

iot_data_t *edgex_v3_base_response (const char *msg)
{
  iot_data_t *result = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_string_map_add (result, "ApiVersion", iot_data_alloc_string (EDGEX_API_VERSION, IOT_DATA_REF));
  iot_data_string_map_add (result, "message", iot_data_alloc_string (msg, IOT_DATA_REF));
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
  if (svr->cors.enabled)
  {
    for (char **c = svr->cors.allowmethods_parsed; *c; c++)
    {
      free (*c);
    }
    for (char **c = svr->cors.allowheaders_parsed; *c; c++)
    {
      free (*c);
    }
    free (svr->cors.allowmethods_parsed);
    free (svr->cors.allowheaders_parsed);
    free (svr->cors.maxage);
  }
  free (svr);
}
