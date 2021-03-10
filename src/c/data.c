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

typedef struct postparams
{
  devsdk_service_t *svc;
  edgex_event_cooked *event;
} postparams;

static char *edgex_value_tostring (const iot_data_t *value)
{
#define BUFSIZE 32
  char *res;

  if (iot_data_type (value) == IOT_DATA_ARRAY && edgex_propertytype_data (value) != Edgex_Binary)
  {
    char vstr[BUFSIZE];
    uint32_t length = iot_data_array_length (value);
    res = malloc (BUFSIZE * length);
    res[0] = 0;
    for (uint32_t i = 0; i < length; i++)
    {
      switch (edgex_propertytype_data (value))
      {
        case Edgex_Int8Array:
          sprintf (vstr, "%" PRIi8, ((int8_t *)iot_data_address (value))[i]);
          break;
        case Edgex_Uint8Array:
          sprintf (vstr, "%" PRIu8, ((uint8_t *)iot_data_address (value))[i]);
          break;
        case Edgex_Int16Array:
          sprintf (vstr, "%" PRIi16, ((int16_t *)iot_data_address (value))[i]);
          break;
        case Edgex_Uint16Array:
          sprintf (vstr, "%" PRIu16, ((uint16_t *)iot_data_address (value))[i]);
          break;
        case Edgex_Int32Array:
          sprintf (vstr, "%" PRIi32, ((int32_t *)iot_data_address (value))[i]);
          break;
        case Edgex_Uint32Array:
          sprintf (vstr, "%" PRIu32, ((uint32_t *)iot_data_address (value))[i]);
          break;
        case Edgex_Int64Array:
          sprintf (vstr, "%" PRIi64, ((int64_t *)iot_data_address (value))[i]);
          break;
        case Edgex_Uint64Array:
          sprintf (vstr, "%" PRIu64, ((uint64_t *)iot_data_address (value))[i]);
          break;
        case Edgex_Float32Array:
          sprintf (vstr, "%.8e", ((float *)iot_data_address (value))[i]);
          break;
        case Edgex_Float64Array:
          sprintf (vstr, "%.16e", ((double *)iot_data_address (value))[i]);
          break;
        case Edgex_BoolArray:
          strcpy (vstr, ((bool *)iot_data_address (value))[i] ? "true" : "false");
          break;
        default:
          strcpy (vstr, "?");
      }
      strcat (res, i ? ",\"" : "[\"");
      strcat (res, vstr);
      strcat (res, "\"");
    }
    strcat (res, "]");
  }
  else
  {
    switch (edgex_propertytype_data (value))
    {
      case Edgex_String:
        res = strdup (iot_data_string (value));
        break;
      case Edgex_Binary:
      {
        uint32_t sz, rsz;
        const uint8_t *data = iot_data_address (value);
        rsz = iot_data_array_size (value);
        sz = iot_b64_encodesize (rsz);
        res = malloc (sz);
        iot_b64_encode (data, rsz, res, sz);
        break;
      }
      default:
        res = iot_data_to_json (value);
        break;
    }
  }
  return res;
}

/* Event data structure:

Reading:
  apiVersion: "v2"
  id: uuid (sdk to generate)
  created: Timestamp (filled in downstream)
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
  id: uuid (sdk to generate one)
  created: Timestamp (filled in downstream)
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
    if (commandinfo->pvals[i]->type == Edgex_Binary)
    {
      iot_data_t *b = iot_data_alloc_bool (true);
      iot_data_set_metadata (values[i].value, b);
      iot_data_free (b);
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

      edgex_propertytype pt = edgex_propertytype_data (values[i].value);
      char *id = edgex_device_genuuid ();

      if (pt == Edgex_Binary)
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
          { .key = cbor_move (cbor_build_string ("value")), .value = cbor_move (cread) });
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
        .value = cbor_move (cbor_build_string (commandinfo->reqs[i].resname))
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
        .value = cbor_move (cbor_build_string (edgex_propertytype_tostring (pt)))
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

    cbor_item_t *cwrapper = cbor_new_definite_map (2);
    cbor_map_add (cwrapper, (struct cbor_pair)
      { .key = cbor_move (cbor_build_string ("apiVersion")), .value = cbor_move (cbor_build_string (EDGEX_API_VERSION)) });
    cbor_map_add (cwrapper, (struct cbor_pair)
     { .key = cbor_move (cbor_build_string ("Event")), .value = cbor_move (cevent) });

    result->encoding = CBOR;
    result->value.cbor.length = cbor_serialize_alloc (cwrapper, &result->value.cbor.data, &bsize);
    cbor_decref (&cwrapper);
  }
  else
  {
    JSON_Value *jevent = json_value_init_object ();
    JSON_Object *jobj = json_value_get_object (jevent);
    JSON_Value *arrval = json_value_init_array ();
    JSON_Array *jrdgs = json_value_get_array (arrval);

    for (uint32_t i = 0; i < commandinfo->nreqs; i++)
    {
      char *id = edgex_device_genuuid ();
      edgex_propertytype pt = edgex_propertytype_data (values[i].value);
      char *reading = edgex_value_tostring (values[i].value);

      JSON_Value *rval = json_value_init_object ();
      JSON_Object *robj = json_value_get_object (rval);

      json_object_set_string (robj, "apiVersion", EDGEX_API_VERSION);
      json_object_set_string (robj, "id", id);
      json_object_set_string (robj, "profileName", commandinfo->profile->name);
      json_object_set_string (robj, "deviceName", device_name);
      json_object_set_string (robj, "resourceName", commandinfo->reqs[i].resname);
      json_object_set_string (robj, (pt == Edgex_Binary) ? "binaryValue" : "value", reading);
      json_object_set_uint (robj, "origin", values[i].origin ? values[i].origin : timenow);
      json_object_set_string (robj, "valueType", edgex_propertytype_tostring (pt));
      if (pt == Edgex_Binary)
      {
        json_object_set_string (robj, "mediaType", commandinfo->pvals[i]->mediaType);
      }
      json_array_append_value (jrdgs, rval);
      free (id);
      free (reading);
    }

    json_object_set_string (jobj, "apiVersion", EDGEX_API_VERSION);
    json_object_set_string (jobj, "id", eventId);
    json_object_set_string (jobj, "deviceName", device_name);
    json_object_set_string (jobj, "profileName", commandinfo->profile->name);
    json_object_set_string (jobj, "sourceName", commandinfo->name);
    json_object_set_uint (jobj, "origin", timenow);
    json_object_set_value (jobj, "readings", arrval);
    result->encoding = JSON;
    JSON_Value *reqval = edgex_wrap_request_single ("Event", jevent);
    result->value.json = json_serialize_to_string (reqval);
    json_value_free (reqval);
  }
  free (eventId);
  return result;
}

static void *edgex_data_post (void *p)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  postparams *pp = (postparams *) p;
  devsdk_error err = EDGEX_OK;

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v2/event/%s",
    pp->svc->config.endpoints.data.host,
    pp->svc->config.endpoints.data.port,
    pp->event->path
  );

  switch (pp->event->encoding)
  {
    case JSON:
    {
      edgex_http_post (pp->svc->logger, &ctx, url, pp->event->value.json, NULL, &err);
      break;
    }
    case CBOR:
    {
      edgex_http_postbin
        (pp->svc->logger, &ctx, url, pp->event->value.cbor.data, pp->event->value.cbor.length, CONTENT_CBOR, NULL, &err);
      break;
    }
  }
  edgex_event_cooked_free (pp->event);
  free (pp);
  return NULL;
}

void edgex_data_client_add_event (devsdk_service_t *svc, edgex_event_cooked *ev)
{
  postparams *pp = malloc (sizeof (postparams));
  pp->svc = svc;
  pp->event = ev;
  iot_threadpool_add_work (svc->eventq, edgex_data_post, pp, -1);
}

void edgex_data_client_add_event_now (devsdk_service_t *svc, edgex_event_cooked *ev)
{
  postparams *pp = malloc (sizeof (postparams));
  pp->svc = svc;
  pp->event = ev;
  edgex_data_post (pp);
}

void edgex_event_cooked_add_ref (edgex_event_cooked *e)
{
  atomic_fetch_add (&e->refs, 1);
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
