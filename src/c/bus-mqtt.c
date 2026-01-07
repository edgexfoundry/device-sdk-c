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
#include <MQTTAsync.h>

typedef struct edgex_bus_mqtt_t
{
  iot_logger_t *lc;
  char *uri;
  MQTTAsync client;
  uint16_t qos;
  bool retained;
  bool connected;
  pthread_mutex_t mtx;
  pthread_cond_t cond;
} edgex_bus_mqtt_t;

static void edgex_bus_mqtt_free (void *ctx)
{
  edgex_bus_mqtt_t *cinfo = (edgex_bus_mqtt_t *)ctx;
  MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
  opts.context = cinfo->client;
  MQTTAsync_disconnect (cinfo->client, &opts);
  MQTTAsync_destroy (&cinfo->client);
  pthread_cond_destroy (&cinfo->cond);
  pthread_mutex_destroy (&cinfo->mtx);
  free (cinfo->uri);
  free (cinfo);
}

static void edgex_bus_mqtt_onsend (void *context, MQTTAsync_successData *response)
{
  edgex_bus_mqtt_t *cinfo = (edgex_bus_mqtt_t *)context;
  iot_log_trace (cinfo->lc, "mqtt: published");
}

static void edgex_bus_mqtt_onsendfail (void *context, MQTTAsync_failureData *response)
{
  edgex_bus_mqtt_t *cinfo = (edgex_bus_mqtt_t *)context;
  if (response->message)
  {
    iot_log_error (cinfo->lc, "mqtt: publish failed: %s (code %d)", response->message, response->code);
  }
  else
  {
    iot_log_error (cinfo->lc, "mqtt: publish failed, error code %d", response->code);
  }
}

static void edgex_bus_mqtt_subscribe (void *ctx, const char *topic)
{
  edgex_bus_mqtt_t *cinfo = (edgex_bus_mqtt_t *)ctx;
  int rc;
  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
  MQTTSubscribe_options subOpts = MQTTSubscribe_options_initializer;

  iot_log_debug (cinfo->lc, "mqtt: subscribing to %s", topic);
  subOpts.noLocal = 1;
  opts.context = ctx;
  opts.subscribeOptions = subOpts;
  rc = MQTTAsync_subscribe (cinfo->client, topic, cinfo->qos, &opts);
  if (rc != MQTTASYNC_SUCCESS)
  {
    iot_log_error (cinfo->lc, "mqtt: unable to subscribe to %s, error code %d", topic, rc);
  }
  else
  {
    rc = MQTTAsync_waitForCompletion(cinfo->client, opts.token, 1000);
    if (rc != MQTTASYNC_SUCCESS)
    {
      iot_log_error(cinfo->lc, "mqtt: subscribe to %s failed, error code %d", topic, rc);
    }
  }
}

static void edgex_bus_mqtt_post (void *ctx, const char *topic, const iot_data_t *envelope, bool use_cbor)
{
  edgex_bus_mqtt_t *cinfo = (edgex_bus_mqtt_t *)ctx;
  int result;
  MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
  char *data = NULL;
  uint32_t datasz = 0;
  iot_data_t *cbor = NULL;
  if (use_cbor)
  {
    cbor = iot_data_to_cbor (envelope);
    data = iot_data_binary_take(cbor, &datasz);
  }
  else
  {
    data = iot_data_to_json (envelope);
    datasz = strlen (data);
  }
  pubmsg.payload = data;
  pubmsg.payloadlen = datasz;
  pubmsg.qos = cinfo->qos;
  pubmsg.retained = cinfo->retained;
  opts.context = cinfo;
  opts.onSuccess = edgex_bus_mqtt_onsend;
  opts.onFailure = edgex_bus_mqtt_onsendfail;
  iot_log_trace (cinfo->lc, "mqtt: publish to topic %s", topic);
  result = MQTTAsync_sendMessage (cinfo->client, topic, &pubmsg, &opts);
  if (result != MQTTASYNC_SUCCESS)
  {
    iot_log_error (cinfo->lc, "mqtt: failed to post event, error %d", result);
  }
  if (cbor)
  {
    iot_data_free (cbor);
  }
  free(data);
}

static void edgex_bus_mqtt_onconnect(void *context, MQTTAsync_successData *response)
{
  edgex_bus_mqtt_t *cinfo = (edgex_bus_mqtt_t *)context;
  iot_log_info (cinfo->lc, "mqtt: connected");
  cinfo->connected = true;
  pthread_mutex_lock (&cinfo->mtx);
  pthread_cond_signal (&cinfo->cond);
  pthread_mutex_unlock (&cinfo->mtx);
}

static void edgex_bus_mqtt_onconnectfail (void *context, MQTTAsync_failureData *response)
{
  edgex_bus_mqtt_t *cinfo = (edgex_bus_mqtt_t *)context;
  if (response->message)
  {
    iot_log_error (cinfo->lc, "mqtt: connect failed: %s (code %d)", response->message, response->code);
  }
  else
  {
    iot_log_error (cinfo->lc, "mqtt: connect failed, error code %d", response->code);
  }
}

static int edgex_bus_mqtt_msgarrvd (void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
  edgex_bus_t *bus = (edgex_bus_t *)context;
  char *topic = topicName;

  if (topicLen != 0) // Indicates topic string not terminated
  {
    topic = strndup (topicName, topicLen);
  }

  edgex_bus_handle_request (bus, topic, message->payload, message->payloadlen);

  if (topic != topicName)
  {
    free(topic);
  }
  MQTTAsync_freeMessage (&message);
  MQTTAsync_free (topicName);
  return 1;
}

edgex_bus_t *edgex_bus_create_mqtt (iot_logger_t *lc, const char *svcname, const iot_data_t *cfg, edgex_secret_provider_t *secstore, iot_threadpool_t *queue, const devsdk_timeout *tm)
{
  int rc;
  struct timespec max_wait;
  int timedout = 0;
  iot_data_t *secrets = NULL;
  MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
  MQTTAsync_SSLOptions ssl_opts = MQTTAsync_SSLOptions_initializer;
  MQTTAsync_createOptions create_opts = MQTTAsync_createOptions_initializer;
  edgex_bus_t *result;
  edgex_bus_mqtt_t *cinfo = calloc (1, sizeof (edgex_bus_mqtt_t));

  cinfo->lc = lc;

  const char *host = iot_data_string_map_get_string (cfg, EX_BUS_HOST);
  const char *prot = iot_data_string_map_get_string (cfg, EX_BUS_PROTOCOL);
  const char *certfile = iot_data_string_map_get_string (cfg, EX_BUS_CERTFILE);
  const char *keyfile = iot_data_string_map_get_string (cfg, EX_BUS_KEYFILE);
  uint16_t port = iot_data_ui16 (iot_data_string_map_get (cfg, EX_BUS_PORT));
  if (*prot == '\0' || strcmp (prot, "mqtt") == 0 || strcmp (prot, "tcp") == 0)
  {
    prot = "tcp";
  }
  else if (strcmp (prot, "ssl") == 0 || strcmp (prot, "tls") == 0 ||
           strcmp (prot, "mqtts") == 0 || strcmp (prot, "mqtt+ssl") == 0 ||
           strcmp (prot, "tcps") == 0)
  {
    prot = "ssl";
  }
  else
  {
    iot_log_error (lc, "mqtt: unsupported protocol: %s", prot);
    free (cinfo);
    return NULL;
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

  cinfo->uri = malloc (strlen (host) + strlen (prot) + 10);
  sprintf (cinfo->uri, "%s://%s:%" PRIu16, prot, host, port);
  iot_log_info (lc, "Message Bus is set to MQTT at %s", cinfo->uri);

  cinfo->qos = iot_data_ui16 (iot_data_string_map_get (cfg, EX_BUS_QOS));
  cinfo->retained = iot_data_bool (iot_data_string_map_get (cfg, EX_BUS_RETAINED));

  create_opts.sendWhileDisconnected = 1;
  rc = MQTTAsync_createWithOptions
    (&cinfo->client, cinfo->uri, iot_data_string_map_get_string (cfg, EX_BUS_CLIENTID), MQTTCLIENT_PERSISTENCE_NONE, NULL, &create_opts);
  if (rc != MQTTASYNC_SUCCESS)
  {
    iot_log_error (lc, "mqtt: failed to create client, return code %d", rc);
    free (cinfo->uri);
    free (cinfo);
    return NULL;
  }
  result = malloc (sizeof (edgex_bus_t));
  MQTTAsync_setCallbacks (cinfo->client, result, NULL, edgex_bus_mqtt_msgarrvd, NULL);
  conn_opts.keepAliveInterval = iot_data_ui16 (iot_data_string_map_get (cfg, EX_BUS_KEEPALIVE));
  conn_opts.cleansession = 1;
  conn_opts.automaticReconnect = 1;
  conn_opts.onSuccess = edgex_bus_mqtt_onconnect;
  conn_opts.onFailure = edgex_bus_mqtt_onconnectfail;
  conn_opts.context = cinfo;

  if (strcmp (iot_data_string_map_get_string (cfg, EX_BUS_AUTHMODE), "usernamepassword") == 0)
  {
    secrets = edgex_secrets_get (secstore, iot_data_string_map_get_string (cfg, EX_BUS_SECRETNAME));
    conn_opts.username = iot_data_string_map_get_string (secrets, "username");
    conn_opts.password = iot_data_string_map_get_string (secrets, "password");
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
  ssl_opts.verify = iot_data_string_map_get_bool (cfg, EX_BUS_SKIPVERIFY, false) ? 0 : 1;

  pthread_mutex_init (&cinfo->mtx, NULL);
  pthread_cond_init (&cinfo->cond, NULL);
  while (true)
  {
    uint64_t t1, t2;

    t1 = iot_time_msecs ();

    if (tm->deadline <= t1)
    {
      break;
    }
    max_wait.tv_sec = tm->deadline / 1000;
    max_wait.tv_nsec = 1000000 * (tm->deadline % 1000);

    pthread_mutex_lock (&cinfo->mtx);
    if ((rc = MQTTAsync_connect (cinfo->client, &conn_opts)) == MQTTASYNC_SUCCESS)
    {
      timedout = pthread_cond_timedwait (&cinfo->cond, &cinfo->mtx, &max_wait);
    }
    pthread_mutex_unlock (&cinfo->mtx);
    if (cinfo->connected)
    {
      break;
    }
    else
    {
      if (timedout == ETIMEDOUT)
      {
        iot_log_error (lc, "mqtt: failed to connect, timed out");
      }
      else
      {
        iot_log_error (lc, "mqtt: failed to connect, return code %d", rc);
      }
      t2 = iot_time_msecs ();
      if (t2 > tm->deadline - tm->interval)
      {
        break;
      }
      if (tm->interval > t2 - t1)
      {
        iot_wait_msecs (tm->interval - (t2 - t1));
      }
    }
  }
  iot_data_free (secrets);
  pthread_cond_destroy (&cinfo->cond);
  pthread_mutex_destroy (&cinfo->mtx);

  if (cinfo->connected)
  {
    edgex_bus_init (result, svcname, cfg);
    result->ctx = cinfo;
    result->postfn = edgex_bus_mqtt_post;
    result->freefn = edgex_bus_mqtt_free;
    result->subsfn = edgex_bus_mqtt_subscribe;
  }
  else
  {
    MQTTAsync_destroy (&cinfo->client);
    free (cinfo->uri);
    free (cinfo);
    free (result);
    result = NULL;
  }

  return result;
}
