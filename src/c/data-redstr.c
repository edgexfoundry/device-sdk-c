/*
 * Copyright (c) 2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "data-redstr.h"
#include "correlation.h"
#include "service.h"
#include "device.h"
#include "iot/base64.h"
#include "iot/time.h"
#include "iot/thread.h"
#include <hiredis/hiredis.h>
#include <microhttpd.h>

JSON_Value *edgex_redstr_config_json (const iot_data_t *allconf)
{
  JSON_Value *mqval = json_value_init_object ();
  JSON_Object *mqobj = json_value_get_object (mqval);
  json_object_set_string (mqobj, "Host", iot_data_string_map_get_string (allconf, EX_BUS_HOST));
  json_object_set_uint (mqobj, "Port", iot_data_ui16 (iot_data_string_map_get (allconf, EX_BUS_PORT)));
  json_object_set_string (mqobj, "Topic", iot_data_string_map_get_string (allconf, EX_BUS_TOPIC));

  JSON_Value *topicval = json_value_init_object ();
  JSON_Object *topicobj = json_value_get_object (topicval);
  json_object_set_string (topicobj, "CommandRequestTopic", iot_data_string_map_get_string (allconf, EX_BUS_TOPIC_CMDREQ));
  json_object_set_string (topicobj, "CommandResponseTopicPrefix", iot_data_string_map_get_string (allconf, EX_BUS_TOPIC_CMDRESP));
  json_object_set_value (mqobj, "Topics", topicval);

  return mqval;
}

typedef struct edc_redstr_conninfo
{
  devsdk_service_t *svc;
  iot_logger_t *lc;
  redisContext *ctx;
  redisContext *ctx_rd;
  char *topicbase;
  char *pubsub_topicbase;
  char *metric_topicbase;
  pthread_mutex_t mtx;
  pthread_t thread;
  ssize_t offset;
  bool running;
} edc_redstr_conninfo;

static void edc_redstr_freefn (iot_logger_t *lc, void *address)
{
  edc_redstr_conninfo *cinfo = (edc_redstr_conninfo *)address;
  redisFree (cinfo->ctx);
  redisFree (cinfo->ctx_rd);
  free (cinfo->topicbase);
  free (cinfo->pubsub_topicbase);
  free (cinfo->metric_topicbase);
  pthread_mutex_destroy (&cinfo->mtx);
  if (cinfo->running)
  {
    pthread_cancel (cinfo->thread);
  }
  free (cinfo);
}

static void edc_redstr_remapslash (char *c)
{
  while ((c = strchr (c, '/')))
  {
    *c = '.';
  }
}

static void edc_redstr_remaphash (char *c)
{
  char *e = c + strlen (c) - 1;
  if (*e == '#')
  {
    *e = '*';
  }
}

static void edc_redstr_send (edc_redstr_conninfo *cinfo, const char *topic, const iot_data_t *msg)
{
  char *json = iot_data_to_json (msg);
  pthread_mutex_lock (&cinfo->mtx);
  redisReply *reply = redisCommand (cinfo->ctx, "PUBLISH %s %s", topic, json);

  if (reply == NULL)
  {
    iot_log_error (cinfo->lc, "Error posting Event via Redis: %s", cinfo->ctx->errstr);
    if (redisReconnect (cinfo->ctx) == REDIS_ERR)
    {
      iot_log_error (cinfo->lc, "Redis reconnection failed: %s", cinfo->ctx->errstr);
    }
  }

  freeReplyObject (reply);
  pthread_mutex_unlock (&cinfo->mtx);
  free (json);
}

static void edc_redstr_pubmetric (void *address, const char *mname, const iot_data_t *envelope)
{
  edc_redstr_conninfo *cinfo = (edc_redstr_conninfo *)address;

  char *topic = malloc (strlen (cinfo->metric_topicbase) + strlen (mname) + 1);
  strcpy (topic, cinfo->metric_topicbase);
  strcat (topic, mname);

  edc_redstr_send (cinfo, topic, envelope);

  free (topic);
}

static char *edc_redstr_b64 (devsdk_http_data src)
{
  size_t encsz;
  char *result;

  encsz = iot_b64_encodesize (src.size);
  result = malloc (encsz);
  iot_b64_encode (src.bytes, src.size, result, encsz);
  return result;
}

static void edc_redstr_postfn (iot_logger_t *lc, void *address, edgex_event_cooked *event)
{
  devsdk_http_reply h;
  const char *crl;
  bool freecrl = false;
  char *topic;
  edc_redstr_conninfo *cinfo = (edc_redstr_conninfo *)address;

  topic = malloc (strlen (cinfo->topicbase) + strlen (event->path) + 2);
  strcpy (topic, cinfo->topicbase);
  strcat (topic, ".");
  strcat (topic, event->path);
  edc_redstr_remapslash (topic);
  edgex_event_cooked_write (event, &h);

  crl = edgex_device_get_crlid ();
  if (!crl)
  {
    edgex_device_alloc_crlid (NULL);
    freecrl = true;
    crl = edgex_device_get_crlid ();
  }

  iot_data_t *msg = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_string_map_add (msg, "CorrelationID", iot_data_alloc_string (crl, IOT_DATA_REF));
  iot_data_string_map_add (msg, "Payload", iot_data_alloc_string (edc_redstr_b64 (h.data), IOT_DATA_TAKE));
  iot_data_string_map_add (msg, "ContentType", iot_data_alloc_string (h.content_type, IOT_DATA_REF));

  edc_redstr_send (cinfo, topic, msg);

  free (h.data.bytes);
  iot_data_free (msg);
  free (topic);
  if (freecrl)
  {
    edgex_device_free_crlid ();
  }
}

static redisContext *edc_redstr_connect (iot_logger_t *lc, const char *host, uint16_t port, struct timeval tv)
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

static bool edc_redis_subscribe (iot_logger_t *lc, redisContext *ctx, const char *topic)
{
  bool result = true;
  char *s_topic = strdup (topic);
  edc_redstr_remapslash (s_topic);
  edc_redstr_remaphash (s_topic);

  redisReply *readreply = redisCommand (ctx, "psubscribe %s", s_topic);
  if (!readreply || ctx->err)
  {
    iot_log_error (lc, "Redis: Can't subscribe to topic");
    result = false;
  }
  freeReplyObject (readreply);
  free (s_topic);
  return result;
}

static char *edc_redstr_stripright (char *t)
{
  char *c = strrchr (t, '.');
  if (c)
  {
    *c = 0;
    return c + 1;
  }
  else
  {
    return NULL;
  }
}

static void *edc_redstr_listener (void *p)
{
  redisReply *rep;
  edc_redstr_conninfo *cinfo = (edc_redstr_conninfo *)p;
  while (redisGetReply (cinfo->ctx_rd, (void*)&rep) == REDIS_OK)
  {
    if (rep->type == REDIS_REPLY_ARRAY && rep->elements == 4 && rep->element[2]->type == REDIS_REPLY_STRING && rep->element[3]->type == REDIS_REPLY_STRING)
    {
      char *topic = strdup (rep->element[2]->str);
      char *op = edc_redstr_stripright (topic);
      char *cmd = edc_redstr_stripright (topic);
      char *dev = edc_redstr_stripright (topic);
      iot_data_t *envelope = iot_data_from_json (rep->element[3]->str);
      if (envelope && strcmp (iot_data_string_map_get_string (envelope, "ApiVersion"), "v2") == 0)
      {
        devsdk_http_request hreq;
        devsdk_http_reply hreply;  // V3: make the handlers take more generic parameters
        iot_data_t *reply;

        char *rtopic = malloc (strlen (rep->element[2]->str) + cinfo->offset);
        sprintf (rtopic, "%s.%s.%s.%s", cinfo->pubsub_topicbase, dev, cmd, op);
        memset (&hreply, 0, sizeof (hreply));
        memset (&hreq, 0, sizeof (hreq));
        reply = iot_data_alloc_map (IOT_DATA_STRING);
        iot_data_string_map_add (reply, "CorrelationID", iot_data_add_ref (iot_data_string_map_get (envelope, "CorrelationID")));
        iot_data_string_map_add (reply, "RequestID", iot_data_add_ref (iot_data_string_map_get (envelope, "RequestID")));
        iot_data_string_map_add (reply, "ApiVersion", iot_data_add_ref (iot_data_string_map_get (envelope, "ApiVersion")));

        if (strcmp (op, "get") == 0)
        {
          hreq.method = DevSDK_Get;
        }
        else if (strcmp (op, "set") == 0)
        {
          hreq.method = DevSDK_Put;
        }
        else
        {
          hreq.method = DevSDK_Unknown;
        }

        if (hreq.method != DevSDK_Unknown)
        {
          const char *payload;
          hreq.params = devsdk_nvpairs_new ("cmd", cmd, devsdk_nvpairs_new ("name", dev, NULL));
          hreq.qparams = iot_data_add_ref (iot_data_string_map_get (envelope, "QueryParams"));
          hreq.content_type = iot_data_string_map_get_string (envelope, "ContentType");
          payload = iot_data_string_map_get_string (envelope, "Payload");
          if (payload)
          {
            hreq.data.size = iot_b64_maxdecodesize (payload);
            hreq.data.bytes = malloc (hreq.data.size + 1);
            iot_b64_decode (payload, hreq.data.bytes, &hreq.data.size);
            ((char *)hreq.data.bytes)[hreq.data.size] = '\0';
          }

          edgex_device_handler_device_namev2 (cinfo->svc, &hreq, &hreply);

          devsdk_nvpairs_free ((devsdk_nvpairs *)hreq.params);
          iot_data_free (hreq.qparams);
          free (hreq.data.bytes);
        }
        else
        {
          edgex_error_response (cinfo->lc, &hreply, MHD_HTTP_METHOD_NOT_ALLOWED, "redis: only get and set operations allowed");
        }

        iot_data_string_map_add (reply, "ContentType", iot_data_alloc_string (hreply.content_type, IOT_DATA_REF));
        iot_data_string_map_add (reply, "ErrorCode", iot_data_alloc_ui8 (hreply.code / 100 == 2 ? 0 : 1));
        iot_data_string_map_add (reply, "Payload", iot_data_alloc_string (edc_redstr_b64 (hreply.data), IOT_DATA_TAKE));

        edc_redstr_send (cinfo, rtopic, reply);

        free (rtopic);
        iot_data_free (reply);
        free (hreply.data.bytes);
      }
      else
      {
        iot_log_error (cinfo->lc, "redis: unrecognized format in request");
      }
      free (topic);
      iot_data_free (envelope);
    }
    else
    {
      iot_log_error (cinfo->lc, "redis: unexpected message format");
    }
    freeReplyObject (rep);
  }
  return NULL;
}

static bool edc_redstr_auth (iot_logger_t *lc, redisContext *ctx, const char *user, const char *pass)
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

edgex_data_client_t *edgex_data_client_new_redstr (devsdk_service_t *svc, const devsdk_timeout *tm, iot_threadpool_t *queue)
{
  struct timeval tv;
  iot_logger_t *lc = svc->logger;
  const iot_data_t *allconf = svc->config.sdkconf;
  edgex_data_client_t *result = NULL;
  const char *cmdtopic;
  const char *reptopic;
  const char *mettopic;
  edc_redstr_conninfo *cinfo = calloc (1, sizeof (edc_redstr_conninfo));
  pthread_mutex_init (&cinfo->mtx, NULL);

  const char *host = iot_data_string_map_get_string (allconf, EX_BUS_HOST);
  uint16_t port = iot_data_ui16 (iot_data_string_map_get (allconf, EX_BUS_PORT));
  if (port == 0)
  {
    port = 6379;
  }

  iot_log_info (lc, "Event data will be sent through Redis streams at %s:%u", host, port);

  tv.tv_sec = tm->interval / 1000;
  tv.tv_usec = (tm->interval % 1000) * 1000;

  while (true)
  {
    uint64_t t1, t2;
    t1 = iot_time_msecs ();
    if (cinfo->ctx == NULL)
    {
      cinfo->ctx = edc_redstr_connect (lc, host, port, tv);
    }
    if (cinfo->ctx_rd == NULL)
    {
      cinfo->ctx_rd = edc_redstr_connect (lc, host, port, tv);
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
    edc_redstr_freefn (lc, cinfo);
    return NULL;
  }

  if (strcmp (iot_data_string_map_get_string (allconf, EX_BUS_AUTHMODE), "usernamepassword") == 0)
  {
    bool auth = true;
    iot_data_t *secrets = devsdk_get_secrets (svc, iot_data_string_map_get_string (allconf, EX_BUS_SECRETNAME));
    const char *pass = iot_data_string_map_get_string (secrets, "password");
    if (pass)
    {
      const char *user = iot_data_string_map_get_string (secrets, "username");
      auth = edc_redstr_auth (lc, cinfo->ctx, user, pass) && edc_redstr_auth (lc, cinfo->ctx_rd, user, pass);
    }
    iot_data_free (secrets);
    if (!auth)
    {
      edc_redstr_freefn (lc, cinfo);
      return NULL;
    }
  }

  cmdtopic = iot_data_string_map_get_string (allconf, EX_BUS_TOPIC_CMDREQ);
  if (!edc_redis_subscribe (lc, cinfo->ctx_rd, cmdtopic))
  {
    edc_redstr_freefn (lc, cinfo);
    return NULL;
  }

  reptopic = iot_data_string_map_get_string (allconf, EX_BUS_TOPIC_CMDRESP);
  cinfo->topicbase = strdup (iot_data_string_map_get_string (allconf, EX_BUS_TOPIC));
  cinfo->pubsub_topicbase = malloc (strlen (reptopic) + strlen (svc->name) + 2);
  sprintf (cinfo->pubsub_topicbase, "%s.%s", reptopic, svc->name);
  mettopic = svc->config.metrics.topic;
  cinfo->metric_topicbase = malloc (strlen (mettopic) + strlen (svc->name) + 3);
  sprintf (cinfo->metric_topicbase, "%s.%s.", mettopic, svc->name);
  edc_redstr_remapslash (cinfo->topicbase);
  edc_redstr_remapslash (cinfo->pubsub_topicbase);
  edc_redstr_remapslash (cinfo->metric_topicbase);
  cinfo->offset = strlen (cinfo->pubsub_topicbase) - strlen (cmdtopic) + sizeof ("/#");

  cinfo->svc = svc;
  cinfo->lc = lc;

  if (!iot_thread_create (&cinfo->thread, edc_redstr_listener, cinfo, IOT_THREAD_NO_PRIORITY, IOT_THREAD_NO_AFFINITY, lc))
  {
    edc_redstr_freefn (lc, cinfo);
    return NULL;
  }

  cinfo->running = true;
  result = malloc (sizeof (edgex_data_client_t));
  result->lc = lc;
  result->queue = queue;
  result->pf = edc_redstr_postfn;
  result->ff = edc_redstr_freefn;
  result->mf = edc_redstr_pubmetric;
  result->address = cinfo;
  return result;
}
