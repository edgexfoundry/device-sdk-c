/*
 * Copyright (c) 2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "data-redstr.h"
#include "correlation.h"
#include "iot/base64.h"
#include "iot/time.h"
#include <hiredis/hiredis.h>

JSON_Value *edgex_redstr_config_json (const iot_data_t *allconf)
{
  JSON_Value *mqval = json_value_init_object ();
  JSON_Object *mqobj = json_value_get_object (mqval);
  json_object_set_string (mqobj, "Host", iot_data_string_map_get_string (allconf, EX_MQ_HOST));
  json_object_set_uint (mqobj, "Port", iot_data_ui16 (iot_data_string_map_get (allconf, EX_MQ_PORT)));
  json_object_set_string (mqobj, "Topic", iot_data_string_map_get_string (allconf, EX_MQ_TOPIC));

  return mqval;
}

typedef struct edc_redstr_conninfo
{
  redisContext *ctx;
  char *topicbase;
  pthread_mutex_t mtx;
} edc_redstr_conninfo;

static void edc_redstr_freefn (iot_logger_t *lc, void *address)
{
  edc_redstr_conninfo *cinfo = (edc_redstr_conninfo *)address;
  redisFree (cinfo->ctx);
  free (cinfo->topicbase);
  pthread_mutex_destroy (&cinfo->mtx);
  free (cinfo);
}

static void remapslash (char *c)
{
  while ((c = strchr (c, '/')))
  {
    *c = '.';
  }
}

static void edc_redstr_postfn (iot_logger_t *lc, void *address, edgex_event_cooked *event)
{
  devsdk_http_reply h;
  const char *crl;
  bool freecrl = false;
  size_t payloadsz;
  char *payload;
  char *json;
  char *topic;
  void *reply;
  edc_redstr_conninfo *cinfo = (edc_redstr_conninfo *)address;

  topic = malloc (strlen (cinfo->topicbase) + strlen (event->path) + 2);
  strcpy (topic, cinfo->topicbase);
  strcat (topic, ".");
  strcat (topic, event->path);
  remapslash (topic);
  edgex_event_cooked_write (event, &h);

  crl = edgex_device_get_crlid ();
  if (!crl)
  {
    edgex_device_alloc_crlid (NULL);
    freecrl = true;
    crl = edgex_device_get_crlid ();
  }

  payloadsz = iot_b64_encodesize (h.data.size);
  payload = malloc (payloadsz);
  iot_b64_encode (h.data.bytes, h.data.size, payload, payloadsz);

  JSON_Value *val = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (val);
  json_object_set_string (obj, "CorrelationID", crl);
  json_object_set_string (obj, "Payload", payload);
  json_object_set_string (obj, "ContentType", h.content_type);

  json = json_serialize_to_string (val);

  pthread_mutex_lock (&cinfo->mtx);
  reply = redisCommand (cinfo->ctx, "PUBLISH %s %s", topic, json);

  if (reply == NULL)
  {
    iot_log_error (lc, "Error posting Event via Redis: %s", cinfo->ctx->errstr);
    if (redisReconnect (cinfo->ctx) == REDIS_ERR)
    {
      iot_log_error (lc, "Redis reconnection failed: %s", cinfo->ctx->errstr);
    }
  }

  freeReplyObject (reply);
  pthread_mutex_unlock (&cinfo->mtx);
  free (h.data.bytes);
  free (payload);
  json_free_serialized_string (json);
  free (topic);
  json_value_free (val);
  if (freecrl)
  {
    edgex_device_free_crlid ();
  }
}

edgex_data_client_t *edgex_data_client_new_redstr (const iot_data_t *allconf, iot_logger_t *lc, const devsdk_timeout *tm, iot_threadpool_t *queue)
{
  struct timeval tv;
  edgex_data_client_t *result = malloc (sizeof (edgex_data_client_t));
  edc_redstr_conninfo *cinfo = malloc (sizeof (edc_redstr_conninfo));

  const char *host = iot_data_string_map_get_string (allconf, EX_MQ_HOST);
  uint16_t port = iot_data_ui16 (iot_data_string_map_get (allconf, EX_MQ_PORT));
  if (port == 0)
  {
    port = 6379;
  }

  iot_log_info (lc, "Event data will be sent through Redis streams at %s:%u", host, port);

  result->lc = lc;
  result->queue = queue;
  result->pf = edc_redstr_postfn;
  result->ff = edc_redstr_freefn;
  result->address = cinfo;

  tv.tv_sec = tm->interval / 1000;
  tv.tv_usec = (tm->interval % 1000) * 1000;

  while (true)
  {
    uint64_t t1, t2;
    t1 = iot_time_msecs ();
    cinfo->ctx = redisConnectWithTimeout (host, port, tv);
    if (cinfo->ctx && (cinfo->ctx->err == 0))
    {
      break;
    }
    if (cinfo->ctx)
    {
      iot_log_error (lc, "Failed to create Redis Streams client: %s", cinfo->ctx->errstr);
      redisFree (cinfo->ctx);
    }
    else
    {
      iot_log_error (lc, "Can't allocate redis context");
    }
    t2 = iot_time_msecs ();
    if (t2 > tm->deadline - tm->interval)
    {
      return false;
    }
    if (tm->interval > t2 - t1)
    {
      devsdk_wait_msecs (tm->interval - (t2 - t1));
    }
  }
  if (cinfo->ctx == NULL || cinfo->ctx->err)
  {
    free (cinfo);
    free (result);
    return NULL;
  }

  cinfo->topicbase = strdup (iot_data_string_map_get_string (allconf, EX_MQ_TOPIC));
  remapslash (cinfo->topicbase);

  pthread_mutex_init (&cinfo->mtx, NULL);
  return result;
}
