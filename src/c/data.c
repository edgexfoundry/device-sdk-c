/*
 * Copyright (c) 2018-2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "edgex-rest.h"
#include "rest.h"
#include "data.h"
#include "errorlist.h"
#include "iot/time.h"
#include "service.h"
#include "transform.h"
#include "correlation.h"
#include "iot/base64.h"

#include <cbor.h>
#include <microhttpd.h>

#define EDGEX_API_VERSION "v2"

typedef struct edc_postparams
{
  edgex_data_client_t *client;
  edgex_event_cooked *event;
  devsdk_metrics_t *metrics;
} edc_postparams;

static void edc_update_metrics (devsdk_metrics_t *metrics, const edgex_event_cooked *event)
{
  atomic_fetch_add (&metrics->esent, 1);
  atomic_fetch_add (&metrics->rsent, event->nrdgs);
}

static void *edc_postjob (void *p)
{
  edc_postparams *pp = (edc_postparams *)p;
  edc_update_metrics (pp->metrics, pp->event);
  pp->client->pf (pp->client->lc, pp->client->address, pp->event);
  free (pp);
  return NULL;
}

static char *edgex_value_tostring (const iot_data_t *value)
{
  char *res;
  if (iot_data_type (value) == IOT_DATA_BINARY)
  {
    uint32_t sz, rsz;
    rsz = iot_data_array_size (value);
    sz = iot_b64_encodesize (rsz);
    res = malloc (sz);
    iot_b64_encode (iot_data_address (value), rsz, res, sz);
  }
  else
  {
    res = (iot_data_type (value) == IOT_DATA_STRING) ? strdup (iot_data_string (value)) : iot_data_to_json (value);
  }

  return res;
}

/* Event data structure:

Reading:
  apiVersion: "v2"
  id: uuid (sdk to generate)
  origin: Timestamp (filled in by the implementation or the SDK)
  deviceName: String (name of the Device)
  resourceName: String (name of the DeviceResource)
  profileName: String (name of the Device Profile)
  valueType: String

plus

  value: String

or

  binaryValue: String
  mediaType: String

Event:
  apiVersion: "v2"
  statusCode: int
  id: uuid (sdk to generate one)
  origin: Timestamp (filled in by the SDK)
  deviceName: String (name of the Device)
  profileName: String (name of the Profile)
  sourceName: String (name of the deviceResource or deviceCommand)
  tags: Array of Strings (may be added to at any stage)
  readings: Array of Readings
*/

edgex_event_cooked *edgex_data_process_event
(
  const char *device_name,
  const edgex_cmdinfo *commandinfo,
  devsdk_commandresult *values,
  bool doTransforms
)
{
  char *eventId;
  edgex_event_cooked *result = NULL;
  bool useCBOR = false;
  uint64_t timenow = iot_time_nsecs ();

  for (uint32_t i = 0; i < commandinfo->nreqs; i++)
  {
    if (commandinfo->pvals[i]->type.type == IOT_DATA_BINARY)
    {
      useCBOR = true;
    }
    if (doTransforms)
    {
      edgex_transform_outgoing (&values[i], commandinfo->pvals[i], commandinfo->maps[i]);
    }
    const char *assertion = commandinfo->pvals[i]->assertion;
    if (assertion && *assertion)
    {
      char *reading = edgex_value_tostring (values[i].value);
      if (strcmp (reading, assertion))
      {
        free (reading);
        return NULL;
      }
      else
      {
        free (reading);
      }
    }
  }

  eventId = edgex_device_genuuid ();
  result = malloc (sizeof (edgex_event_cooked));
  atomic_store (&result->refs, 1);
  result->nrdgs = commandinfo->nreqs;

  result->path = malloc (strlen (commandinfo->profile->name) + strlen (device_name) + strlen (commandinfo->name) + 3);
  strcpy (result->path, commandinfo->profile->name);
  strcat (result->path, "/");
  strcat (result->path, device_name);
  strcat (result->path, "/");
  strcat (result->path, commandinfo->name);

  if (useCBOR)
  {
    size_t bsize = 0;
    cbor_item_t *cevent = cbor_new_definite_map (7);
    cbor_item_t *crdgs = cbor_new_definite_array (commandinfo->nreqs);

    for (uint32_t i = 0; i < commandinfo->nreqs; i++)
    {
      cbor_item_t *cread;
      cbor_item_t *crdg;

      iot_typecode_t tc;
      iot_data_typecode (values[i].value, &tc);
      char *id = edgex_device_genuuid ();

      if (tc.type == IOT_DATA_BINARY)
      {
        const uint8_t *data;
        crdg = cbor_new_definite_map (9);
        uint32_t sz = iot_data_array_size (values[i].value);
        data = iot_data_address (values[i].value);
        cread = cbor_build_bytestring (data, sz);
        cbor_map_add (crdg, (struct cbor_pair)
          { .key = cbor_move (cbor_build_string ("binaryValue")), .value = cbor_move (cread) });
        cbor_map_add (crdg, (struct cbor_pair)
          { .key = cbor_move (cbor_build_string ("mediaType")), .value = cbor_move (cbor_build_string (commandinfo->pvals[i]->mediaType)) });
      }
      else
      {
        char *reading = edgex_value_tostring (values[i].value);
        crdg = cbor_new_definite_map (8);
        cread = cbor_build_string (reading);
        free (reading);
        cbor_map_add (crdg, (struct cbor_pair)
          { .key = cbor_move (cbor_build_string (tc.type == IOT_DATA_MAP ? "objectValue" : "value")), .value = cbor_move (cread) });
      }

      cbor_map_add (crdg, (struct cbor_pair)
      {
        .key = cbor_move (cbor_build_string ("apiVersion")),
        .value = cbor_move (cbor_build_string (EDGEX_API_VERSION))
      });

      cbor_map_add (crdg, (struct cbor_pair)
      {
        .key = cbor_move (cbor_build_string ("id")),
        .value = cbor_move (cbor_build_string (id))
      });

      free (id);

      cbor_map_add (crdg, (struct cbor_pair)
      {
        .key = cbor_move (cbor_build_string ("resourceName")),
        .value = cbor_move (cbor_build_string (commandinfo->reqs[i].resource->name))
      });

      cbor_map_add (crdg, (struct cbor_pair)
      {
        .key = cbor_move (cbor_build_string ("deviceName")),
        .value = cbor_move (cbor_build_string (device_name))
      });
      cbor_map_add (crdg, (struct cbor_pair)
      {
        .key = cbor_move (cbor_build_string ("profileName")),
        .value = cbor_move (cbor_build_string (commandinfo->profile->name))
      });

      cbor_map_add (crdg, (struct cbor_pair)
      {
        .key = cbor_move (cbor_build_string ("valueType")),
        .value = cbor_move (cbor_build_string (edgex_typecode_tostring (tc)))
      });

      cbor_map_add (crdg, (struct cbor_pair)
      {
        .key = cbor_move (cbor_build_string ("origin")),
        .value = cbor_move (cbor_build_uint64 (values[i].origin ? values[i].origin : timenow))
      });

      cbor_array_push (crdgs, cbor_move (crdg));
    }

    cbor_map_add (cevent, (struct cbor_pair)
      { .key = cbor_move (cbor_build_string ("apiVersion")), .value = cbor_move (cbor_build_string (EDGEX_API_VERSION)) });
    cbor_map_add (cevent, (struct cbor_pair)
      { .key = cbor_move (cbor_build_string ("id")), .value = cbor_move (cbor_build_string (eventId)) });
    cbor_map_add (cevent, (struct cbor_pair)
      { .key = cbor_move (cbor_build_string ("deviceName")), .value = cbor_move (cbor_build_string (device_name)) });
    cbor_map_add (cevent, (struct cbor_pair)
      { .key = cbor_move (cbor_build_string ("profileName")), .value = cbor_move (cbor_build_string (commandinfo->profile->name)) });
    cbor_map_add (cevent, (struct cbor_pair)
      { .key = cbor_move (cbor_build_string ("sourceName")), .value = cbor_move (cbor_build_string (commandinfo->name)) });
    cbor_map_add (cevent, (struct cbor_pair)
      { .key = cbor_move (cbor_build_string ("origin")), .value = cbor_move (cbor_build_uint64 (timenow)) });
    cbor_map_add (cevent, (struct cbor_pair)
      { .key = cbor_move (cbor_build_string ("readings")), .value = cbor_move (crdgs) });

    cbor_item_t *cwrapper = cbor_new_definite_map (3);
    cbor_map_add (cwrapper, (struct cbor_pair)
      { .key = cbor_move (cbor_build_string ("apiVersion")), .value = cbor_move (cbor_build_string (EDGEX_API_VERSION)) });
    cbor_map_add (cwrapper, (struct cbor_pair) { .key = cbor_move (cbor_build_string ("event")), .value = cbor_move (cevent) });
    cbor_map_add (cwrapper, (struct cbor_pair) { .key = cbor_move (cbor_build_string ("statusCode")), .value = cbor_move (cbor_build_uint32 (MHD_HTTP_OK)) });

    result->encoding = CBOR;
    result->value.cbor.length = cbor_serialize_alloc (cwrapper, &result->value.cbor.data, &bsize);
    cbor_decref (&cwrapper);
  }
  else
  {
    iot_data_t *rvec = iot_data_alloc_vector (commandinfo->nreqs);
    for (uint32_t i = 0; i < commandinfo->nreqs; i++)
    {
      iot_data_t *rmap = iot_data_alloc_map (IOT_DATA_STRING);
      char *id = edgex_device_genuuid ();
      iot_typecode_t tc;
      iot_data_typecode (values[i].value, &tc);

      iot_data_string_map_add (rmap, "apiVersion", iot_data_alloc_string (EDGEX_API_VERSION, IOT_DATA_REF));
      iot_data_string_map_add (rmap, "id", iot_data_alloc_string (id, IOT_DATA_TAKE));
      iot_data_string_map_add (rmap, "profileName", iot_data_alloc_string (commandinfo->profile->name, IOT_DATA_REF));
      iot_data_string_map_add (rmap, "deviceName", iot_data_alloc_string (device_name, IOT_DATA_REF));
      iot_data_string_map_add (rmap, "resourceName", iot_data_alloc_string (commandinfo->reqs[i].resource->name, IOT_DATA_REF));
      iot_data_string_map_add (rmap, "valueType", iot_data_alloc_string (edgex_typecode_tostring (tc), IOT_DATA_REF));
      iot_data_string_map_add (rmap, "origin", iot_data_alloc_ui64 (values[i].origin ? values[i].origin : timenow));
      switch (tc.type)
      {
        case IOT_DATA_BINARY:
          iot_data_string_map_add (rmap, "binaryValue", iot_data_copy (values[i].value));
          iot_data_string_map_add (rmap, "mediaType", iot_data_alloc_string (commandinfo->pvals[i]->mediaType, IOT_DATA_REF));
          break;
        case IOT_DATA_ARRAY:
          iot_data_string_map_add (rmap, "value", iot_data_alloc_string (edgex_value_tostring (values[i].value), IOT_DATA_TAKE));
          break;
        case IOT_DATA_MAP:
          iot_data_string_map_add (rmap, "objectValue", iot_data_copy (values[i].value));
          break;
        case IOT_DATA_STRING:
          iot_data_string_map_add (rmap, "value", iot_data_alloc_string (iot_data_string (values[i].value), IOT_DATA_REF));
          break;
        default:
          iot_data_string_map_add (rmap, "value", iot_data_alloc_string (iot_data_to_json (values[i].value), IOT_DATA_TAKE));
      }
      iot_data_vector_add (rvec, i, rmap);
    }

    iot_data_t *evmap = iot_data_alloc_map (IOT_DATA_STRING);
    iot_data_string_map_add (evmap, "apiVersion", iot_data_alloc_string (EDGEX_API_VERSION, IOT_DATA_REF));
    iot_data_string_map_add (evmap, "id", iot_data_alloc_string (eventId, IOT_DATA_REF));
    iot_data_string_map_add (evmap, "deviceName", iot_data_alloc_string (device_name, IOT_DATA_REF));
    iot_data_string_map_add (evmap, "profileName", iot_data_alloc_string (commandinfo->profile->name, IOT_DATA_REF));
    iot_data_string_map_add (evmap, "sourceName", iot_data_alloc_string (commandinfo->name, IOT_DATA_REF));
    iot_data_string_map_add (evmap, "origin", iot_data_alloc_ui64 (timenow));
    iot_data_string_map_add (evmap, "readings", rvec);

    iot_data_t *reqmap = iot_data_alloc_map (IOT_DATA_STRING);
    iot_data_string_map_add (reqmap, "apiVersion", iot_data_alloc_string (EDGEX_API_VERSION, IOT_DATA_REF));
    iot_data_string_map_add (reqmap, "event", evmap);
    iot_data_string_map_add (reqmap, "statusCode", iot_data_alloc_ui32 (MHD_HTTP_OK));
    result->encoding = JSON;
    result->value.json = iot_data_to_json (reqmap);
    iot_data_free (reqmap);
  }
  free (eventId);
  return result;
}

void edgex_data_client_add_event (edgex_data_client_t *client, edgex_event_cooked *ev, devsdk_metrics_t *metrics)
{
  edc_postparams *pp = malloc (sizeof (edc_postparams));
  pp->client = client;
  pp->event = ev;
  pp->metrics = metrics;
  iot_threadpool_add_work (client->queue, edc_postjob, pp, -1);
}

void edgex_data_client_add_event_now (edgex_data_client_t *client, edgex_event_cooked *ev, devsdk_metrics_t *metrics)
{
  edc_update_metrics (metrics, ev);
  client->pf (client->lc, client->address, ev);
}

static char *edc_b64 (iot_data_t *src)
{
  size_t encsz;
  char *result;
  char *json = iot_data_to_json (src);
  iot_data_free (src);
  size_t sz = strlen (json) + 1;

  encsz = iot_b64_encodesize (sz);
  result = malloc (encsz);
  iot_b64_encode (json, sz, result, encsz);
  free (json);
  return result;
}

static void edc_publish_metric (edgex_data_client_t *client, const char *mname, uint64_t val)
{
  iot_data_t *field;
  iot_data_t *fields;
  iot_data_t *metric;
  iot_data_t *envelope;

  field = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_string_map_add (field, "name", iot_data_alloc_string ("counter-count", IOT_DATA_REF));
  iot_data_string_map_add (field, "value", iot_data_alloc_ui64 (val));
  fields = iot_data_alloc_vector (1);
  iot_data_vector_add (fields, 0, field);
  metric = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_string_map_add (metric, "apiVersion", iot_data_alloc_string ("v2", IOT_DATA_REF));
  iot_data_string_map_add (metric, "name", iot_data_alloc_string (mname, IOT_DATA_REF));
  iot_data_string_map_add (metric, "fields", fields);
  iot_data_string_map_add (metric, "timestamp", iot_data_alloc_ui64 (iot_time_nsecs ()));

  envelope = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_string_map_add (envelope, "CorrelationID", iot_data_alloc_string (edgex_device_get_crlid (), IOT_DATA_REF));
  iot_data_string_map_add (envelope, "apiVersion", iot_data_alloc_string ("v2", IOT_DATA_REF));
  iot_data_string_map_add (envelope, "ErrorCode", iot_data_alloc_ui32 (0));
  iot_data_string_map_add (envelope, "ContentType", iot_data_alloc_string ("application/json", IOT_DATA_REF));
  iot_data_string_map_add (envelope, "Payload", iot_data_alloc_string (edc_b64 (metric), IOT_DATA_TAKE));

  client->mf (client->address, mname, envelope);

  iot_data_free (envelope);
}

void edgex_data_client_publish_metrics (edgex_data_client_t *client, const devsdk_metrics_t *metrics, const edgex_device_metricinfo *cfg)
{
  if (client->mf)
  {
    iot_log_debug (client->lc, "Publishing metrics");
    edgex_device_alloc_crlid (NULL);
    if (cfg->flags & EX_METRIC_EVSENT) edc_publish_metric (client, "EventsSent", atomic_load (&metrics->esent));
    if (cfg->flags & EX_METRIC_RDGSENT) edc_publish_metric (client, "ReadingsSent", atomic_load (&metrics->rsent));
    if (cfg->flags & EX_METRIC_RDCMDS) edc_publish_metric (client, "ReadCommandsExecuted", atomic_load (&metrics->rcexe));
    if (cfg->flags & EX_METRIC_SECREQ) edc_publish_metric (client, "SecuritySecretsRequested", atomic_load (&metrics->secrq));
    if (cfg->flags & EX_METRIC_SECSTO) edc_publish_metric (client, "SecuritySecretsStored", atomic_load (&metrics->secsto));
    edgex_device_free_crlid ();
  }
}

void edgex_data_client_free (edgex_data_client_t *client)
{
  if (client)
  {
    client->ff (client->lc, client->address);
    free (client);
  }
}

void edgex_event_cooked_add_ref (edgex_event_cooked *e)
{
  atomic_fetch_add (&e->refs, 1);
}

size_t edgex_event_cooked_size (edgex_event_cooked *e)
{
  switch (e->encoding)
  {
    case JSON:
      return strlen (e->value.json);
      break;
    case CBOR:
      return e->value.cbor.length;
      break;
  }
  return 0;
}

void edgex_event_cooked_write (edgex_event_cooked *e, devsdk_http_reply *reply)
{
  switch (e->encoding)
  {
    case JSON:
      reply->data.bytes = e->value.json;
      reply->data.size = strlen (e->value.json);
      reply->content_type = CONTENT_JSON;
      break;
    case CBOR:
      reply->data.bytes = e->value.cbor.data;
      reply->data.size = e->value.cbor.length;
      reply->content_type = CONTENT_CBOR;
      break;
  }
  reply->code = MHD_HTTP_OK;
  free (e->path);
  free (e);
}

void edgex_event_cooked_free (edgex_event_cooked *e)
{
  if (e && (atomic_fetch_add (&e->refs, -1) == 1))
  {
    switch (e->encoding)
    {
      case JSON:
        json_free_serialized_string (e->value.json);
        break;
      case CBOR:
        free (e->value.cbor.data);
        break;
    }
    free (e->path);
    free (e);
  }
}

void devsdk_commandresult_free (devsdk_commandresult *res, int n)
{
  if (res)
  {
    for (int i = 0; i < n; i++)
    {
      iot_data_free (res[i].value);
    }
    free (res);
  }
}

devsdk_commandresult *devsdk_commandresult_dup (const devsdk_commandresult *res, int n)
{
  devsdk_commandresult *result = calloc (n, sizeof (devsdk_commandresult));
  for (int i = 0; i < n; i++)
  {
    result[i].value = iot_data_copy (res[i].value);
  }
  return result;
}

bool devsdk_commandresult_equal
  (const devsdk_commandresult *lhs, const devsdk_commandresult *rhs, int n)
{
  bool result = true;
  for (int i = 0; i < n; i++)
  {
    if (!iot_data_equal (lhs[i].value, rhs[i].value))
    {
      result = false;
      break;
    }
  }
  return result;
}
