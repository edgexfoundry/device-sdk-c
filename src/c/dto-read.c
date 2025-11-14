/*
 * Copyright (c) 2023-2024
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "dto-read.h"
#include "devutil.h"

static char *get_string_dfl (const iot_data_t *obj, const char *name, const char *dfl)
{
  const char *str = iot_data_string_map_get_string (obj, name);
  return strdup (str ? str : dfl);
}

static char *get_string (const iot_data_t *obj, const char *name)
{
  return get_string_dfl (obj, name, "");
}

edgex_device_adminstate edgex_adminstate_read (const iot_data_t *obj)
{
  const char *c = iot_data_string (obj);
  return (c && strcmp (c, "LOCKED") == 0) ? LOCKED : UNLOCKED;
}

static edgex_device_operatingstate edgex_operatingstate_read (const iot_data_t *obj)
{
  const char *c = iot_data_string (obj);
  return (c && strcmp (c, "DOWN") == 0) ? DOWN : UP;
}

static devsdk_protocols *edgex_protocols_read (const iot_data_t *obj)
{
  devsdk_protocols *result = NULL;
  if (obj && (iot_data_type(obj) == IOT_DATA_MAP))
  {
    iot_data_map_iter_t iter;
    iot_data_map_iter (obj, &iter);
    while (iot_data_map_iter_next (&iter))
    {
      devsdk_protocols *prot = malloc (sizeof (devsdk_protocols));
      prot->name = strdup (iot_data_map_iter_string_key (&iter));
      prot->properties = iot_data_add_ref (iot_data_map_iter_value (&iter));
      prot->next = result;
      result = prot;
    }
  }
  return result;
}

static edgex_device_autoevents *edgex_autoevent_read (const iot_data_t *obj)
{
  edgex_device_autoevents *result = calloc (1, sizeof (edgex_device_autoevents));
  result->resource = get_string (obj, "sourceName");
  result->onChange = iot_data_string_map_get_bool (obj, "onChange", false);
  iot_data_string_map_get_number (obj, "onChangeThreshold", IOT_DATA_FLOAT64, &result->onChangeThreshold);
  result->interval = get_string  (obj, "interval");
  return result;
}

static edgex_device_autoevents *edgex_autoevents_read (const iot_data_t *obj)
{
  edgex_device_autoevents *result = NULL;
  const iot_data_t *aes = iot_data_string_map_get (obj, "autoEvents");
  if (aes)
  {
    iot_data_vector_iter_t iter;
    iot_data_vector_iter (aes, &iter);
    while (iot_data_vector_iter_next (&iter))
    {
      edgex_device_autoevents *temp = edgex_autoevent_read (iot_data_vector_iter_value (&iter));
      temp->next = result;
      result = temp;
    }
  }
  return result;
}

edgex_device *edgex_device_read (const iot_data_t *obj)
{
  edgex_device *result = calloc (1, sizeof (edgex_device));
  result->name = get_string (obj, "name");
  result->profile = calloc (1, sizeof (edgex_deviceprofile));
  result->profile->name = get_string (obj, "profileName");
  result->servicename = get_string (obj, "serviceName");
  result->protocols = edgex_protocols_read (iot_data_string_map_get (obj, "protocols"));
  result->adminState = edgex_adminstate_read (iot_data_string_map_get (obj, "adminState"));
  result->description = get_string (obj, "description");
  result->operatingState = edgex_operatingstate_read (iot_data_string_map_get (obj, "operatingState"));
  result->autos = edgex_autoevents_read (obj);
  result->devimpl = calloc (1, sizeof (devsdk_device_t));
  result->devimpl->name = result->name;
  result->labels = edgex_labels_read(obj);

  return result;
}

edgex_watcher *edgex_pw_read (const iot_data_t *obj)
{
  edgex_watcher *result = calloc (1, sizeof (edgex_watcher));
  const iot_data_t *ddprops = iot_data_string_map_get (obj, "discoveredDevice");

  result->name = get_string (obj, "name");
  result->profile = get_string (ddprops, "profileName");
  result->identifiers = iot_data_add_ref (iot_data_string_map_get (obj, "identifiers"));
  result->blocking_identifiers =  iot_data_add_ref (iot_data_string_map_get (obj, "blockingIdentifiers"));
  result->autoevents = edgex_autoevents_read (ddprops);
  result->adminstate = edgex_adminstate_read (iot_data_string_map_get (ddprops, "adminState"));
  result->enabled = (edgex_adminstate_read (iot_data_string_map_get (obj, "adminState")) == UNLOCKED);
  return result;
}

edgex_watcher *edgex_pws_read (const iot_data_t *obj)
{
  edgex_watcher *result = NULL;
  const iot_data_t *pws = iot_data_string_map_get (obj, "provisionWatchers");
  if (pws)
  {
    iot_data_vector_iter_t iter;
    iot_data_vector_iter (pws, &iter);
    while (iot_data_vector_iter_next (&iter))
    {
      edgex_watcher *temp = edgex_pw_read (iot_data_vector_iter_value (&iter));
      temp->next = result;
      result = temp;
    }
  }
  return result;
}

static void edgex_get_readwrite (const iot_data_t *object, bool *read, bool *write)
{
  const char *rwstring = iot_data_string_map_get_string (object, "readWrite");
  if (rwstring && *rwstring)
  {
    *read = strchr (rwstring, 'R');
    *write = strchr (rwstring, 'W');
  }
  else
  {
    *read = true;
    *write = true;
  }
}

static void edgex_get_transformArg (const iot_data_t *obj, const char *name, iot_typecode_t type, edgex_transformArg *res)
{
  res->enabled = false;
  if (type.type >= IOT_DATA_INT8 && type.type <= IOT_DATA_UINT64)
  {
    int64_t i = 0;
    if (iot_data_string_map_get_number(obj, name, IOT_DATA_INT64, &i))
    {
      res->enabled = true;
      res->value.ival = i;
    }
  }
  else if (type.type == IOT_DATA_FLOAT32 || type.type == IOT_DATA_FLOAT64)
  {
    double d = 0;
    if (iot_data_string_map_get_number(obj, name, IOT_DATA_FLOAT64, &d))
    {
      res->enabled = true;
      res->value.dval = d;
    }
  }
}

static const char *proptypes[] =
{
  "Int8", "Uint8", "Int16", "Uint16", "Int32", "Uint32", "Int64", "Uint64",
  "Float32", "Float64", "Bool", "Unused1", "String", "Unused2", "Binary", "Unused3", "Unused4", "Unused5", "Object"
};

static const char *arrProptypes[] =
{
  "Int8Array", "Uint8Array", "Int16Array", "Uint16Array", "Int32Array", "Uint32Array", "Int64Array", "Uint64Array",
  "Float32Array", "Float64Array", "BoolArray"
};

static void typecode_from_edgex_name (iot_typecode_t *res, const char *str)
{
  for (int i = 0; i < sizeof (arrProptypes) / sizeof (*arrProptypes); i++)
  {
    if (strcmp (str, arrProptypes[i]) == 0)
    {
      res->type = IOT_DATA_ARRAY;
      res->element_type = i;
      res->key_type = IOT_DATA_INVALID;
      return;
    }
  }
  for (int i = 0; i < sizeof (proptypes) / sizeof (*proptypes); i++)
  {
    if (strcmp (str, proptypes[i]) == 0 && strncmp (str, "Unused", 5) != 0)
    {
      res->type = i;
      res->element_type = (i == IOT_DATA_MAP) ? IOT_DATA_MULTI : IOT_DATA_INVALID;
      res->key_type = (i == IOT_DATA_MAP) ? IOT_DATA_STRING : IOT_DATA_INVALID;
      return;
    }
  }
}

static edgex_propertyvalue *propertyvalue_read (const iot_data_t *obj)
{
  edgex_propertyvalue *result = calloc (1, sizeof (edgex_propertyvalue));
  iot_typecode_t pt;
  typecode_from_edgex_name (&pt, iot_data_string_map_get_string (obj, "valueType"));
  edgex_get_transformArg (obj, "scale", pt, &result->scale);
  edgex_get_transformArg (obj, "offset", pt, &result->offset);
  edgex_get_transformArg (obj, "base", pt, &result->base);
  edgex_get_transformArg (obj, "mask", pt, &result->mask);
  edgex_get_transformArg (obj, "shift", pt, &result->shift);
  edgex_get_transformArg (obj, "minimum", pt, &result->minimum);
  edgex_get_transformArg (obj, "maximum", pt, &result->maximum);
  result->type = pt;
  edgex_get_readwrite (obj, &result->readable, &result->writable);
  result->defaultvalue = get_string (obj, "defaultValue");
  result->assertion = get_string (obj, "assertion");
  result->units = get_string (obj, "units");
  result->mediaType = get_string_dfl (obj, "mediaType", (pt.type == IOT_DATA_BINARY) ? "application/octet-stream" : "");
  return result;
}

static edgex_deviceresource *deviceresource_read (const iot_data_t *obj)
{
  edgex_deviceresource *result = malloc (sizeof (edgex_deviceresource));
  result->name = get_string (obj, "name");
  result->description = get_string (obj, "description");
  result->tag = get_string (obj, "tag");
  result->properties = propertyvalue_read (iot_data_string_map_get (obj, "properties"));
  result->attributes = iot_data_add_ref (iot_data_string_map_get (obj, "attributes"));
  result->parsed_attrs = NULL;
  result->next = NULL;
  return result;
}

static edgex_resourceoperation *edgex_resourceoperation_read (const iot_data_t *obj)
{
  edgex_resourceoperation *result = calloc (1, sizeof (edgex_resourceoperation));
  result->deviceResource = get_string (obj, "deviceResource");
  result->defaultValue = get_string (obj, "defaultValue");
  result->mappings = iot_data_add_ref (iot_data_string_map_get (obj, "mappings"));
  return result;
}

static edgex_devicecommand *devicecommand_read (const iot_data_t *obj)
{
  const iot_data_t *ops;
  iot_data_vector_iter_t iter;
  edgex_devicecommand *result = calloc (1, sizeof (edgex_devicecommand));
  edgex_resourceoperation **last_ptr = &result->resourceOperations;

  result->name = get_string (obj, "name");
  edgex_get_readwrite (obj, &result->readable, &result->writable);
  ops = iot_data_string_map_get (obj, "resourceOperations");
  iot_data_vector_iter (ops, &iter);
  while (iot_data_vector_iter_next (&iter))
  {
    edgex_resourceoperation *temp = edgex_resourceoperation_read (iot_data_vector_iter_value (&iter));
    *last_ptr = temp;
    last_ptr = &(temp->next);
  }
  return result;
}


edgex_deviceprofile *edgex_profile_read (const iot_data_t *obj)
{
  edgex_deviceprofile *result = calloc (1, sizeof (edgex_deviceprofile));
  edgex_deviceresource **last_ptr = &result->device_resources;
  edgex_devicecommand **last_ptr2 = &result->device_commands;
  const iot_data_t *vec;
  iot_data_vector_iter_t iter;

  result->name = get_string (obj, "name");
  result->description = get_string (obj, "description");
  result->manufacturer = get_string (obj, "manufacturer");
  result->model = get_string (obj, "model");
  vec = iot_data_string_map_get (obj, "deviceResources");
  iot_data_vector_iter (vec, &iter);
  while (iot_data_vector_iter_next (&iter))
  {
    edgex_deviceresource *temp = deviceresource_read (iot_data_vector_iter_value (&iter));
    *last_ptr = temp;
    last_ptr = &(temp->next);
  }
  vec = iot_data_string_map_get (obj, "deviceCommands");
  iot_data_vector_iter (vec, &iter);
  while (iot_data_vector_iter_next (&iter))
  {
    edgex_devicecommand *temp = devicecommand_read (iot_data_vector_iter_value (&iter));
    *last_ptr2 = temp;
    last_ptr2 = &(temp->next);
  }
  return result;
}

devsdk_strings *edgex_labels_read(const iot_data_t *obj)
{
  devsdk_strings *labels = NULL;
  const iot_data_t *ldata = iot_data_string_map_get (obj, "labels");
  if (ldata)
  {
    iot_data_vector_iter_t iter;
    iot_data_vector_iter (ldata, &iter);
    while (iot_data_vector_iter_prev (&iter))
    {
      labels = devsdk_strings_new (iot_data_vector_iter_string (&iter), labels);
    }
  }
  return labels;
}
