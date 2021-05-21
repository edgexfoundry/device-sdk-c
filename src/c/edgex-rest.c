/*
 * Copyright (c) 2018-2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "edgex-rest.h"
#include "cmdinfo.h"
#include "autoevent.h"
#include "watchers.h"
#include "correlation.h"
#include "parson.h"
#include <microhttpd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define SAFE_STR(s) (s ? s : "NULL")
#define SAFE_STRDUP(s) (s ? strdup(s) : NULL)

static char *get_string_dfl (const JSON_Object *obj, const char *name, const char *dfl)
{
  const char *str = json_object_get_string (obj, name);
  return strdup (str ? str : dfl);
}

static char *get_string (const JSON_Object *obj, const char *name)
{
  return get_string_dfl (obj, name, "");
}

static const char *get_array_string (const JSON_Array *array, size_t index)
{
  const char *str = json_array_get_string (array, index);
  return str ? str : "";
}

static bool get_boolean (const JSON_Object *obj, const char *name, bool dflt)
{
  int i = json_object_get_boolean (obj, name);
  return (i == -1) ? dflt : i;
}

static devsdk_strings *array_to_strings (const JSON_Array *array)
{
  size_t count = json_array_get_count (array);
  size_t i;
  devsdk_strings *result = NULL;
  devsdk_strings *temp;
  devsdk_strings **last_ptr = &result;

  for (i = 0; i < count; i++)
  {
    temp = devsdk_strings_new (get_array_string (array, i), NULL);
    *last_ptr = temp;
    last_ptr = &(temp->next);
  }

  return result;
}

static JSON_Value *strings_to_array (const devsdk_strings *s)
{
  JSON_Value *result = json_value_init_array ();
  JSON_Array *array = json_value_get_array (result);
  while (s)
  {
    json_array_append_string (array, s->str);
    s = s->next;
  }
  return result;
}

devsdk_strings *devsdk_strings_dup (const devsdk_strings *strs)
{
  devsdk_strings *result = NULL;
  devsdk_strings *copy;
  devsdk_strings **last = &result;

  while (strs)
  {
    copy = malloc (sizeof (devsdk_strings));
    copy->str = strdup (strs->str);
    copy->next = NULL;
    *last = copy;
    last = &(copy->next);
    strs = strs->next;
  }

  return result;
}

void devsdk_strings_free (devsdk_strings *strs)
{
  while (strs)
  {
    devsdk_strings *current = strs;
    free (strs->str);
    strs = strs->next;
    free (current);
  }
}

static JSON_Value *nvpairs_write (const devsdk_nvpairs *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);

  for (const devsdk_nvpairs *nv = e; nv; nv = nv->next)
  {
    json_object_set_string (obj, nv->name, nv->value);
  }

  return result;
}

static devsdk_nvpairs *nvpairs_read (const JSON_Object *obj)
{
  devsdk_nvpairs *result = NULL;
  size_t count = json_object_get_count (obj);
  for (size_t i = 0; i < count; i++)
  {
    devsdk_nvpairs *nv = malloc (sizeof (devsdk_nvpairs));
    nv->name = strdup (json_object_get_name (obj, i));
    nv->value = strdup (json_value_get_string (json_object_get_value_at (obj, i)));
    nv->next = result;
    result = nv;
  }
  return result;
}

devsdk_nvpairs *devsdk_nvpairs_dup (const devsdk_nvpairs *p)
{
  devsdk_nvpairs *result = NULL;
  devsdk_nvpairs *copy;
  devsdk_nvpairs **last = &result;
  while (p)
  {
    copy = malloc (sizeof (devsdk_nvpairs));
    copy->name = strdup (p->name);
    copy->value = strdup (p->value);
    copy->next = NULL;
    *last = copy;
    last = &(copy->next);
    p = p->next;
  }
  return result;
}

void devsdk_nvpairs_free (devsdk_nvpairs *p)
{
  while (p)
  {
    devsdk_nvpairs *current = p;
    free (p->name);
    free (p->value);
    p = p->next;
    free (current);
  }
}

static const char *proptypes[] =
{
  "Int8", "Uint8", "Int16", "Uint16", "Int32", "Uint32", "Int64", "Uint64",
  "Float32", "Float64", "Bool", "String", "Binary", "unused1", "unused2",
  "Int8Array", "Uint8Array", "Int16Array", "Uint16Array", "Int32Array", "Uint32Array", "Int64Array", "Uint64Array",
  "Float32Array", "Float64Array", "BoolArray"
};

const char *edgex_propertytype_tostring (edgex_propertytype pt)
{
  return proptypes[pt];
}

bool edgex_propertytype_fromstring (edgex_propertytype *res, const char *str)
{
  if (str)
  {
    for (edgex_propertytype i = Edgex_Int8; i <= Edgex_BoolArray; i++)
    {
      if (i != Edgex_Unused1 && i != Edgex_Unused2 && strcmp (str, proptypes[i]) == 0)
      {
        *res = i;
        return true;
      }
    }
  }
  return false;
}

static const char *adstatetypes[] = { "LOCKED", "UNLOCKED" };

static const char *edgex_adminstate_tostring (edgex_device_adminstate ad)
{
  return adstatetypes[ad];
}

static edgex_device_adminstate edgex_adminstate_fromstring (const char *str)
{
  return (str && strcmp (str, adstatetypes[LOCKED]) == 0) ? LOCKED : UNLOCKED;
}

static const char *opstatetypes[] = { "UP", "DOWN" };

static const char *edgex_operatingstate_tostring
  (edgex_device_operatingstate op)
{
  return opstatetypes[op];
}

static edgex_device_operatingstate edgex_operatingstate_fromstring
  (const char *str)
{
  return (str && strcmp (str, opstatetypes[DOWN]) == 0) ? DOWN : UP;
}

static void edgex_get_readwrite (const JSON_Object *object, bool *read, bool *write)
{
  const char *rwstring = json_object_get_string (object, "readWrite");
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

static bool get_transformArg
(
  iot_logger_t *lc,
  const JSON_Object *obj,
  const char *name,
  edgex_propertytype type,
  edgex_transformArg *res
)
{
  const char *str;
  char *end = NULL;
  bool ok = true;

  res->enabled = false;
  str = json_object_get_string (obj, name);
  if (str && *str)
  {
    if (type >= Edgex_Int8 && type <= Edgex_Uint64)
    {
      errno = 0;
      int64_t i = strtol (str, &end, 0);
      if (errno || (end && *end))
      {
        iot_log_error (lc, "Unable to parse \"%s\" as integer for valueproperty \"%s\"", str, name);
        ok = false;
      }
      else
      {
        res->enabled = true;
        res->value.ival = i;
      }
    }
    else if (type == Edgex_Float32 || type == Edgex_Float64)
    {
      errno = 0;
      double d = strtod (str, &end);
      if (errno || (end && *end))
      {
        iot_log_error ( lc, "Unable to parse \"%s\" as float for valueproperty \"%s\"", str, name);
        ok = false;
      }
      else
      {
        res->enabled = true;
        res->value.dval = d;
      }
    }
    else
    {
      iot_log_error (lc, "Valueproperty \"%s\" specified for non-numeric data", name);
      ok = false;
    }
  }
  return ok;
}

static edgex_propertyvalue *propertyvalue_read
  (iot_logger_t *lc, const JSON_Object *obj)
{
  edgex_propertytype pt;
  edgex_propertyvalue *result = NULL;
  const char *tstr = json_object_get_string (obj, "valueType");
  if (edgex_propertytype_fromstring (&pt, tstr))
  {
    bool ok = true;
    result = malloc (sizeof (edgex_propertyvalue));
    memset (result, 0, sizeof (edgex_propertyvalue));
    ok &= get_transformArg (lc, obj, "scale", pt, &result->scale);
    ok &= get_transformArg (lc, obj, "offset", pt, &result->offset);
    ok &= get_transformArg (lc, obj, "base", pt, &result->base);
    ok &= get_transformArg (lc, obj, "mask", pt, &result->mask);
    ok &= get_transformArg (lc, obj, "shift", pt, &result->shift);
    if (result->mask.enabled || result->shift.enabled)
    {
      if (pt == Edgex_Float32 || pt == Edgex_Float64)
      {
        iot_log_error (lc, "Mask/Shift transform specified for float data");
        ok = false;
      }
    }
    ok &= get_transformArg (lc, obj, "minimum", pt, &result->minimum);
    ok &= get_transformArg (lc, obj, "maximum", pt, &result->maximum);
    if (!ok)
    {
      free (result);
      return NULL;
    }
    result->type = pt;
    edgex_get_readwrite (obj, &result->readable, &result->writable);
    result->defaultvalue = get_string (obj, "defaultValue");
    result->assertion = get_string (obj, "assertion");
    result->units = get_string (obj, "units");
    result->mediaType = get_string_dfl (obj, "mediaType", (pt == Edgex_Binary) ? "application/octet-stream" : "");
  }
  else
  {
    iot_log_error (lc, "Unable to parse \"%s\" as data type", tstr ? tstr : "(null)");
  }
  return result;
}

static edgex_propertyvalue *propertyvalue_dup (const edgex_propertyvalue *pv)
{
  edgex_propertyvalue *result = NULL;
  if (pv)
  {
    result = malloc (sizeof (edgex_propertyvalue));
    result->type = pv->type;
    result->readable = pv->readable;
    result->writable = pv->writable;
    result->minimum = pv->minimum;
    result->maximum = pv->maximum;
    result->defaultvalue = strdup (pv->defaultvalue);
    result->mask = pv->mask;
    result->shift = pv->shift;
    result->scale = pv->scale;
    result->offset = pv->offset;
    result->base = pv->base;
    result->assertion = strdup (pv->assertion);
    result->units = strdup (pv->units);
    result->mediaType = strdup (pv->mediaType);
  }
  return result;
}

static void propertyvalue_free (edgex_propertyvalue *e)
{
  free (e->defaultvalue);
  free (e->assertion);
  free (e->units);
  free (e->mediaType);
  free (e);
}

static edgex_deviceresource *deviceresource_read
  (iot_logger_t *lc, const JSON_Object *obj)
{
  edgex_deviceresource *result = NULL;
  edgex_propertyvalue *pv = propertyvalue_read (lc, json_object_get_object (obj, "properties"));
  char *name = get_string (obj, "name");

  if (pv)
  {
    JSON_Object *attributes_obj;

    result = malloc (sizeof (edgex_deviceresource));
    result->name = name;
    result->description = get_string (obj, "description");
    result->tag = get_string (obj, "tag");
    result->properties = pv;
    attributes_obj = json_object_get_object (obj, "attributes");
    result->attributes = nvpairs_read (attributes_obj);
    result->next = NULL;
  }
  else
  {
    iot_log_error (lc, "Error reading property for deviceResource %s", name);
  }
  return result;
}

static edgex_deviceresource *edgex_deviceresource_dup (const edgex_deviceresource *e)
{
  edgex_deviceresource *result = NULL;
  if (e)
  {
    result = malloc (sizeof (edgex_deviceresource));
    result->name = strdup (e->name);
    result->description = strdup (e->description);
    result->tag = strdup (e->tag);
    result->properties = propertyvalue_dup (e->properties);
    result->attributes = devsdk_nvpairs_dup (e->attributes);
    result->next = edgex_deviceresource_dup (e->next);
  }
  return result;
}

static void deviceresource_free (edgex_deviceresource *e)
{
  while (e)
  {
    edgex_deviceresource *current = e;
    free (e->name);
    free (e->description);
    free (e->tag);
    propertyvalue_free (e->properties);
    devsdk_nvpairs_free (e->attributes);
    e = e->next;
    free (current);
  }
}

static edgex_resourceoperation *resourceoperation_read (const JSON_Object *obj)
{
  edgex_resourceoperation *result = malloc (sizeof (edgex_resourceoperation));
  JSON_Object *mappings_obj;

  result->deviceResource = get_string (obj, "deviceResource");
  result->defaultValue = get_string (obj, "defaultValue");
  mappings_obj = json_object_get_object (obj, "mappings");
  result->mappings = nvpairs_read (mappings_obj);
  result->next = NULL;
  return result;
}

static edgex_resourceoperation *resourceoperation_dup
  (const edgex_resourceoperation *ro)
{
  edgex_resourceoperation *result = NULL;
  if (ro)
  {
    result = malloc (sizeof (edgex_resourceoperation));
    result->deviceResource = strdup (ro->deviceResource);
    result->defaultValue = strdup (ro->defaultValue);
    result->mappings = devsdk_nvpairs_dup (ro->mappings);
    result->next = resourceoperation_dup (ro->next);
  }
  return result;
}

static void resourceoperation_free (edgex_resourceoperation *e)
{
  while (e)
  {
    edgex_resourceoperation *current = e;
    free (e->deviceResource);
    free (e->defaultValue);
    devsdk_nvpairs_free (e->mappings);
    e = e->next;
    free (current);
  }
}

static edgex_devicecommand *devicecommand_read (const JSON_Object *obj)
{
  edgex_devicecommand *result = malloc (sizeof (edgex_devicecommand));
  size_t count;
  JSON_Array *array;
  edgex_resourceoperation **last_ptr = &result->resourceOperations;

  result->name = get_string (obj, "name");
  edgex_get_readwrite (obj, &result->readable, &result->writable);
  array = json_object_get_array (obj, "resourceOperations");
  result->resourceOperations = NULL;
  count = json_array_get_count (array);
  for (size_t i = 0; i < count; i++)
  {
    edgex_resourceoperation *temp = resourceoperation_read
      (json_array_get_object (array, i));
    *last_ptr = temp;
    last_ptr = &(temp->next);
  }

  result->next = NULL;
  return result;
}

static edgex_devicecommand *devicecommand_dup (const edgex_devicecommand *pr)
{
  edgex_devicecommand *result = NULL;
  if (pr)
  {
    result = malloc (sizeof (edgex_devicecommand));
    result->name = strdup (pr->name);
    result->readable = pr->readable;
    result->writable = pr->writable;
    result->resourceOperations = resourceoperation_dup (pr->resourceOperations);
    result->next = devicecommand_dup (pr->next);
  }
  return result;
}

static void devicecommand_free (edgex_devicecommand *e)
{
  while (e)
  {
    edgex_devicecommand *current = e;
    free (e->name);
    resourceoperation_free (e->resourceOperations);
    e = e->next;
    free (current);
  }
}

static bool resourceop_validate (iot_logger_t *lc, edgex_resourceoperation *ro, edgex_deviceresource *reslist)
{
  while (ro)
  {
    edgex_deviceresource *res = reslist;
    while (res)
    {
      if (strcmp (ro->deviceResource, res->name) == 0)
      {
        break;
      }
      res = res->next;
    }
    if (res == NULL)
    {
      iot_log_error (lc, "No deviceResource \"%s\" found", ro->deviceResource);
      return false;
    }
    ro = ro->next;
  }
  return true;
}

static edgex_deviceprofile *deviceprofile_read
  (iot_logger_t *lc, const JSON_Object *obj)
{
  edgex_deviceprofile *result = calloc (1, sizeof (edgex_deviceprofile));
  size_t count;
  JSON_Array *array;
  edgex_deviceresource **last_ptr = &result->device_resources;
  edgex_devicecommand **last_ptr2 = &result->device_commands;

  result->name = get_string (obj, "name");
  result->description = get_string (obj, "description");
  result->created = json_object_get_uint (obj, "created");
  result->modified = json_object_get_uint (obj, "modified");
  result->origin = json_object_get_uint (obj, "origin");
  result->manufacturer = get_string (obj, "manufacturer");
  result->model = get_string (obj, "model");
  result->labels = array_to_strings (json_object_get_array (obj, "labels"));
  array = json_object_get_array (obj, "deviceResources");
  count = json_array_get_count (array);
  for (size_t i = 0; i < count; i++)
  {
    edgex_deviceresource *temp = deviceresource_read
      (lc, json_array_get_object (array, i));
    if (temp)
    {
      *last_ptr = temp;
      last_ptr = &(temp->next);
    }
    else
    {
      iot_log_error (lc, "Parse error in device profile %s", result->name);
      edgex_deviceprofile_free (result);
      return NULL;
    }
  }
  array = json_object_get_array (obj, "deviceCommands");
  count = json_array_get_count (array);
  for (size_t i = 0; i < count; i++)
  {
    edgex_devicecommand *temp = devicecommand_read
      (json_array_get_object (array, i));
    if (resourceop_validate (lc, temp->resourceOperations, result->device_resources))
    {
      *last_ptr2 = temp;
      last_ptr2 = &(temp->next);
    }
    else
    {
      iot_log_error (lc, "Parse error in deviceCommand %s of device profile %s", temp->name, result->name);
      devicecommand_free (temp);
      edgex_deviceprofile_free (result);
      return NULL;
    }
  }
  return result;
}

edgex_deviceprofile *edgex_deviceprofile_read
  (iot_logger_t *lc, const char *json)
{
  edgex_deviceprofile *result = NULL;
  JSON_Value *val = json_parse_string (json);
  JSON_Object *obj;

  obj = json_value_get_object (val);

  if (obj)
  {
    result = deviceprofile_read (lc, obj);
  }

  json_value_free (val);

  return result;
}

static void cmdinfo_free (edgex_cmdinfo *inf)
{
  if (inf)
  {
    cmdinfo_free (inf->next);
    free (inf->reqs);
    free (inf->pvals);
    free (inf->maps);
    free (inf->dfls);
    free (inf);
  }
}

static edgex_device_autoevents *autoevent_read (const JSON_Object *obj)
{
  edgex_device_autoevents *result = malloc (sizeof (edgex_device_autoevents));
  result->resource = get_string (obj, "sourceName");
  result->onChange = get_boolean (obj, "onChange", false);
  result->interval = get_string  (obj, "interval");
  result->impl = NULL;
  result->next = NULL;
  return result;
}

static JSON_Value *autoevents_write (const edgex_device_autoevents *e)
{
  JSON_Value *result = json_value_init_array ();
  JSON_Array *arr = json_value_get_array (result);

  for (const edgex_device_autoevents *ae = e; ae; ae = ae->next)
  {
    JSON_Value *pval = json_value_init_object ();
    JSON_Object *pobj = json_value_get_object (pval);
    json_object_set_string (pobj, "sourceName", ae->resource);
    json_object_set_string (pobj, "interval", ae->interval);
    json_object_set_boolean (pobj, "onChange", ae->onChange);
    json_array_append_value (arr, pval);
  }
  return result;
}

static edgex_device_autoevents *autoevents_dup
  (const edgex_device_autoevents *e)
{
  edgex_device_autoevents *result = NULL;
  if (e)
  {
    result = malloc (sizeof (edgex_device_autoevents));
    result->resource = strdup (e->resource);
    result->interval = strdup (e->interval);
    result->onChange = e->onChange;
    result->impl = NULL;
    result->next = autoevents_dup (e->next);
  }
  return result;
}

void edgex_device_autoevents_free (edgex_device_autoevents *e)
{
  if (e)
  {
    free (e->resource);
    free (e->interval);
    edgex_device_autoevents_free (e->next);
    free (e);
  }
}

static devsdk_protocols *protocols_read (const JSON_Object *obj)
{
  devsdk_protocols *result = NULL;
  size_t count = json_object_get_count (obj);
  for (size_t i = 0; i < count; i++)
  {
    JSON_Value *pval = json_object_get_value_at (obj, i);
    devsdk_protocols *prot = malloc (sizeof (devsdk_protocols));
    prot->name = strdup (json_object_get_name (obj, i));
    prot->properties = nvpairs_read (json_value_get_object (pval));
    prot->next = result;
    result = prot;
  }
  return result;
}

static JSON_Value *protocols_write (const devsdk_protocols *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);

  for (const devsdk_protocols *prot = e; prot; prot = prot->next)
  {
    json_object_set_value (obj, prot->name, nvpairs_write (prot->properties));
  }
  return result;
}

devsdk_protocols *devsdk_protocols_dup (const devsdk_protocols *e)
{
  devsdk_protocols *result = NULL;
  for (const devsdk_protocols *p = e; p; p = p->next)
  {
    devsdk_protocols *newprot = malloc (sizeof (devsdk_protocols));
    newprot->name = strdup (p->name);
    newprot->properties = devsdk_nvpairs_dup (p->properties);
    newprot->next = result;
    result = newprot;
  }
  return result;
}

void devsdk_protocols_free (devsdk_protocols *e)
{
  if (e)
  {
    free (e->name);
    devsdk_nvpairs_free (e->properties);
    devsdk_protocols_free (e->next);
    free (e);
  }
}

static edgex_deviceservice *deviceservice_read (const JSON_Object *obj)
{
  edgex_deviceservice *result = malloc (sizeof (edgex_deviceservice));
  result->baseaddress = get_string (obj, "baseAddress");
  result->adminState = edgex_adminstate_fromstring
    (json_object_get_string (obj, "adminState"));
  result->description = get_string (obj, "description");
  result->labels = array_to_strings (json_object_get_array (obj, "labels"));
  result->name = get_string (obj, "name");
  result->origin = json_object_get_uint (obj, "origin");

  return result;
}

static JSON_Value *deviceservice_write (const edgex_deviceservice *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);

  json_object_set_string (obj, "apiVersion", "v2");
  json_object_set_string (obj, "baseaddress", e->baseaddress);
  json_object_set_string
    (obj, "adminState", edgex_adminstate_tostring (e->adminState));
  json_object_set_string (obj, "description", e->description);
  json_object_set_value (obj, "labels", strings_to_array (e->labels));
  json_object_set_string (obj, "name", e->name);
  json_object_set_uint (obj, "origin", e->origin);

  return result;
}

void edgex_deviceservice_free (edgex_deviceservice *e)
{
  if (e)
  {
    free (e->baseaddress);
    free (e->description);
    devsdk_strings_free (e->labels);
    free (e->name);
    free (e);
  }
}

edgex_deviceprofile *edgex_deviceprofile_dup (const edgex_deviceprofile *src)
{
  edgex_deviceprofile *dest = NULL;
  if (src)
  {
    dest = calloc (1, sizeof (edgex_deviceprofile));
    dest->name = strdup (src->name);
    dest->description = SAFE_STRDUP (src->description);
    dest->created = src->created;
    dest->modified = src->modified;
    dest->origin = src->origin;
    dest->manufacturer = SAFE_STRDUP (src->manufacturer);
    dest->model = SAFE_STRDUP (src->model);
    dest->labels = devsdk_strings_dup (src->labels);
    dest->device_resources = edgex_deviceresource_dup (src->device_resources);
    dest->device_commands = devicecommand_dup (src->device_commands);
  }
  return dest;
}

void edgex_deviceprofile_free (edgex_deviceprofile *e)
{
  while (e)
  {
    edgex_deviceprofile *next = e->next;
    free (e->name);
    free (e->description);
    free (e->manufacturer);
    free (e->model);
    devsdk_strings_free (e->labels);
    deviceresource_free (e->device_resources);
    devicecommand_free (e->device_commands);
    cmdinfo_free (e->cmdinfo);
    free (e);
    e = next;
  }
}

edgex_deviceservice *edgex_deviceservice_read (const char *json)
{
  edgex_deviceservice *result = NULL;
  JSON_Value *val = json_parse_string (json);
  JSON_Object *obj;

  obj = json_value_get_object (val);

  if (obj)
  {
    result = deviceservice_read (obj);
  }

  json_value_free (val);

  return result;
}

edgex_deviceservice *edgex_getDSresponse_read (const char *json)
{
  edgex_deviceservice *result = NULL;
  JSON_Value *val = json_parse_string (json);
  JSON_Object *obj;

  obj = json_value_get_object (val);

  if (obj)
  {
    JSON_Object *ds = json_object_get_object (obj, "service");
    if (ds)
    {
      result = deviceservice_read (ds);
    }
  }

  json_value_free (val);

  return result;
}

edgex_deviceprofile *edgex_getprofileresponse_read (iot_logger_t *lc, const char *json)
{
  edgex_deviceprofile *result = NULL;
  JSON_Value *val = json_parse_string (json);
  JSON_Object *obj;

  obj = json_value_get_object (val);

  if (obj)
  {
    JSON_Object *dp = json_object_get_object (obj, "profile");
    if (dp)
    {
      result = deviceprofile_read (lc, dp);
    }
  }

  json_value_free (val);

  return result;
}

JSON_Value *edgex_wrap_request_single (const char *objName, JSON_Value *payload)
{
  JSON_Value *val = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (val);

  json_object_set_string (obj, "apiVersion", "v2");
  json_object_set_value (obj, objName, payload);

  return val;
}

JSON_Value *edgex_wrap_request (const char *objName, JSON_Value *payload)
{
  JSON_Value *arrval = json_value_init_array ();
  JSON_Array *array = json_value_get_array (arrval);
  json_array_append_value (array, edgex_wrap_request_single (objName, payload));

  return arrval;
}

char *edgex_createDSreq_write (const edgex_deviceservice *ds)
{
  JSON_Value *val = edgex_wrap_request ("Service", deviceservice_write (ds));
  char *result = json_serialize_to_string (val);
  json_value_free (val);
  return result;
}

char *edgex_updateDSreq_write (const char *name, const char *baseaddr)
{
  JSON_Value *dsval = json_value_init_object ();
  JSON_Object *dsobj = json_value_get_object (dsval);

  json_object_set_string (dsobj, "apiVersion", "v2");
  json_object_set_string (dsobj, "name", name);
  json_object_set_string (dsobj, "baseAddress", baseaddr);

  JSON_Value *val = edgex_wrap_request ("Service", dsval);
  char *result = json_serialize_to_string (val);
  json_value_free (val);
  return result;
}

static edgex_device *device_read (const JSON_Object *obj)
{
  edgex_device *result = malloc (sizeof (edgex_device));
  result->name = get_string (obj, "name");
  result->profile = calloc (1, sizeof (edgex_deviceprofile));
  result->profile->name = get_string (obj, "profileName");
  result->servicename = get_string (obj, "serviceName");
  result->protocols = protocols_read
    (json_object_get_object (obj, "protocols"));
  result->adminState = edgex_adminstate_fromstring
    (json_object_get_string (obj, "adminState"));
  result->description = get_string (obj, "description");
  result->labels = array_to_strings (json_object_get_array (obj, "labels"));
  result->operatingState = edgex_operatingstate_fromstring
    (json_object_get_string (obj, "operatingState"));
  result->autos = NULL;
  JSON_Array *array = json_object_get_array (obj, "autoEvents");
  size_t count = json_array_get_count (array);
  for (size_t i = 0; i < count; i++)
  {
    edgex_device_autoevents *temp = autoevent_read
      (json_array_get_object (array, i));
    temp->next = result->autos;
    result->autos = temp;
  }
  result->next = NULL;

  return result;
}

static JSON_Value *device_write (const edgex_device *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);

  json_object_set_string (obj, "apiVersion", "v2");
  json_object_set_string (obj, "profileName", e->profile->name);
  json_object_set_string (obj, "serviceName", e->servicename);
  json_object_set_value (obj, "protocols", protocols_write (e->protocols));
  json_object_set_value (obj, "autoevents", autoevents_write (e->autos));
  json_object_set_string (obj, "adminState", edgex_adminstate_tostring (e->adminState));
  json_object_set_string (obj, "operatingState", edgex_operatingstate_tostring (e->operatingState));
  json_object_set_string (obj, "name", e->name);
  json_object_set_string (obj, "description", e->description);
  json_object_set_value (obj, "labels", strings_to_array (e->labels));
  json_object_set_uint (obj, "origin", e->origin);

  return result;
}

edgex_device *edgex_device_dup (const edgex_device *e)
{
  edgex_device *result = malloc (sizeof (edgex_device));
  result->name = strdup (e->name);
  result->description = strdup (e->description);
  result->labels = devsdk_strings_dup (e->labels);
  result->protocols = devsdk_protocols_dup (e->protocols);
  result->autos = autoevents_dup (e->autos);
  result->adminState = e->adminState;
  result->operatingState = e->operatingState;
  result->origin = e->origin;
  result->servicename = strdup (e->servicename);
  result->profile = edgex_deviceprofile_dup (e->profile);
  result->next = NULL;
  return result;
}

char *edgex_createdevicereq_write (const edgex_device *dev)
{
  JSON_Value *val = edgex_wrap_request ("Device", device_write (dev));
  char *result = json_serialize_to_string (val);
  json_value_free (val);
  return result;
}

edgex_device *edgex_createdevicereq_read (const char *json)
{
  edgex_device *result = NULL;
  JSON_Value *val = json_parse_string (json);
  JSON_Object *obj;

  obj = json_value_get_object (val);

  if (obj)
  {
    JSON_Object *devobj = json_object_get_object (obj, "device");
    if (devobj)
    {
      result = device_read (devobj);
    }
  }

  json_value_free (val);

  return result;
}

iot_typecode_t *edgex_propertytype_totypecode (edgex_propertytype pt)
{
  if (pt <= Edgex_String)
  {
    return iot_typecode_alloc_basic (pt);
  }
  if (pt == Edgex_Binary)
  {
    return iot_typecode_alloc_array (IOT_DATA_UINT8);
  }
  else
  {
    return iot_typecode_alloc_array (pt - (Edgex_Int8Array - Edgex_Int8));
  }
}

devsdk_device_resources *edgex_profile_toresources (const edgex_deviceprofile *p)
{
  devsdk_device_resources *result = NULL;

  for (const edgex_deviceresource *r = p->device_resources; r; r = r->next)
  {
    devsdk_device_resources *entry = malloc (sizeof (devsdk_device_resources));
    entry->resname = strdup (r->name);
    entry->attributes = devsdk_nvpairs_dup ((const devsdk_nvpairs *)r->attributes);
    entry->type = edgex_propertytype_totypecode (r->properties->type);
    entry->readable = r->properties->readable;
    entry->writable = r->properties->writable;
    entry->next = result;
    result = entry;
  }
  return result;
}

devsdk_devices *edgex_device_todevsdk (const edgex_device *e)
{
  devsdk_devices *result = malloc (sizeof (devsdk_devices));
  result->devname = strdup (e->name);
  result->protocols = devsdk_protocols_dup (e->protocols);
  result->resources = edgex_profile_toresources (e->profile);
  result->next = NULL;
  return result;
}

void edgex_device_free (edgex_device *e)
{
  while (e)
  {
    edgex_device *current = e;
    devsdk_protocols_free (e->protocols);
    edgex_device_autoevents_free (e->autos);
    free (e->description);
    devsdk_strings_free (e->labels);
    free (e->name);
    if (e->profile)
    {
      edgex_deviceprofile_free (e->profile);
    }
    free (e->servicename);
    e = e->next;
    free (current);
  }
}

char *edgex_device_write (const edgex_device *e)
{
  char *result;
  JSON_Value *val;

  val = device_write (e);
  result = json_serialize_to_string (val);
  json_value_free (val);
  return result;
}

char *edgex_device_write_sparse
(
  const char * name,
  const char * description,
  const devsdk_strings * labels,
  const char * profile_name
)
{
  char *json;
  JSON_Value *val;
  JSON_Value *jval = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (jval);

  json_object_set_string (obj, "name", name);
  if (description) { json_object_set_string (obj, "description", description); }
  if (profile_name) { json_object_set_string (obj, "profileName", profile_name); }
  if (labels)
  {
    json_object_set_value (obj, "labels", strings_to_array (labels));
  }
  val = edgex_wrap_request ("Device", jval);
  json = json_serialize_to_string (val);
  json_value_free (val);

  return json;
}

char *edgex_updateDevOpreq_write (const char *name, edgex_device_operatingstate opstate)
{
  char *json;
  JSON_Value *val;
  JSON_Value *jval = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (jval);

  json_object_set_string (obj, "name", name);
  json_object_set_string (obj, "operatingstate", edgex_operatingstate_tostring (opstate));
  val = edgex_wrap_request ("Device", jval);
  json = json_serialize_to_string (val);
  json_value_free (val);

  return json;
}

char *edgex_updateDevLCreq_write (const char *name, uint64_t lastconnected)
{
  char *json;
  JSON_Value *val;
  JSON_Value *jval = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (jval);

  json_object_set_string (obj, "name", name);
  json_object_set_uint (obj, "lastConnected", lastconnected);
  val = edgex_wrap_request ("Device", jval);
  json = json_serialize_to_string (val);
  json_value_free (val);

  return json;
}


edgex_device *edgex_devices_read (iot_logger_t *lc, const char *json)
{
  edgex_device *result = NULL;
  JSON_Value *val = json_parse_string (json);
  JSON_Object *obj = json_value_get_object (val);

  JSON_Array *array = json_object_get_array (obj, "devices");
  edgex_device **last_ptr = &result;

  if (array)
  {
    size_t count = json_array_get_count (array);
    for (size_t i = 0; i < count; i++)
    {
      edgex_device *temp = device_read (json_array_get_object (array, i));
      if (temp)
      {
        *last_ptr = temp;
        last_ptr = &(temp->next);
      }
    }
  }

  json_value_free (val);

  return result;
}

static edgex_blocklist *blocklist_read (const JSON_Object *obj)
{
  edgex_blocklist *result = NULL;
  size_t count = json_object_get_count (obj);
  for (size_t i = 0; i < count; i++)
  {
    edgex_blocklist *bl = malloc (sizeof (edgex_blocklist));
    bl->name = strdup (json_object_get_name (obj, i));
    bl->values = array_to_strings (json_value_get_array (json_object_get_value_at (obj, i)));
    bl->next = result;
    result = bl;
  }
  return result;
}

static edgex_blocklist *edgex_blocklist_dup (const edgex_blocklist *e)
{
  edgex_blocklist *result = NULL;
  edgex_blocklist **last = &result;
  for (const edgex_blocklist *bl = e; bl; bl = bl->next)
  {
    edgex_blocklist *elem = calloc (1, sizeof (edgex_blocklist));
    elem->name = strdup (bl->name);
    elem->values = devsdk_strings_dup (bl->values);
    *last = elem;
    last = &(elem->next);
  }
  return result;
}

static void edgex_blocklist_free (edgex_blocklist *e)
{
  while (e)
  {
    edgex_blocklist *current = e;
    free (e->name);
    devsdk_strings_free (e->values);
    e = e->next;
    free (current);
  }
}

static edgex_watcher *watcher_read (const JSON_Object *obj)
{
  edgex_watcher *result = calloc (1, sizeof (edgex_watcher));
  result->name = get_string (obj, "name");
  result->profile = get_string (obj, "profileName");
  JSON_Object *idobj = json_object_get_object (obj, "identifiers");
  if (idobj)
  {
    result->identifiers = nvpairs_read (idobj);
  }
  JSON_Object *blockObj = json_object_get_object (obj, "blockingIdentifiers");
  if (blockObj)
  {
    result->blocking_identifiers = blocklist_read (blockObj);
  }
  JSON_Array *aearray = json_object_get_array (obj, "autoEvents");
  size_t count = json_array_get_count (aearray);
  for (size_t i = 0; i < count; i++)
  {
    edgex_device_autoevents *temp = autoevent_read (json_array_get_object (aearray, i));
    temp->next = result->autoevents;
    result->autoevents = temp;
  }
  result->adminstate = edgex_adminstate_fromstring
    (json_object_get_string (obj, "adminState"));

  return result;
}

edgex_watcher *edgex_createPWreq_read (const char *json)
{
  edgex_watcher *result = NULL;
  JSON_Value *val = json_parse_string (json);
  JSON_Object *obj;

  obj = json_value_get_object (val);

  if (obj)
  {
    JSON_Object *pwobj = json_object_get_object (obj, "provisionWatcher");
    if (pwobj)
    {
      result = watcher_read (pwobj);
    }
  }

  json_value_free (val);

  return result;
}

edgex_watcher *edgex_watchers_read (const char *json)
{
  edgex_watcher *result = NULL;
  JSON_Value *val = json_parse_string (json);
  JSON_Object *obj = json_value_get_object (val);

  JSON_Array *array = json_object_get_array (obj, "provisionWatchers");
  edgex_watcher **last_ptr = &result;

  if (array)
  {
    size_t count = json_array_get_count (array);
    for (size_t i = 0; i < count; i++)
    {
      edgex_watcher *temp = watcher_read (json_array_get_object (array, i));
      if (temp)
      {
        *last_ptr = temp;
        last_ptr = &(temp->next);
      }
    }
  }

  json_value_free (val);

  return result;
}

edgex_watcher *edgex_watcher_dup (const edgex_watcher *e)
{
  edgex_watcher *res = malloc (sizeof (edgex_watcher));
  res->regs = NULL;
  res->name = strdup (e->name);
  res->identifiers = devsdk_nvpairs_dup (e->identifiers);
  res->blocking_identifiers = edgex_blocklist_dup (e->blocking_identifiers);
  res->autoevents = autoevents_dup (e->autoevents);
  res->profile = strdup (e->profile);
  res->adminstate = e->adminstate;
  res->next = NULL;
  return res;
}

void edgex_watcher_free (edgex_watcher *e)
{
  edgex_watcher *ew = e;
  while (ew)
  {
    edgex_watcher *next = ew->next;
    free (ew->name);
    devsdk_nvpairs_free (ew->identifiers);
    edgex_watcher_regexes_free (ew->regs);
    edgex_blocklist_free (ew->blocking_identifiers);
    edgex_device_autoevents_free (ew->autoevents);
    free (ew->profile);
    free (ew);
    ew = next;
  }
}

static JSON_Value *edgex_parse_json (devsdk_http_data d)
{
  return json_parse_string ((char *)d.bytes);
}

static edgex_baserequest *baserequest_read (JSON_Object *obj)
{
  edgex_baserequest *res = malloc (sizeof (edgex_baserequest));
  res->requestId = get_string (obj, "requestId");
  return res;
}

edgex_baserequest *edgex_baserequest_read (devsdk_http_data d)
{
  JSON_Value *val = edgex_parse_json (d);
  JSON_Object *obj = json_value_get_object (val);
  edgex_baserequest *res = baserequest_read (obj);
  json_value_free (val);
  return res;
}

void edgex_baserequest_free (edgex_baserequest *e)
{
  free (e->requestId);
  free (e);
}


void edgex_baseresponse_populate (edgex_baseresponse *e, const char *version, int code, const char *msg)
{
  e->apiVersion = version;
  e->requestId = edgex_device_get_crlid ();
  e->statusCode = code;
  e->message = msg;
}

static void value_write (JSON_Value *val, devsdk_http_reply *reply)
{
  char *result = json_serialize_to_string (val);
  json_value_free (val);
  reply->data.bytes = result;
  reply->data.size = strlen (result);
  reply->code = MHD_HTTP_OK;
  reply->content_type = CONTENT_JSON;
}

static JSON_Value *baseresponse_write (const edgex_baseresponse *br)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);
  json_object_set_string (obj, "apiVersion", br->apiVersion);
  json_object_set_string (obj, "requestId", br->requestId);
  json_object_set_uint (obj, "statusCode", br->statusCode);
  if (br->message)
  {
    json_object_set_string (obj, "message", br->message);
  }
  return result;
}

void edgex_baseresponse_write (const edgex_baseresponse *br, devsdk_http_reply *reply)
{
  JSON_Value *val = baseresponse_write (br);
  value_write (val, reply);
}

edgex_errorresponse *edgex_errorresponse_create (uint64_t code, char *msg)
{
  edgex_errorresponse *res = malloc (sizeof (edgex_errorresponse));
  res->statusCode = code;
  res->message = msg;
  res->requestId = "";
  res->apiVersion = "v2";
  return res;
}

void edgex_errorresponse_write (const edgex_errorresponse *er, devsdk_http_reply *reply)
{
  JSON_Value *val = baseresponse_write (er);
  value_write (val, reply);
  reply->code = er->statusCode;
}

void edgex_errorresponse_free (edgex_errorresponse *e)
{
  if (e)
  {
    free ((char *)e->message);
    free (e);
  }
}

char *edgex_id_from_response (const char *response)
{
  char *result = NULL;
  JSON_Value *val = json_parse_string (response);
  if (val)
  {
    JSON_Array *arr = json_array (val);
    if (arr)
    {
      JSON_Object *obj = json_array_get_object (arr, 0);
      if (obj)
      {
        result = SAFE_STRDUP (json_object_get_string (obj, "id"));
      }
    }
    json_value_free (val);
  }
  return result;
}

static JSON_Value *pingresponse_write (const edgex_pingresponse *pr)
{
  char buff[128];
  struct tm ftm;
  localtime_r (&pr->timestamp, &ftm);
  strftime (buff, sizeof (buff), "%a, %d %b %Y %H:%M:%S %Z", &ftm);

  JSON_Value *result = baseresponse_write ((const edgex_baseresponse *)pr);
  JSON_Object *obj = json_value_get_object (result);
  json_object_set_string (obj, "timestamp", buff);
  return result;
}

void edgex_pingresponse_write (const edgex_pingresponse *pr, devsdk_http_reply *reply)
{
  JSON_Value *val = pingresponse_write (pr);
  value_write (val, reply);
}

static JSON_Value *configresponse_write (const edgex_configresponse *cr)
{
  JSON_Value *result = baseresponse_write ((const edgex_baseresponse *)cr);
  JSON_Object *obj = json_value_get_object (result);
  json_object_set_value (obj, "config", cr->config);
  return result;
}

void edgex_configresponse_write (const edgex_configresponse *cr, devsdk_http_reply *reply)
{
  JSON_Value *val = configresponse_write (cr);
  value_write (val, reply);
}

void edgex_configresponse_free (edgex_configresponse *cr)
{
  free (cr);
}

static JSON_Value *metricsesponse_write (const edgex_metricsresponse *mr)
{
  JSON_Value *result = baseresponse_write ((const edgex_baseresponse *)mr);
  JSON_Object *obj = json_value_get_object (result);
#ifdef __GNU_LIBRARY__
  json_object_set_uint (obj, "Alloc", mr->alloc);
  json_object_set_uint (obj, "TotalAlloc", mr->totalloc);
  json_object_set_number (obj, "CpuLoadAvg", mr->loadavg);
#endif
  json_object_set_number (obj, "CpuTime", mr->cputime);
  json_object_set_number (obj, "CpuAvgUsage", mr->cpuavg);

  return result;
}

void edgex_metricsresponse_write (const edgex_metricsresponse *mr, devsdk_http_reply *reply)
{
  JSON_Value *val = metricsesponse_write (mr);
  value_write (val, reply);
}
