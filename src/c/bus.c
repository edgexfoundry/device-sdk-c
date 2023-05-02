/*
 * Copyright (c) 2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "bus.h"
#include "bus-impl.h"
#include "correlation.h"
#include "api.h"
#include <iot/base64.h>

typedef struct edgex_bus_endpoint_t
{
  char *base;
  iot_data_t *params;
  edgex_handler_fn handler;
  void *ctx;
} edgex_bus_endpoint_t;

static iot_data_t *edgex_bus_parse_tail (const char *tail)
{
  iot_data_t *result = iot_data_alloc_list ();
  while (*tail)
  {
    char *element;
    char *c = strchr (tail, '/');
    if (c)
    {
      element = strndup (tail, c - tail);
      tail = c + 1;
    }
    else
    {
      element = strdup (tail);
      tail += strlen (element);
    }
    iot_data_list_tail_push (result, iot_data_alloc_string (element, IOT_DATA_TAKE));
  }
  return result;
}

static edgex_handler_fn edgex_bus_match_handler (edgex_bus_t *bus, const char *path, iot_data_t *params, void **ctx)
{
  edgex_handler_fn h = NULL;
  iot_data_list_iter_t iter;
  pthread_mutex_lock (&bus->mtx);
  iot_data_list_iter (bus->handlers, &iter);
  while (iot_data_list_iter_next (&iter))
  {
    const edgex_bus_endpoint_t *ep = iot_data_address (iot_data_list_iter_value (&iter));
    if (strncmp (path, ep->base, strlen (ep->base)) == 0)
    {
      iot_data_t *tail = edgex_bus_parse_tail (path + strlen (ep->base));
      if (iot_data_list_length (tail) == iot_data_list_length (ep->params))
      {
        iot_data_list_iter_t keys;
        iot_data_list_iter_t vals;
        iot_data_list_iter (ep->params, &keys);
        iot_data_list_iter (tail, &vals);
        while (iot_data_list_iter_next (&keys))
        {
          iot_data_list_iter_next (&vals);
          iot_data_map_add (params, iot_data_add_ref (iot_data_list_iter_value (&keys)), iot_data_add_ref (iot_data_list_iter_value (&vals)));
        }
        h = ep->handler;
        *ctx = ep->ctx;
        iot_data_free (tail);
        break;
      }
    }
  }
  pthread_mutex_unlock (&bus->mtx);
  return h;
}

static void edgex_bus_endpoint_free (void *p)
{
  edgex_bus_endpoint_t *ep = (edgex_bus_endpoint_t *)p;
  free (ep->base);
  iot_data_free (ep->params);
  free (ep);
}

static char *edgex_data_to_b64 (const iot_data_t *src)
{
  char *json = iot_data_to_json (src);
  size_t sz = strlen (json) + 1;
  size_t encsz = iot_b64_encodesize (sz);
  char *result = malloc (encsz);
  iot_b64_encode (json, sz, result, encsz);
  free (json);
  return result;
}

void edgex_bus_post (edgex_bus_t *bus, const char *path, const iot_data_t *payload)
{
  iot_data_t *envelope = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_string_map_add (envelope, "correlationID", iot_data_alloc_string (edgex_device_get_crlid (), IOT_DATA_REF));
  iot_data_string_map_add (envelope, "apiVersion", iot_data_alloc_string (EDGEX_API_VERSION, IOT_DATA_REF));
  iot_data_string_map_add (envelope, "errorCode", iot_data_alloc_ui32 (0));
  iot_data_string_map_add (envelope, "contentType", iot_data_alloc_string ("application/json", IOT_DATA_REF));
  iot_data_string_map_add (envelope, "payload", iot_data_alloc_string (edgex_data_to_b64 (payload), IOT_DATA_TAKE));

  bus->postfn (bus->ctx, path, envelope);
  iot_data_free (envelope);
}

int edgex_bus_rmi (edgex_bus_t *bus, const char *path, const char *svcname, const iot_data_t *request, iot_data_t **reply)
{
  // NYI. as post, but include a request id in the envelope
  // register a handler for the reply if we dont already have one for that service
  // wait for a cond var
  return -1;
}

void edgex_bus_handle_request (edgex_bus_t *bus, const char *path, const char *envelope)
{
  void *ctx = NULL;
  iot_data_t *pathparams = iot_data_alloc_map (IOT_DATA_STRING);
  edgex_handler_fn h = edgex_bus_match_handler (bus, path, pathparams, &ctx);

  if (h)
  {
    int32_t status;
    const iot_data_t *crl = NULL;
    iot_data_t *req = NULL;
    iot_data_t *reply = NULL;
    iot_data_t *envdata = iot_data_from_json (envelope);
    const char *payload = iot_data_string_map_get_string (envdata, "payload");
    if (payload)
    {
      size_t sz = iot_b64_maxdecodesize (payload);
      char *json = malloc (sz + 1);
      iot_b64_decode (payload, json, &sz);
      json[sz] = '\0';
      req = iot_data_from_json (json);
      free (json);
    }
    crl = iot_data_string_map_get (envdata, "correlationID");
    if (crl)
    {
      edgex_device_alloc_crlid (iot_data_string (crl));
    }

    status = h (ctx, req, pathparams, iot_data_string_map_get (envdata, "queryParams"), &reply);

    if (reply)
    {
      char *rpath;
      const char *idstr;
      const iot_data_t *id;
      iot_data_t *renv = iot_data_alloc_map (IOT_DATA_STRING);
      iot_data_string_map_add (renv, "errorCode", iot_data_alloc_i32 (status));
      iot_data_string_map_add (renv, "contentType", iot_data_alloc_string ("application/json", IOT_DATA_REF));
      // XXX and if it's CBOR? - metadata on the reply should say so
      iot_data_string_map_add (renv, "payload", iot_data_alloc_string (edgex_data_to_b64 (reply), IOT_DATA_TAKE));
      iot_data_free (reply);
      iot_data_string_map_add (renv, "correlationID", iot_data_add_ref (crl));
      iot_data_string_map_add (renv, "apiVersion", iot_data_alloc_string (EDGEX_API_VERSION, IOT_DATA_REF));
      id = iot_data_string_map_get (envdata, "requestID");
      idstr = iot_data_string (id);
      // iot_data_string_map_add (renv, "requestID", iot_data_add_ref (id));
      rpath = edgex_bus_mktopic (bus, EDGEX_DEV_TOPIC_RESPONSE, idstr);
      bus->postfn (bus->ctx, rpath, renv);
      free (rpath);
      iot_data_free (renv);
    }
    if (crl)
    {
      edgex_device_free_crlid ();
    }
    iot_data_free (req);
    iot_data_free (envdata);
  }
  iot_data_free (pathparams);
}

char *edgex_bus_mktopic (edgex_bus_t *bus, const char *type, const char *param)
{
  char *result;
  size_t len = strlen (bus->prefix) + strlen (type) + strlen (bus->svcname) + strlen (param) + 4;
  result = malloc (len);
  strcpy (result, bus->prefix);
  strcat (result, "/");
  if (strlen (type))
  {
    strcat (result, type);
    strcat (result, "/");
  }
  strcat (result, bus->svcname);
  if (strlen (param))
  {
    strcat (result, "/");
    strcat (result, param);
  }
  return result;
}

void edgex_bus_init (edgex_bus_t *bus, const char *svcname, const iot_data_t *cfg)
{
  bus->prefix = strdup (iot_data_string_map_get_string (cfg, EX_BUS_TOPIC));
  bus->svcname = strdup (svcname);
  bus->handlers = iot_data_alloc_list ();
  pthread_mutex_init (&bus->mtx, NULL);
}

void edgex_bus_free (edgex_bus_t *bus)
{
  if (bus)
  {
    bus->freefn (bus->ctx);
    free (bus->prefix);
    free (bus->svcname);
    iot_data_free (bus->handlers);
    pthread_mutex_destroy (&bus->mtx);
    free (bus);
  }
}

void edgex_bus_register_handler (edgex_bus_t *bus, const char *path, void *ctx, edgex_handler_fn handler)
{
  char *sub;
  edgex_bus_endpoint_t *entry = malloc (sizeof (edgex_bus_endpoint_t));
  entry->params = iot_data_alloc_list ();
  const char *param = strchr (path, '{');
  if (param)
  {
    size_t plen = param - path;
    entry->base = strndup (path, plen);
    sub = strndup (path, plen + 1);
    sub[plen] = '#';
    while (param)
    {
      char *end = strchr (param, '}');
      iot_data_list_tail_push (entry->params, iot_data_alloc_string (strndup (param + 1, end - param - 1), IOT_DATA_TAKE));
      param = strchr (end, '{');
    }
  }
  else
  {
    sub = strdup (path);
    entry->base = strdup (path);
  }
  entry->handler = handler;
  entry->ctx = ctx;
  pthread_mutex_lock (&bus->mtx);
  iot_data_list_head_push (bus->handlers, iot_data_alloc_pointer (entry, edgex_bus_endpoint_free));
  pthread_mutex_unlock (&bus->mtx);
  bus->subsfn (bus->ctx, sub);
  free (sub);
}

void edgex_bus_config_defaults (iot_data_t *allconf, const char *svcname)
{
  iot_data_string_map_add (allconf, EX_BUS_DISABLED, iot_data_alloc_bool (false));
  iot_data_string_map_add (allconf, EX_BUS_PROTOCOL, iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (allconf, EX_BUS_HOST, iot_data_alloc_string ("localhost", IOT_DATA_REF));
  iot_data_string_map_add (allconf, EX_BUS_PORT, iot_data_alloc_ui16 (0));
  iot_data_string_map_add (allconf, EX_BUS_TOPIC, iot_data_alloc_string ("edgex", IOT_DATA_REF));
  iot_data_string_map_add (allconf, EX_BUS_AUTHMODE, iot_data_alloc_string ("none", IOT_DATA_REF));
  iot_data_string_map_add (allconf, EX_BUS_SECRETNAME, iot_data_alloc_string ("", IOT_DATA_REF));

  iot_data_string_map_add (allconf, EX_BUS_CLIENTID, iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (allconf, EX_BUS_QOS, iot_data_alloc_ui16 (0));
  iot_data_string_map_add (allconf, EX_BUS_KEEPALIVE, iot_data_alloc_ui16 (60));
  iot_data_string_map_add (allconf, EX_BUS_RETAINED, iot_data_alloc_bool (false));
  iot_data_string_map_add (allconf, EX_BUS_CERTFILE, iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (allconf, EX_BUS_KEYFILE, iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (allconf, EX_BUS_SKIPVERIFY, iot_data_alloc_bool (false));
}

JSON_Value *edgex_bus_config_json (const iot_data_t *allconf)
{
  JSON_Value *busval = json_value_init_object ();
  JSON_Object *busobj = json_value_get_object (busval);
  json_object_set_string (busobj, "Protocol", iot_data_string_map_get_string (allconf, EX_BUS_PROTOCOL));
  json_object_set_string (busobj, "Host", iot_data_string_map_get_string (allconf, EX_BUS_HOST));
  json_object_set_uint (busobj, "Port", iot_data_ui16 (iot_data_string_map_get (allconf, EX_BUS_PORT)));
  json_object_set_string (busobj, "Topic", iot_data_string_map_get_string (allconf, EX_BUS_TOPIC));
  json_object_set_string (busobj, "AuthMode", iot_data_string_map_get_string (allconf, EX_BUS_AUTHMODE));
  json_object_set_string (busobj, "SecretName", iot_data_string_map_get_string (allconf, EX_BUS_SECRETNAME));

  JSON_Value *optval = json_value_init_object ();
  JSON_Object *optobj = json_value_get_object (optval);
  json_object_set_string (optobj, "ClientId", iot_data_string_map_get_string (allconf, EX_BUS_CLIENTID));
  json_object_set_number (optobj, "Qos", iot_data_ui16 (iot_data_string_map_get (allconf, EX_BUS_QOS)));
  json_object_set_number (optobj, "KeepAlive", iot_data_ui16 (iot_data_string_map_get (allconf, EX_BUS_KEEPALIVE)));
  json_object_set_boolean (optobj, "Retained", iot_data_bool (iot_data_string_map_get (allconf, EX_BUS_RETAINED)));
  json_object_set_string (optobj, "CertFile", iot_data_string_map_get_string (allconf, EX_BUS_CERTFILE));
  json_object_set_string (optobj, "KeyFile", iot_data_string_map_get_string (allconf, EX_BUS_KEYFILE));
  json_object_set_boolean (optobj, "SkipCertVerify", iot_data_bool (iot_data_string_map_get (allconf, EX_BUS_SKIPVERIFY)));
  json_object_set_value (busobj, "Optional", optval);

  return busval;
}
