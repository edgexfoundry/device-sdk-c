/*
 * Copyright (c) 2018-2025
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "edgex-rest.h"
#include "rest.h"
#include "api.h"
#include "data.h"
#include "errorlist.h"
#include "iot/time.h"
#include "service.h"
#include "transform.h"
#include "correlation.h"
#include "iot/base64.h"

#include <cbor.h>
#include <microhttpd.h>

static void edc_update_metrics (devsdk_metrics_t *metrics, const edgex_event_cooked *event)
{
  atomic_fetch_add (&metrics->esent, 1);
  atomic_fetch_add (&metrics->rsent, event->nrdgs);
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
  tags: Map of Strings (may be added to at any stage)
  readings: Array of Readings
*/

edgex_event_cooked *edgex_data_process_event
(
  const edgex_device *device,
  const edgex_cmdinfo *commandinfo,
  devsdk_commandresult *values,
  iot_data_t *tags,
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
  result->nrdgs = commandinfo->nreqs;

  result->path = malloc (strlen (commandinfo->profile->name) + strlen (device->name) + strlen (commandinfo->name) + 3);
  strcpy (result->path, commandinfo->profile->name);
  strcat (result->path, "/");
  strcat (result->path, device->name);
  strcat (result->path, "/");
  strcat (result->path, commandinfo->name);

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
    iot_data_string_map_add (rmap, "deviceName", iot_data_alloc_string (device->name, IOT_DATA_REF));
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
        iot_data_string_map_add (rmap, "value", iot_data_alloc_string (iot_data_string (values[i].value), IOT_DATA_COPY));
        break;
      default:
        iot_data_string_map_add (rmap, "value", iot_data_alloc_string (iot_data_to_json (values[i].value), IOT_DATA_TAKE));
    }

    if (commandinfo->reqs[i].resource->tags)
    {
      iot_data_string_map_add (rmap, "tags", iot_data_copy(commandinfo->reqs[i].resource->tags));
    }

    iot_data_vector_add (rvec, i, rmap);
  }

  iot_data_t *event_tags = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_map_merge(event_tags, tags);
  iot_data_map_merge(event_tags,commandinfo->tags);
  iot_data_map_merge(event_tags,device->tags);

  iot_data_t *evmap = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_string_map_add (evmap, "apiVersion", iot_data_alloc_string (EDGEX_API_VERSION, IOT_DATA_REF));
  iot_data_string_map_add (evmap, "id", iot_data_alloc_string (eventId, IOT_DATA_TAKE));
  iot_data_string_map_add (evmap, "deviceName", iot_data_alloc_string (device->name, IOT_DATA_REF));
  iot_data_string_map_add (evmap, "profileName", iot_data_alloc_string (commandinfo->profile->name, IOT_DATA_REF));
  iot_data_string_map_add (evmap, "sourceName", iot_data_alloc_string (commandinfo->name, IOT_DATA_REF));
  iot_data_string_map_add (evmap, "origin", iot_data_alloc_ui64 (timenow));
  iot_data_string_map_add (evmap, "readings", rvec);

  if (iot_data_map_size(event_tags)) {
    iot_data_string_map_add (evmap, "tags", event_tags);
  }

  iot_data_t *reqmap = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_string_map_add (reqmap, "apiVersion", iot_data_alloc_string (EDGEX_API_VERSION, IOT_DATA_REF));
  iot_data_string_map_add (reqmap, "event", evmap);
  iot_data_string_map_add (reqmap, "statusCode", iot_data_alloc_ui32 (MHD_HTTP_OK));

  result->encoding = useCBOR ? CBOR : JSON;
  // XXX tag the value with this
  result->value = reqmap;
  return result;
}

void edgex_data_client_add_event (edgex_bus_t *client, edgex_event_cooked *ev, devsdk_metrics_t *metrics)
{
  char *topic = edgex_bus_mktopic (client, EDGEX_DEV_TOPIC_EVENT, ev->path);
  edc_update_metrics (metrics, ev);
  edgex_bus_post (client, topic, ev->value);
  free (topic);
}

size_t edgex_event_cooked_size (edgex_event_cooked *e)
{
  size_t result;
  if (e->encoding == JSON)
  {
      char *json = iot_data_to_json (e->value);
      result = strlen (json);
      free (json);
  }
  else
  {
    iot_data_t *cbor = iot_data_to_cbor (e->value);
    result = iot_data_array_size (cbor);
    iot_data_free (cbor);
  }
  return result;
}

void edgex_event_cooked_write (edgex_event_cooked *e, devsdk_http_reply *reply)
{
  switch (e->encoding)
  {
    case JSON:
    {
      char *json = iot_data_to_json (e->value);
      reply->data.bytes = json;
      reply->data.size = strlen (json);
      reply->content_type = CONTENT_JSON;
      break;
    }
    case CBOR:
    {
      iot_data_t *cbor = iot_data_to_cbor (e->value);
      reply->data.size = iot_data_array_size (cbor);
      reply->data.bytes = malloc (reply->data.size);
      memcpy (reply->data.bytes, iot_data_address (e->value), reply->data.size);
      reply->content_type = CONTENT_CBOR;
      iot_data_free (cbor);
      break;
    }
  }
  reply->code = MHD_HTTP_OK;
}

void edgex_event_cooked_free (edgex_event_cooked *e)
{
  if (e)
  {
    iot_data_free (e->value);
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
