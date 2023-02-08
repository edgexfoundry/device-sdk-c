/*
 * Copyright (c) 2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "api.h"
#include "bus.h"
#include "bus-impl.h"
#include <iot/time.h>
#include <iot/thread.h>
#include <hiredis/hiredis.h>

typedef struct edgex_bus_redstr_t
{
  iot_logger_t *lc;
  redisContext *ctx;
  redisContext *ctx_rd;
  pthread_t thread;
  pthread_mutex_t mtx;
} edgex_bus_redstr_t;

static void edgex_bus_redstr_free (void *ctx)
{
  edgex_bus_redstr_t *cinfo = (edgex_bus_redstr_t *)ctx;
  redisFree (cinfo->ctx);
  redisFree (cinfo->ctx_rd);
  pthread_cancel (cinfo->thread);
  pthread_mutex_destroy (&cinfo->mtx);
  free (cinfo);
}

static void edgex_bus_redstr_remapslash (char *c)
{
  while ((c = strchr (c, '/')))
  {
    *c = '.';
  }
}

static void edgex_bus_redstr_remapdot (char *c)
{
  while ((c = strchr (c, '.')))
  {
    *c = '/';
  }
}

static void edgex_bus_redstr_post (void *ctx, const char *topic_in, const iot_data_t *envelope)
{
  edgex_bus_redstr_t *cinfo = (edgex_bus_redstr_t *)ctx;
  char *topic = strdup (topic_in);
  edgex_bus_redstr_remapslash (topic);
  char *json = iot_data_to_json (envelope);
  pthread_mutex_lock (&cinfo->mtx);
  redisReply *reply = redisCommand (cinfo->ctx, "PUBLISH %s %s", topic, json);

  if (reply == NULL)
  {
    iot_log_error (cinfo->lc, "Error posting via Redis: %s", cinfo->ctx->errstr);
    if (redisReconnect (cinfo->ctx) == REDIS_ERR)
    {
      iot_log_error (cinfo->lc, "Redis reconnection failed: %s", cinfo->ctx->errstr);
    }
  }

  freeReplyObject (reply);
  pthread_mutex_unlock (&cinfo->mtx);
  free (json);
  free (topic);
}

static redisContext *edgex_bus_redstr_connect (iot_logger_t *lc, const char *host, uint16_t port, struct timeval tv)
{
  redisContext *result = redisConnectWithTimeout (host, port, tv);
  if (result)
  {
    if (result->err)
    {
      iot_log_error (lc, "Failed to create Redis Streams client: %s", result->errstr);
      redisFree (result);
      result = NULL;
    }
  }
  else
  {
    iot_log_error (lc, "Can't allocate redis context");
  }
  return result;
}

static void edgex_bus_redis_subscribe (void *ctx, const char *topic_in)
{
}

static void *edgex_bus_redis_listener (void *p)
{
  edgex_bus_t *bus = (edgex_bus_t *)p;
  edgex_bus_redstr_t *cinfo = (edgex_bus_redstr_t *)bus->ctx;
  redisReply *rep;
  while (redisGetReply (cinfo->ctx_rd, (void*)&rep) == REDIS_OK)
  {
    if (rep->type == REDIS_REPLY_ARRAY && rep->elements == 4 && rep->element[2]->type == REDIS_REPLY_STRING && rep->element[3]->type == REDIS_REPLY_STRING)
    {
      char *topic = strdup (rep->element[2]->str);
      edgex_bus_redstr_remapdot (topic);
      edgex_bus_handle_request (bus, topic, rep->element[3]->str);
      free (topic);
    }
    freeReplyObject (rep);
  }
  return NULL;
}

static bool edgex_bus_redstr_auth (iot_logger_t *lc, redisContext *ctx, const char *user, const char *pass)
{
  bool result = true;
  redisReply *reply = (redisReply *)redisCommand (ctx, "AUTH %s %s", user ? user : "", pass);
  if (reply == NULL || reply->type == REDIS_REPLY_ERROR)
  {
    result = false;
    iot_log_error (lc, "Error authenticating with Redis: %s", reply ? reply->str : ctx->errstr);
  }
  freeReplyObject (reply);
  return result;
}

edgex_bus_t *edgex_bus_create_redstr (iot_logger_t *lc, const char *svcname, const iot_data_t *cfg, edgex_secret_provider_t *secstore, iot_threadpool_t *queue, const devsdk_timeout *tm)
{
  struct timeval tv;
  edgex_bus_t *result = NULL;
  edgex_bus_redstr_t *cinfo = calloc (1, sizeof (edgex_bus_redstr_t));
  cinfo->lc = lc;
  pthread_mutex_init (&cinfo->mtx, NULL);

  const char *host = iot_data_string_map_get_string (cfg, EX_BUS_HOST);
  uint16_t port = iot_data_ui16 (iot_data_string_map_get (cfg, EX_BUS_PORT));
  if (port == 0)
  {
    port = 6379;
  }

  iot_log_info (lc, "Message Bus is set to Redis streams at %s:%u", host, port);

  tv.tv_sec = tm->interval / 1000;
  tv.tv_usec = (tm->interval % 1000) * 1000;

  while (true)
  {
    uint64_t t1, t2;
    t1 = iot_time_msecs ();
    if (cinfo->ctx == NULL)
    {
      cinfo->ctx = edgex_bus_redstr_connect (lc, host, port, tv);
    }
    if (cinfo->ctx_rd == NULL)
    {
      cinfo->ctx_rd = edgex_bus_redstr_connect (lc, host, port, tv);
    }
    t2 = iot_time_msecs ();
    if ((cinfo->ctx && cinfo->ctx_rd) || t2 > tm->deadline - tm->interval)
    {
      break;
    }
    if (tm->interval > t2 - t1)
    {
      iot_wait_msecs (tm->interval - (t2 - t1));
    }
  }
  if (cinfo->ctx == NULL || cinfo->ctx_rd == NULL)
  {
    edgex_bus_redstr_free (cinfo);
    return NULL;
  }

  if (strcmp (iot_data_string_map_get_string (cfg, EX_BUS_AUTHMODE), "usernamepassword") == 0)
  {
    bool auth = true;
    iot_data_t *secrets = edgex_secrets_get (secstore, iot_data_string_map_get_string (cfg, EX_BUS_SECRETNAME));
    const char *pass = iot_data_string_map_get_string (secrets, "password");
    if (pass)
    {
      const char *user = iot_data_string_map_get_string (secrets, "username");
      auth = edgex_bus_redstr_auth (lc, cinfo->ctx, user, pass) && edgex_bus_redstr_auth (lc, cinfo->ctx_rd, user, pass);
    }
    iot_data_free (secrets);
    if (!auth)
    {
      edgex_bus_redstr_free (cinfo);
      return NULL;
    }
  }

  redisReply *readreply = redisCommand (cinfo->ctx_rd, "psubscribe %s.*", iot_data_string_map_get_string (cfg, EX_BUS_TOPIC));
  if (!readreply || cinfo->ctx_rd->err)
  {
    iot_log_error (cinfo->lc, "Redis: Can't subscribe");
  }
  freeReplyObject (readreply);

  result = malloc (sizeof (edgex_bus_t));
  edgex_bus_init (result, svcname, cfg);
  result->ctx = cinfo;
  result->postfn = edgex_bus_redstr_post;
  result->freefn = edgex_bus_redstr_free;
  result->subsfn = edgex_bus_redis_subscribe;
  iot_thread_create (&cinfo->thread, edgex_bus_redis_listener, result, IOT_THREAD_NO_PRIORITY, IOT_THREAD_NO_AFFINITY, lc);
  return result;
}
