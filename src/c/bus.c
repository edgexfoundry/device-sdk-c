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
  size_t base_len;
  bool ignore_tail;
} edgex_bus_endpoint_t;

static iot_data_t *edgex_bus_parse_tail (const char *tail, const edgex_bus_endpoint_t *ep)
{
  iot_data_t *result = iot_data_alloc_list();
  if (ep->ignore_tail)
  {
    return result;
  }
  tail += ep->base_len;
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

static edgex_handler_fn edgex_bus_match_handler(edgex_bus_t *bus, const char *path, iot_data_t *params, void **ctx)
{
  edgex_handler_fn h = NULL;
  iot_data_list_iter_t iter;
  pthread_mutex_lock (&bus->mtx);
  iot_data_list_iter (bus->handlers, &iter);
  while (iot_data_list_iter_next (&iter))
  {
    const edgex_bus_endpoint_t *ep = iot_data_address (iot_data_list_iter_value (&iter));
    if (strncmp (path, ep->base, ep->base_len) == 0)
    {
      iot_data_t *tail = edgex_bus_parse_tail (path, ep);
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
      iot_data_free (tail);
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

static char *edgex_data_to_b64 (const iot_data_t *src, bool use_cbor)
{
  char *data = NULL;
  uint32_t sz = 0;
  iot_data_t *cbor = NULL;
  if (use_cbor)
  {
    cbor = iot_data_to_cbor (src);
    if (cbor)
    {
      data = iot_data_binary_take(cbor, &sz);
    }
  }
  else
  {
    data = iot_data_to_json (src);
    sz = strlen (data); // ignore the last null character, which causes an unmarshal error in core-data
  }
  size_t encsz = iot_b64_encodesize (sz);
  char *result = malloc (encsz);
  iot_b64_encode (data, sz, result, encsz);
  iot_data_free (cbor);
  free (data);
  return result;
}

void edgex_bus_post (edgex_bus_t *bus, const char *path, const iot_data_t *payload, bool event_is_cbor)
{
  iot_data_t *envelope = iot_data_alloc_map (IOT_DATA_STRING);
  if (edgex_device_get_crlid ())
  {
    iot_data_string_map_add (envelope, "correlationID", iot_data_alloc_string (edgex_device_get_crlid (), IOT_DATA_REF));
  }
  iot_data_string_map_add (envelope, "apiVersion", iot_data_alloc_string (EDGEX_API_VERSION, IOT_DATA_REF));
  iot_data_string_map_add (envelope, "errorCode", iot_data_alloc_ui32 (0));
  if (event_is_cbor || bus->cbor)
  {
    iot_data_string_map_add (envelope, "contentType", iot_data_alloc_string ("application/cbor", IOT_DATA_REF));
  }
  else
  {
    iot_data_string_map_add (envelope, "contentType", iot_data_alloc_string ("application/json", IOT_DATA_REF));
  }
  // Like Go SDK's behavior: if envelope is CBOR, payload will not be base64-encoded, either way.
  // If envelope is JSON and the event is a binary reading, payload will be base64-encoded CBOR, either way.
  if ((!bus->cbor) && (bus->msgb64payload || event_is_cbor))
  {
    iot_data_string_map_add (envelope, "payload", iot_data_alloc_string (edgex_data_to_b64 (payload, event_is_cbor), IOT_DATA_TAKE));
  }
  else
  {
    iot_data_string_map_add (envelope, "payload", iot_data_add_ref (payload));
  }

  bus->postfn (bus->ctx, path, envelope, bus->cbor);
  iot_data_free (envelope);
}

int edgex_bus_rmi (edgex_bus_t *bus, const char *path, const char *svcname, const iot_data_t *request, iot_data_t **reply)
{
  // NYI. as post, but include a request id in the envelope
  // register a handler for the reply if we dont already have one for that service
  // wait for a cond var
  return -1;
}

void edgex_bus_handle_request (edgex_bus_t *bus, const char *path, const char *envelope, uint32_t len)
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
    iot_data_t *envdata = NULL;
    bool envelope_is_json = false;
    bool payload_is_cbor = false;
    if (bus->cbor)
    {
      envdata = iot_data_from_cbor ((const uint8_t *)envelope, len);
    }
    else
    {
      envelope_is_json = true;
      if (envelope[len - 1] != '\0')
      {
        char *nullterm = strndup (envelope, len);
        envdata = iot_data_from_json (nullterm);
        free (nullterm);
      }
      else
      {
        envdata = iot_data_from_json (envelope);
      }
    }
    const char *contentType = iot_data_string_map_get_string (envdata, "contentType");
    if (strcmp (contentType, "application/cbor") == 0)
    {
      payload_is_cbor = true;
    }

    if ((bus->msgb64payload) || (envelope_is_json && payload_is_cbor))
    {
      const char *payload = iot_data_string_map_get_string (envdata, "payload");
      if (payload) 
      {
        size_t sz = iot_b64_maxdecodesize(payload);
        char *data = malloc(sz + 1);
        iot_b64_decode(payload, data, &sz);
        data[sz] = '\0';
        if (payload_is_cbor)
        {
          req = iot_data_from_cbor ((const uint8_t *)data, sz);
        }
        else
        {
          req = iot_data_from_json (data);
        }
        free(data);
      }
    }
    else
    {
      const iot_data_t *payload = iot_data_string_map_get_map (envdata, "payload");
      if (payload)
      {
        req = iot_data_add_ref (payload);
      }
    }


    crl = iot_data_string_map_get (envdata, "correlationID");
    if (crl)
    {
      edgex_device_alloc_crlid (iot_data_string (crl));
    }
    bool event_is_cbor = false;
    status = h (ctx, req, pathparams, iot_data_string_map_get (envdata, "queryParams"), &reply, &event_is_cbor);
    if (reply)
    {
      char *rpath;
      const char *idstr;
      const iot_data_t *id;
      iot_data_t *renv = iot_data_alloc_map (IOT_DATA_STRING);
      iot_data_string_map_add (renv, "errorCode", iot_data_alloc_i32 (status));
      if (bus->cbor || event_is_cbor)
      {
        iot_data_string_map_add (renv, "contentType", iot_data_alloc_string ("application/cbor", IOT_DATA_REF));
      }
      else
      {
        iot_data_string_map_add (renv, "contentType", iot_data_alloc_string ("application/json", IOT_DATA_REF));
      }
      if ((!bus->cbor) && (bus->msgb64payload || event_is_cbor))
      {
        iot_data_string_map_add (renv, "payload", iot_data_alloc_string (edgex_data_to_b64 (reply, event_is_cbor), IOT_DATA_TAKE));
      }
      else
      {
        iot_data_string_map_add (renv, "payload", iot_data_add_ref (reply));
      }
      iot_data_free (reply);
      iot_data_string_map_add (renv, "correlationID", iot_data_add_ref (crl));
      iot_data_string_map_add (renv, "apiVersion", iot_data_alloc_string (EDGEX_API_VERSION, IOT_DATA_REF));
      id = iot_data_string_map_get (envdata, "requestID");
      if (!id)
      {
        id = iot_data_string_map_get (envdata, "requestId");
      }
      if (id)
      {
        idstr = iot_data_string (id);
        // iot_data_string_map_add (renv, "requestID", iot_data_add_ref (id));
        rpath = edgex_bus_mktopic (bus, EDGEX_DEV_TOPIC_RESPONSE, idstr);
        bus->postfn (bus->ctx, rpath, renv, bus->cbor);
        free (rpath);
      }
      else
      {
        iot_log_error(iot_logger_default (), "edgex_bus_handle_request: no request ID in envelope, cannot send reply");
      }
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
  bus->msgb64payload = false;
  const char *msgb64payload = getenv("EDGEX_MSG_BASE64_PAYLOAD");
  if (msgb64payload && strcmp (msgb64payload, "true") == 0)
  {
    bus->msgb64payload = true;
  }
  bus->cbor = false;
  const char *msgcbor = getenv("EDGEX_MSG_CBOR_ENCODE");
  if (msgcbor && strcmp (msgcbor, "true") == 0)
  {
    bus->cbor = true;
  }
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
  entry->ignore_tail = false;
  entry->params = iot_data_alloc_list ();
  const char *param = strchr (path, '{');
  if (param)
  {
    size_t plen = param - path;
    entry->base = strndup (path, plen);
    entry->base_len = plen;
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
    size_t path_len = strlen (path);
    if (path_len >= 2 && strcmp (path + path_len - 2, "/#") == 0)
    {
      path_len -= 2;
      entry->ignore_tail = true;
    }
    sub = strdup (path);
    entry->base = strdup (path);
    entry->base_len = path_len;
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
