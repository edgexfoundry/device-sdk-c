/*
 * Copyright (c) 2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "reqdata.h"
#include "parson.h"
#include <cbor.h>

struct edgex_reqdata_t
{
  iot_logger_t *lc;
  JSON_Object *json;
  cbor_item_t *cbor;
};

edgex_reqdata_t *edgex_reqdata_parse (iot_logger_t *lc, const devsdk_http_request *req)
{
  edgex_reqdata_t *result = NULL;

  if (req->content_type && strcasecmp (req->content_type, CONTENT_CBOR) == 0)
  {
    struct cbor_load_result lr;
    cbor_item_t *cbor = cbor_load (req->data.bytes, req->data.size, &lr);
    if (cbor)
    {
      if (cbor_isa_map (cbor))
      {
        size_t i;
        struct cbor_pair *pairs = cbor_map_handle (cbor);
        for (i = 0; i < cbor_map_size (cbor); i++)
        {
          if (!cbor_isa_string (pairs[i].key))
          {
            iot_log_error (lc, "CBOR payload is not a String map");
            break;
          }
          if (cbor_isa_string (pairs[i].value))
          {
            // Null-terminate string data for consistency
            size_t sz = cbor_string_length (pairs[i].value);
            unsigned char *newstr = realloc (cbor_string_handle (pairs[i].value), sz + 1);
            newstr[sz] = '\0';
            cbor_string_set_handle (pairs[i].value, newstr, sz + 1);
          }
        }
        if (i == cbor_map_size (cbor))
        {
          result = calloc (1, sizeof (edgex_reqdata_t));
          result->lc = lc;
          result->cbor = cbor;
        }
      }
      else
      {
        iot_log_error (lc, "CBOR payload is not a Map");
      }
    }
    else
    {
      iot_log_error (lc, "Payload did not parse as CBOR");
    }
  }
  else
  {
    JSON_Value *jval = json_parse_string (req->data.bytes);
    if (jval)
    {
      result = calloc (1, sizeof (edgex_reqdata_t));
      result->lc = lc;
      result->json = json_value_get_object (jval);
    }
    else
    {
      iot_log_error (lc, "Payload did not parse as JSON");
    }
  }
  return result;
}

void edgex_reqdata_free (edgex_reqdata_t *data)
{
  if (data)
  {
    if (data->json)
    {
      json_value_free (json_object_get_wrapping_value (data->json));
    }
    else
    {
      cbor_decref (&data->cbor);
    }
    free (data);
  }
}

extern const char *edgex_reqdata_get (const edgex_reqdata_t *data, const char *name, const char *dfl)
{
  const char *result = NULL;
  if (data->json)
  {
    JSON_Value *value = json_object_get_value(data->json, name);
    if (value) {
      result = json_serialize_to_string(value);
    }
  }
  else
  {
    size_t nlen = strlen (name);
    struct cbor_pair *pairs = cbor_map_handle (data->cbor);
    for (size_t i = 0; i < cbor_map_size (data->cbor); i++)
    {
      if (cbor_string_length (pairs[i].key) == nlen && strncmp ((const char*)cbor_string_handle (pairs[i].key), name, nlen) == 0)
      {
        if (cbor_isa_string (pairs[i].value))
        {
          result = (const char*)cbor_string_handle (pairs[i].value);
        }
        else
        {
          iot_log_error (data->lc, "CBOR: data for %s was not a string", name);
        }
        break;
      }
    }
  }
  return result ? result : dfl;
}

extern iot_data_t *edgex_reqdata_get_binary (const edgex_reqdata_t *data, const char *name)
{
  iot_data_t *result = NULL;
  if (data->json)
  {
    const char *b64 = json_object_get_string (data->json, name);
    if (b64)
    {
      result = iot_data_alloc_array_from_base64 (b64);
      iot_data_array_to_binary (result);
    }
  }
  else
  {
    size_t nlen = strlen (name);
    struct cbor_pair *pairs = cbor_map_handle (data->cbor);
    for (size_t i = 0; i < cbor_map_size (data->cbor); i++)
    {
      if (cbor_string_length (pairs[i].key) == nlen && strncmp ((const char*)cbor_string_handle (pairs[i].key), name, nlen) == 0)
      {
        if (cbor_isa_bytestring (pairs[i].value))
        {
          size_t sz = cbor_bytestring_length (pairs[i].value);
          void *bytes = cbor_bytestring_handle (pairs[i].value);
          result = iot_data_alloc_binary (bytes, (uint32_t)sz, IOT_DATA_COPY);
        }
        else
        {
          iot_log_error (data->lc, "CBOR: data for %s was not a bytestring", name);
        }
        break;
      }
    }
  }
  return result;
}
