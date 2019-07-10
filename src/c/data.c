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
#include "edgex-time.h"
#include "device.h"
#include "transform.h"

#include <cbor.h>

edgex_event_cooked *edgex_data_process_event
(
  const char *device_name,
  const edgex_cmdinfo *commandinfo,
  edgex_device_commandresult *values,
  bool doTransforms
)
{
  edgex_event_cooked *result = NULL;
  bool useCBOR = false;
  uint64_t timenow = edgex_device_nanotime_monotonic ();
  for (uint32_t i = 0; i < commandinfo->nreqs; i++)
  {
    if (doTransforms)
    {
      edgex_transform_outgoing (&values[i], commandinfo->pvals[i], commandinfo->maps[i]);
    }
    const char *assertion = commandinfo->pvals[i]->assertion;
    if (assertion && *assertion)
    {
      char *reading = edgex_value_tostring (&values[i], commandinfo->pvals[i]->floatAsBinary);
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
    if (commandinfo->pvals[i]->type == Binary)
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
      cbor_item_t *crdg = cbor_new_definite_map (values[i].origin ? 3 : 2);

      cbor_item_t *cread;
      if (values[i].type == Binary)
      {
        cread = cbor_build_bytestring (values[i].value.binary_result.bytes, values[i].value.binary_result.size);
        cbor_map_add (crdg, (struct cbor_pair)
          { .key = cbor_move (cbor_build_string ("binaryValue")), .value = cbor_move (cread) });
      }
      else
      {
        cread = cbor_build_string (edgex_value_tostring (&values[i], commandinfo->pvals[i]->floatAsBinary));
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
      char *reading = edgex_value_tostring (&values[i], commandinfo->pvals[i]->floatAsBinary);

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
  edgex_error *err
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

void edgex_device_commandresult_free (edgex_device_commandresult *res, int n)
{
  if (res)
  {
    for (int i = 0; i < n; i++)
    {
      if (res[i].type == String)
      {
        free (res[i].value.string_result);
      }
      else if (res[i].type == Binary)
      {
        free (res[i].value.binary_result.bytes);
      }
    }
    free (res);
  }
}

edgex_device_commandresult *edgex_device_commandresult_dup (const edgex_device_commandresult *res, int n)
{
  edgex_device_commandresult *result = calloc (n, sizeof (edgex_device_commandresult));
  size_t sz;
  for (int i = 0; i < n; i++)
  {
    result[i].type = res[i].type;
    switch (res[i].type)
    {
      case Bool:
        result[i].value.bool_result = res[i].value.bool_result;
        break;
      case Uint8:
        result[i].value.ui8_result = res[i].value.ui8_result;
        break;
      case Uint16:
        result[i].value.ui16_result = res[i].value.ui16_result;
        break;
      case Uint32:
        result[i].value.ui32_result = res[i].value.ui32_result;
        break;
      case Uint64:
        result[i].value.ui64_result = res[i].value.ui64_result;
        break;
      case Int8:
        result[i].value.i8_result = res[i].value.i8_result;
        break;
      case Int16:
        result[i].value.i16_result = res[i].value.i16_result;
        break;
      case Int32:
        result[i].value.i32_result = res[i].value.i32_result;
        break;
      case Int64:
        result[i].value.i64_result = res[i].value.i64_result;
        break;
      case Float32:
        result[i].value.f32_result = res[i].value.f32_result;
        break;
      case Float64:
        result[i].value.f64_result = res[i].value.f64_result;
        break;
      case String:
        result[i].value.string_result = strdup (res[i].value.string_result);
        break;
      case Binary:
        sz = res[i].value.binary_result.size;
        result[i].value.binary_result.size = sz;
        result[i].value.binary_result.bytes = malloc (sz);
        memcpy (result[i].value.binary_result.bytes, res[i].value.binary_result.bytes, sz);
        break;
    }
  }
  return result;
}

bool edgex_device_commandresult_equal
  (const edgex_device_commandresult *lhs, const edgex_device_commandresult *rhs, int n)
{
  bool result = true;
  for (int i = 0; i < n; i++)
  {
    if (lhs[i].type != rhs[i].type)
    {
      result = false;
      break;
    }
    switch (lhs[i].type)
    {
      case Bool:
        result = (lhs[i].value.bool_result == rhs[i].value.bool_result);
        break;
      case Uint8:
        result = (lhs[i].value.ui8_result == rhs[i].value.ui8_result);
        break;
      case Uint16:
        result = (lhs[i].value.ui16_result == rhs[i].value.ui16_result);
        break;
      case Uint32:
        result = (lhs[i].value.ui32_result == rhs[i].value.ui32_result);
        break;
      case Uint64:
        result = (lhs[i].value.ui64_result == rhs[i].value.ui64_result);
        break;
      case Int8:
        result = (lhs[i].value.i8_result == rhs[i].value.i8_result);
        break;
      case Int16:
        result = (lhs[i].value.i16_result == rhs[i].value.i16_result);
        break;
      case Int32:
        result = (lhs[i].value.i32_result == rhs[i].value.i32_result);
        break;
      case Int64:
        result = (lhs[i].value.i64_result == rhs[i].value.i64_result);
        break;
      case Float32:
        result = (lhs[i].value.f32_result == rhs[i].value.f32_result);
        break;
      case Float64:
        result = (lhs[i].value.f64_result == rhs[i].value.f64_result);
        break;
      case String:
        result = (strcmp (lhs[i].value.string_result, rhs[i].value.string_result) == 0);
        break;
      case Binary:
        result =
        (
          lhs[i].value.binary_result.size == rhs[i].value.binary_result.size &&
          memcmp (lhs[i].value.binary_result.bytes, rhs[i].value.binary_result.bytes, lhs[i].value.binary_result.size) == 0
        );
        break;
    }
    if (result == false)
    {
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
  edgex_error *err
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
