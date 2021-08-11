/*
 * Copyright (c) 2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "data-mqtt.h"
#include "correlation.h"
#include "iot/base64.h"
#include "iot/time.h"
#include <MQTTAsync.h>

#define PTHREAD_COND_TIMEOUT 99999

void edgex_mqtt_config_defaults (iot_data_t *allconf)
{
  iot_data_string_map_add (allconf, EX_MQ_PROTOCOL, iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (allconf, EX_MQ_HOST, iot_data_alloc_string ("localhost", IOT_DATA_REF));
  iot_data_string_map_add (allconf, EX_MQ_PORT, iot_data_alloc_ui16 (0));
  iot_data_string_map_add (allconf, EX_MQ_TOPIC, iot_data_alloc_string ("edgex/events/device", IOT_DATA_REF));

  iot_data_string_map_add (allconf, EX_MQ_USERNAME, iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (allconf, EX_MQ_PASSWORD, iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (allconf, EX_MQ_CLIENTID, iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (allconf, EX_MQ_QOS, iot_data_alloc_ui16 (0));
  iot_data_string_map_add (allconf, EX_MQ_KEEPALIVE, iot_data_alloc_ui16 (60));
  iot_data_string_map_add (allconf, EX_MQ_RETAINED, iot_data_alloc_bool (false));
  iot_data_string_map_add (allconf, EX_MQ_CERTFILE, iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (allconf, EX_MQ_KEYFILE, iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (allconf, EX_MQ_SKIPVERIFY, iot_data_alloc_bool (false));
}

JSON_Value *edgex_mqtt_config_json (const iot_data_t *allconf)
{
  JSON_Value *mqval = json_value_init_object ();
  JSON_Object *mqobj = json_value_get_object (mqval);
  json_object_set_string (mqobj, "Protocol", iot_data_string_map_get_string (allconf, EX_MQ_PROTOCOL));
  json_object_set_string (mqobj, "Host", iot_data_string_map_get_string (allconf, EX_MQ_HOST));
  json_object_set_uint (mqobj, "Port", iot_data_ui16 (iot_data_string_map_get (allconf, EX_MQ_PORT)));
  json_object_set_string (mqobj, "Topic", iot_data_string_map_get_string (allconf, EX_MQ_TOPIC));

  JSON_Value *optval = json_value_init_object ();
  JSON_Object *optobj = json_value_get_object (optval);
  json_object_set_string (optobj, "Username", iot_data_string_map_get_string (allconf, EX_MQ_USERNAME));
  json_object_set_string (optobj, "Password", iot_data_string_map_get_string (allconf, EX_MQ_PASSWORD));
  json_object_set_string (optobj, "ClientId", iot_data_string_map_get_string (allconf, EX_MQ_CLIENTID));
  json_object_set_number (optobj, "Qos", iot_data_ui16 (iot_data_string_map_get (allconf, EX_MQ_QOS)));
  json_object_set_number (optobj, "KeepAlive", iot_data_ui16 (iot_data_string_map_get (allconf, EX_MQ_KEEPALIVE)));
  json_object_set_boolean (optobj, "Retained", iot_data_bool (iot_data_string_map_get (allconf, EX_MQ_RETAINED)));
  json_object_set_string (optobj, "CertFile", iot_data_string_map_get_string (allconf, EX_MQ_CERTFILE));
  json_object_set_string (optobj, "KeyFile", iot_data_string_map_get_string (allconf, EX_MQ_KEYFILE));
  json_object_set_boolean (optobj, "SkipCertVerify", iot_data_bool (iot_data_string_map_get (allconf, EX_MQ_SKIPVERIFY)));

  json_object_set_value (mqobj, "Optional", optval);
  return mqval;
}

typedef struct edc_mqtt_conninfo
{
  MQTTAsync client;
  char *uri;
  iot_logger_t *lc;
  pthread_mutex_t mtx;
  pthread_cond_t cond;
  uint16_t qos;
  bool retained;
  const char *topicbase;
} edc_mqtt_conninfo;

static void edc_mqtt_freefn (iot_logger_t *lc, void *address)
{
  edc_mqtt_conninfo *cinfo = (edc_mqtt_conninfo *)address;
  MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
  opts.context = cinfo->client;
  MQTTAsync_disconnect (cinfo->client, &opts);
  MQTTAsync_destroy (&cinfo->client);
  free (cinfo->uri);
  pthread_cond_destroy (&cinfo->cond);
  pthread_mutex_destroy (&cinfo->mtx);
  free (address);
}

static void edc_mqtt_onsend (void *context, MQTTAsync_successData *response)
{
  edc_mqtt_conninfo *cinfo = (edc_mqtt_conninfo *)context;
  iot_log_debug (cinfo->lc, "mqtt: published event");
}

static void edc_mqtt_onsendfail (void *context, MQTTAsync_failureData *response)
{
  edc_mqtt_conninfo *cinfo = (edc_mqtt_conninfo *)context;
  iot_log_error (cinfo->lc, "mqtt: publish failed, error code %d", response->code);
}

static void edc_mqtt_postfn (iot_logger_t *lc, void *address, edgex_event_cooked *event)
{
  devsdk_http_reply h;
  int result;
  const char *crl;
  bool freecrl = false;
  size_t payloadsz;
  char *payload;
  char *json;
  char *topic;
  edc_mqtt_conninfo *cinfo = (edc_mqtt_conninfo *)address;
  MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;

  topic = malloc (strlen (cinfo->topicbase) + strlen (event->path) + 2);
  strcpy (topic, cinfo->topicbase);
  strcat (topic, "/");
  strcat (topic, event->path);

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
  json_object_set_string (obj, "Checksum", "");
  json_object_set_string (obj, "CorrelationID", crl);
  json_object_set_string (obj, "Payload", payload);
  json_object_set_string (obj, "ContentType", h.content_type);

  json = json_serialize_to_string (val);
  pubmsg.payload = json;
  pubmsg.payloadlen = strlen (json) + 1;
  pubmsg.qos = cinfo->qos;
  pubmsg.retained = cinfo->retained;
  opts.context = cinfo;
  opts.onSuccess = edc_mqtt_onsend;
  opts.onFailure = edc_mqtt_onsendfail;
  iot_log_trace (lc, "mqtt: publish event to topic %s", topic);
  result = MQTTAsync_sendMessage (cinfo->client, topic, &pubmsg, &opts);
  if (result != MQTTASYNC_SUCCESS)
  {
    iot_log_error (lc, "mqtt: failed to post event, error %d", result);
  }

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

static void edc_mqtt_onconnect (void *context, MQTTAsync_successData *response)
{
  edc_mqtt_conninfo *cinfo = (edc_mqtt_conninfo *)context;
  iot_log_info (cinfo->lc, "mqtt: connected");
  pthread_mutex_lock (&cinfo->mtx);
  pthread_cond_signal (&cinfo->cond);
  pthread_mutex_unlock (&cinfo->mtx);
}

static void edc_mqtt_onconnectfail (void *context, MQTTAsync_failureData *response)
{
  edc_mqtt_conninfo *cinfo = (edc_mqtt_conninfo *)context;
  iot_log_error (cinfo->lc, "mqtt: connect failed, error code %d", response->code);
}

edgex_data_client_t *edgex_data_client_new_mqtt (const iot_data_t *allconf, iot_logger_t *lc, iot_threadpool_t *queue)
{
  int rc;
  char *uri;
  uint64_t tm;
  struct timespec max_wait;
  int timedout;
  MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
  MQTTAsync_SSLOptions ssl_opts = MQTTAsync_SSLOptions_initializer;
  MQTTAsync_createOptions create_opts = MQTTAsync_createOptions_initializer;

  edgex_data_client_t *result = malloc (sizeof (edgex_data_client_t));
  edc_mqtt_conninfo *cinfo = malloc (sizeof (edc_mqtt_conninfo));

  const char *host = iot_data_string_map_get_string (allconf, EX_MQ_HOST);
  const char *prot = iot_data_string_map_get_string (allconf, EX_MQ_PROTOCOL);
  const char *user = iot_data_string_map_get_string (allconf, EX_MQ_USERNAME);
  const char *pass = iot_data_string_map_get_string (allconf, EX_MQ_PASSWORD);
  const char *certfile = iot_data_string_map_get_string (allconf, EX_MQ_CERTFILE);
  const char *keyfile = iot_data_string_map_get_string (allconf, EX_MQ_KEYFILE);
  uint16_t port = iot_data_ui16 (iot_data_string_map_get (allconf, EX_MQ_PORT));
  if (*prot == '\0')
  {
    prot = "tcp";
  }
  if (port == 0)
  {
    if (strcmp (prot, "ssl") == 0)
    {
      port = 8883;
    }
    else
    {
      port = 1883;
    }
  }

  uri = malloc (strlen (host) + strlen (prot) + 10);
  sprintf (uri, "%s://%s:%" PRIu16, prot, host, port);
  iot_log_info (lc, "Event data will be sent through MQTT at %s", uri);

  cinfo->lc = lc;
  result->lc = lc;
  result->queue = queue;
  result->pf = edc_mqtt_postfn;
  result->ff = edc_mqtt_freefn;
  result->address = cinfo;

  cinfo->qos = iot_data_ui16 (iot_data_string_map_get (allconf, EX_MQ_QOS));
  cinfo->retained = iot_data_bool (iot_data_string_map_get (allconf, EX_MQ_RETAINED));
  cinfo->topicbase = iot_data_string_map_get_string (allconf, EX_MQ_TOPIC);
  cinfo->uri = uri;

  create_opts.sendWhileDisconnected = 1;
  rc = MQTTAsync_createWithOptions
    (&cinfo->client, uri, iot_data_string_map_get_string (allconf, EX_MQ_CLIENTID), MQTTCLIENT_PERSISTENCE_NONE, NULL, &create_opts);
  if (rc != MQTTASYNC_SUCCESS)
  {
    iot_log_error (lc, "mqtt: failed to create client, return code %d", rc);
    free (cinfo);
    free (result);
    free (uri);
    return NULL;
  }
  conn_opts.keepAliveInterval = iot_data_ui16 (iot_data_string_map_get (allconf, EX_MQ_KEEPALIVE));
  conn_opts.cleansession = 1;
  conn_opts.automaticReconnect = 1;
  conn_opts.onSuccess = edc_mqtt_onconnect;
  conn_opts.onFailure = edc_mqtt_onconnectfail;
  conn_opts.context = cinfo;
  if (strlen (user))
  {
    conn_opts.username = user;
  }
  if (strlen (pass))
  {
    conn_opts.password = pass;
  }
  conn_opts.ssl = &ssl_opts;
  if (strlen (certfile))
  {
    ssl_opts.trustStore = certfile;
  }
  if (strlen (keyfile))
  {
    ssl_opts.keyStore = keyfile;
  }
  ssl_opts.verify = iot_data_string_map_get_bool (allconf, EX_MQ_SKIPVERIFY, false) ? 0 : 1;

#if 0	// Fix device-service issue #361
  tm = iot_data_ui32 (iot_data_string_map_get (allconf, "Service/Timeout"));
  tm *= iot_data_ui32 (iot_data_string_map_get (allconf, "Service/ConnectRetries"));
  tm += iot_time_msecs ();
  max_wait.tv_sec = tm / 1000;
  max_wait.tv_nsec = 1000000 * (tm % 1000);
#else
	clock_gettime(CLOCK_REALTIME, &max_wait);
	max_wait.tv_sec += PTHREAD_COND_TIMEOUT;
#endif
  
  pthread_mutex_init (&cinfo->mtx, NULL);
  pthread_cond_init (&cinfo->cond, NULL);

  pthread_mutex_lock (&cinfo->mtx);
  if ((rc = MQTTAsync_connect (cinfo->client, &conn_opts)) == MQTTASYNC_SUCCESS)
  {
    timedout = pthread_cond_timedwait (&cinfo->cond, &cinfo->mtx, &max_wait);
  }
  pthread_mutex_unlock (&cinfo->mtx);
  if (rc != MQTTASYNC_SUCCESS || timedout == ETIMEDOUT)
  {
    if (rc == MQTTASYNC_SUCCESS)
    {
      iot_log_error (lc, "mqtt: failed to connect, timed out");
    }
    else
    {
      iot_log_error (lc, "mqtt: failed to connect, return code %d", rc);
    }
    pthread_cond_destroy (&cinfo->cond);
    pthread_mutex_destroy (&cinfo->mtx);
    free (cinfo);
    free (result);
    free (uri);
    result = NULL;
  }
  return result;
}
