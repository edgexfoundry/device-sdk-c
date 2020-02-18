/*
 * Copyright (c) 2018, 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "edgex-rest.h"
#include "rest.h"
#include "data.h"
#include "errorlist.h"
#include "config.h"
#include "iot/time.h"
#include "transform.h"
#include "iot/base64.h"

#include <cbor.h>

static char *edgex_value_tostring (const iot_data_t *value, bool binfloat)
{
#define BUFSIZE 32
  char *res;

  switch (iot_data_type (value))
  {
    case IOT_DATA_FLOAT32:
      res = malloc (BUFSIZE);
      if (binfloat)
      {
        float f = iot_data_f32 (value);
        iot_b64_encode (&f, sizeof (float), res, BUFSIZE);
      }
      else
      {
        sprintf (res, "%.8e", iot_data_f32 (value));
      }
      break;
    case IOT_DATA_FLOAT64:
      res = malloc (BUFSIZE);
      if (binfloat)
      {
        double d = iot_data_f64 (value);
        iot_b64_encode (&d, sizeof (double), res, BUFSIZE);
      }
      else
      {
        sprintf (res, "%.16e", iot_data_f64 (value));
      }
      break;
    case IOT_DATA_STRING:
      res = strdup (iot_data_string (value));
      break;
    case IOT_DATA_BLOB:
    {
      uint32_t sz, rsz;
      const uint8_t *data = iot_data_blob (value, &rsz);
      sz = iot_b64_encodesize (rsz);
      res = malloc (sz);
      iot_b64_encode (data, rsz, res, sz);
      break;
    }
    default:
      res = iot_data_to_json (value, false);
      break;
  }
  return res;
}

edgex_event_cooked *edgex_data_process_event
(
  const char *device_name,
  const edgex_cmdinfo *commandinfo,
  devsdk_commandresult *values,
  bool doTransforms
)
{
  edgex_event_cooked *result = NULL;
  bool useCBOR = false;
  uint64_t timenow = iot_time_nsecs ();
  for (uint32_t i = 0; i < commandinfo->nreqs; i++)
  {
    if (doTransforms)
    {
      edgex_transform_outgoing (&values[i], commandinfo->pvals[i], commandinfo->maps[i]);
    }
    const char *assertion = commandinfo->pvals[i]->assertion;
    if (assertion && *assertion)
    {
      char *reading = edgex_value_tostring (values[i].value, commandinfo->pvals[i]->floatAsBinary);
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
    if (commandinfo->pvals[i]->type == IOT_DATA_BLOB)
    {
      useCBOR = true;
    }
  }

  result = malloc (sizeof (edgex_event_cooked));
  if (useCBOR)
  {
    size_t bsize = 0;
    cbor_item_t *cevent = cbor_new_definite_map (3);
    cbor_item_t *crdgs = cbor_new_definite_array (commandinfo->nreqs);

    for (uint32_t i = 0; i < commandinfo->nreqs; i++)
    {
      cbor_item_t *crdg = cbor_new_definite_map (3);

      cbor_item_t *cread;
      if (iot_data_type (values[i].value) == IOT_DATA_BLOB)
      {
        const uint8_t *data;
        uint32_t sz;
        data = iot_data_blob (values[i].value, &sz);
        cread = cbor_build_bytestring (data, sz);
        cbor_map_add (crdg, (struct cbor_pair)
          { .key = cbor_move (cbor_build_string ("binaryValue")), .value = cbor_move (cread) });
      }
      else
      {
        char *reading = edgex_value_tostring (values[i].value, commandinfo->pvals[i]->floatAsBinary);
        cread = cbor_build_string (reading);
        free (reading);
        cbor_map_add (crdg, (struct cbor_pair)
          { .key = cbor_move (cbor_build_string ("value")), .value = cbor_move (cread) });
      }

      cbor_map_add (crdg, (struct cbor_pair)
      {
        .key = cbor_move (cbor_build_string ("name")),
        .value = cbor_move (cbor_build_string (commandinfo->reqs[i].resname))
      });

      cbor_map_add (crdg, (struct cbor_pair)
      {
        .key = cbor_move (cbor_build_string ("origin")),
        .value = cbor_move (cbor_build_uint64 (values[i].origin ? values[i].origin : timenow))
      });

      cbor_array_push (crdgs, cbor_move (crdg));
    }

    cbor_map_add (cevent, (struct cbor_pair)
      { .key = cbor_move (cbor_build_string ("device")), .value = cbor_move (cbor_build_string (device_name)) });
    cbor_map_add (cevent, (struct cbor_pair)
      { .key = cbor_move (cbor_build_string ("origin")), .value = cbor_move (cbor_build_uint64 (timenow)) });
    cbor_map_add (cevent, (struct cbor_pair)
      { .key = cbor_move (cbor_build_string ("readings")), .value = cbor_move (crdgs) });

    result->encoding = CBOR;
    result->value.cbor.length = cbor_serialize_alloc (cevent, &result->value.cbor.data, &bsize);
    cbor_decref (&cevent);
    return result;
  }
  else
  {
    JSON_Value *jevent = json_value_init_object ();
    JSON_Object *jobj = json_value_get_object (jevent);
    JSON_Value *arrval = json_value_init_array ();
    JSON_Array *jrdgs = json_value_get_array (arrval);

    for (uint32_t i = 0; i < commandinfo->nreqs; i++)
    {
      char *reading = edgex_value_tostring (values[i].value, commandinfo->pvals[i]->floatAsBinary);

      JSON_Value *rval = json_value_init_object ();
      JSON_Object *robj = json_value_get_object (rval);

      json_object_set_string (robj, "name", commandinfo->reqs[i].resname);
      json_object_set_string (robj, "value", reading);
      json_object_set_uint (robj, "origin", values[i].origin ? values[i].origin : timenow);
      json_array_append_value (jrdgs, rval);
      free (reading);
    }

    json_object_set_string (jobj, "device", device_name);
    json_object_set_uint (jobj, "origin", timenow);
    json_object_set_value (jobj, "readings", arrval);
    result->encoding = JSON;
    result->value.json = json_serialize_to_string (jevent);
    json_value_free (jevent);
  }
  return result;
}

void edgex_data_client_add_event
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  edgex_event_cooked *eventval,
  devsdk_error *err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/event",
    endpoints->data.host,
    endpoints->data.port
  );

  switch (eventval->encoding)
  {
    case JSON:
    {
      edgex_http_post (lc, &ctx, url, eventval->value.json, NULL, err);
      break;
    }
    case CBOR:
    {
      edgex_http_postbin
        (lc, &ctx, url, eventval->value.cbor.data, eventval->value.cbor.length, "application/cbor", NULL, err);
      break;
    }
  }
}

void edgex_event_cooked_free (edgex_event_cooked *e)
{
  if (e)
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

/* Dummy methods: onChange on AutoEvents will not work until these two are impl
emented in iot-c-utils */

static iot_data_t *iot_data_dup (const iot_data_t *rhs) { return NULL; }
static bool iot_data_equal (const iot_data_t *lhs, const iot_data_t *rhs) { return false; }

devsdk_commandresult *devsdk_commandresult_dup (const devsdk_commandresult *res, int n)
{
  devsdk_commandresult *result = calloc (n, sizeof (devsdk_commandresult));
  for (int i = 0; i < n; i++)
  {
    result[i].value = iot_data_dup (res[i].value);
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

edgex_valuedescriptor *edgex_data_client_add_valuedescriptor
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  const char *name,
  uint64_t origin,
  const char *min,
  const char *max,
  const char *type,
  const char *uomLabel,
  const char *defaultValue,
  const char *formatting,
  const char *description,
  const char *mediaType,
  const char *floatEncoding,
  devsdk_error *err
)
{
  edgex_valuedescriptor *result = malloc (sizeof (edgex_valuedescriptor));
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  char *json;

  memset (result, 0, sizeof (edgex_valuedescriptor));
  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/valuedescriptor",
    endpoints->data.host,
    endpoints->data.port
  );
  result->origin = origin;
  result->name = strdup (name);
  result->min = strdup (min);
  result->max = strdup (max);
  result->type = strdup (type);
  result->uomLabel = strdup (uomLabel);
  result->defaultValue = strdup (defaultValue);
  result->formatting = strdup (formatting);
  result->description = strdup (description);
  result->mediaType = strdup (mediaType);
  result->floatEncoding = strdup (floatEncoding);
  json = edgex_valuedescriptor_write (result);
  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);
  result->id = ctx.buff;
  free (json);

  return result;
}
