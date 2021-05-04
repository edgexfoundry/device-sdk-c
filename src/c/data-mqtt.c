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
#include <MQTTClient.h>

void edgex_mqtt_config_defaults (iot_data_t *allconf)
{
  iot_data_string_map_add (allconf, EX_MQ_PROTOCOL, iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (allconf, EX_MQ_HOST, iot_data_alloc_string ("localhost", IOT_DATA_REF));
  iot_data_string_map_add (allconf, EX_MQ_PORT, iot_data_alloc_ui16 (0));
  iot_data_string_map_add (allconf, EX_MQ_TOPIC, iot_data_alloc_string ("edgex/events", IOT_DATA_REF));

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
  MQTTClient client;
  char *uri;
  uint16_t qos;
  bool retained;
  const char *topicbase;
} edc_mqtt_conninfo;

static void edc_mqtt_freefn (iot_logger_t *lc, void *address)
{
  edc_mqtt_conninfo *cinfo = (edc_mqtt_conninfo *)address;
  MQTTClient_disconnect (cinfo->client, 10000);
  MQTTClient_destroy (&cinfo->client);
  free (cinfo->uri);
  free (address);
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
  MQTTClient_message pubmsg = MQTTClient_message_initializer;

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

  result = MQTTClient_publishMessage (cinfo->client, topic, &pubmsg, NULL);
  if (result != MQTTCLIENT_SUCCESS)
  {
    iot_log_error (lc, "Failed to post event to mqtt, error %d", result);
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

edgex_data_client_t *edgex_data_client_new_mqtt (const iot_data_t *allconf, iot_logger_t *lc, iot_threadpool_t *queue)
{
  int rc;
  char *uri;
  MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
  MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;

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
  sprintf (uri, "%s://%s:%" PRIu16, prot, host, iot_data_ui16 (iot_data_string_map_get (allconf, EX_MQ_PORT)));
  iot_log_info (lc, "Event data will be sent through MQTT at %s", uri);

  result->lc = lc;
  result->queue = queue;
  result->pf = edc_mqtt_postfn;
  result->ff = edc_mqtt_freefn;
  result->address = cinfo;

  cinfo->qos = iot_data_ui16 (iot_data_string_map_get (allconf, EX_MQ_QOS));
  cinfo->retained = iot_data_bool (iot_data_string_map_get (allconf, EX_MQ_RETAINED));
  cinfo->topicbase = iot_data_string_map_get_string (allconf, EX_MQ_TOPIC);
  cinfo->uri = uri;

  if ((rc = MQTTClient_create (&cinfo->client, uri, iot_data_string_map_get_string (allconf, EX_MQ_CLIENTID), MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
  {
    iot_log_error (lc, "Failed to create MQTT client, return code %d", rc);
    free (cinfo);
    free (result);
    free (uri);
    return NULL;
  }
  conn_opts.keepAliveInterval = iot_data_ui16 (iot_data_string_map_get (allconf, EX_MQ_KEEPALIVE));
  conn_opts.cleansession = 1;
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

  if ((rc = MQTTClient_connect (cinfo->client, &conn_opts)) != MQTTCLIENT_SUCCESS)
  {
    iot_log_error (lc, "Failed to connect MQTT, return code %d", rc);
    free (cinfo);
    free (result);
    free (uri);
    result = NULL;
  }
  return result;
}
