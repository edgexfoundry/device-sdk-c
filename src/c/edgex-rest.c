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

static const char *rwstrings[2][2] = { { "", "W" }, { "R", "RW" } };

static char *get_string_dfl (const JSON_Object *obj, const char *name, const char *dfl)
{
  const char *str = json_object_get_string (obj, name);
  return strdup (str ? str : dfl);
}

static char *get_string (const JSON_Object *obj, const char *name)
{
  return get_string_dfl (obj, name, "");
}

static char *get_array_string (const JSON_Array *array, size_t index)
{
  const char *str = json_array_get_string (array, index);
  return strdup (str ? str : "");
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
    temp = (devsdk_strings *) malloc (sizeof (devsdk_strings));
    temp->str = get_array_string (array, i);
    temp->next = NULL;
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
  "int8", "uint8", "int16", "uint16", "int32", "uint32", "int64",
  "uint64", "float32", "float64", "bool", "string", "binary", "unused1", "unused2",
  "int8array", "uint8array", "int16array", "uint16array", "int32array", "uint32array", "int64array",
  "uint64array", "float32array", "float64array", "boolarray"
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
      if (i != Edgex_Unused1 && i != Edgex_Unused2 && strcasecmp (str, proptypes[i]) == 0)
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

static const char *opstatetypes[] = { "ENABLED", "DISABLED" };

static const char *edgex_operatingstate_tostring
  (edgex_device_operatingstate op)
{
  return opstatetypes[op];
}

static edgex_device_operatingstate edgex_operatingstate_fromstring
  (const char *str)
{
  return (str && strcmp (str, opstatetypes[DISABLED]) == 0) ? DISABLED : ENABLED;
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
  const char *fe;
  edgex_propertytype pt;
  edgex_propertyvalue *result = NULL;
  const char *tstr = json_object_get_string (obj, "type");
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
    const char *rwstring = json_object_get_string (obj, "readWrite");
    if (rwstring && *rwstring)
    {
      result->readable = strchr (rwstring, 'R');
      result->writable = strchr (rwstring, 'W');
    }
    else
    {
      result->readable = true;
      result->writable = true;
    }
    result->defaultvalue = get_string (obj, "defaultValue");
    result->lsb = get_string (obj, "lsb");
    result->assertion = get_string (obj, "assertion");
    result->precision = get_string (obj, "precision");
    fe = json_object_get_string (obj, "floatEncoding");
#ifdef LEGIBLE_FLOATS
    result->floatAsBinary = fe && (strcmp (fe, "base64") == 0) && (strcmp (fe, "base64le") == 0);
#else
    result->floatAsBinary = !(fe && (strcmp (fe, "eNotation") == 0));
#endif
    result->mediaType = get_string_dfl (obj, "mediaType", (pt == Edgex_Binary) ? "application/octet-stream" : "");
  }
  else
  {
    iot_log_error (lc, "Unable to parse \"%s\" as data type", tstr ? tstr : "(null)");
  }
  return result;
}

static void set_arg
(
  JSON_Object *obj,
  const char *name,
  edgex_transformArg arg,
  iot_data_type_t pt
)
{
  if (arg.enabled)
  {
    char tmp[32];
    if (pt == IOT_DATA_FLOAT32 || pt == IOT_DATA_FLOAT64)
    {
      sprintf (tmp, "%f", arg.value.dval);
    }
    else
    {
      sprintf (tmp, PRId64, arg.value.ival);
    }
    json_object_set_string (obj, name, tmp);
  }
}

static JSON_Value *propertyvalue_write (const edgex_propertyvalue *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);
  json_object_set_string (obj, "type", edgex_propertytype_tostring (e->type));
  json_object_set_string
    (obj, "readWrite", rwstrings[e->readable][e->writable]);
  set_arg (obj, "minimum", e->minimum, e->type);
  set_arg (obj, "maximum", e->maximum, e->type);
  json_object_set_string (obj, "defaultValue", e->defaultvalue);
  json_object_set_string (obj, "lsb", e->lsb);
  set_arg (obj, "mask", e->mask, e->type);
  set_arg (obj, "shift", e->shift, e->type);
  set_arg (obj, "scale", e->scale, e->type);
  set_arg (obj, "offset", e->offset, e->type);
  set_arg (obj, "base", e->base, e->type);
  json_object_set_string (obj, "assertion", e->assertion);
  json_object_set_string (obj, "precision", e->precision);
  json_object_set_string
    (obj, "floatEncoding", e->floatAsBinary ? "base64" : "eNotation");
  json_object_set_string (obj, "mediaType", e->mediaType);
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
    result->lsb = strdup (pv->lsb);
    result->mask = pv->mask;
    result->shift = pv->shift;
    result->scale = pv->scale;
    result->offset = pv->offset;
    result->base = pv->base;
    result->assertion = strdup (pv->assertion);
    result->precision = strdup (pv->precision);
    result->mediaType = strdup (pv->mediaType);
    result->floatAsBinary = pv->floatAsBinary;
  }
  return result;
}

static void propertyvalue_free (edgex_propertyvalue *e)
{
  free (e->defaultvalue);
  free (e->lsb);
  free (e->assertion);
  free (e->precision);
  free (e->mediaType);
  free (e);
}

static edgex_units *units_read (const JSON_Object *obj)
{
  edgex_units *result = malloc (sizeof (edgex_units));
  result->type = get_string (obj, "type");
  result->readwrite = get_string (obj, "readWrite");
  result->defaultvalue = get_string (obj, "defaultValue");
  return result;
}

static JSON_Value *units_write (const edgex_units *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);
  json_object_set_string (obj, "type", e->type);
  json_object_set_string (obj, "readWrite", e->readwrite);
  json_object_set_string (obj, "defaultValue", e->defaultvalue);
  return result;
}

static edgex_units *units_dup (const edgex_units *u)
{
  edgex_units *result = NULL;
  if (u)
  {
    result = malloc (sizeof (edgex_units));
    result->type = strdup (u->type);
    result->readwrite = strdup (u->readwrite);
    result->defaultvalue = strdup (u->defaultvalue);
  }
  return result;
}

static void units_free (edgex_units *e)
{
  free (e->type);
  free (e->readwrite);
  free (e->defaultvalue);
  free (e);
}

static edgex_profileproperty *profileproperty_read
  (iot_logger_t *lc, const JSON_Object *obj)
{
  edgex_propertyvalue *val;
  edgex_profileproperty *result = NULL;

  val = propertyvalue_read (lc, json_object_get_object (obj, "value"));
  if (val)
  {
    result = malloc (sizeof (edgex_profileproperty));
    result->value = val;
    result->units = units_read (json_object_get_object (obj, "units"));
  }
  return result;
}

static JSON_Value *profileproperty_write (edgex_profileproperty *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);
  json_object_set_value (obj, "value", propertyvalue_write (e->value));
  json_object_set_value (obj, "units", units_write (e->units));
  return result;
}

static edgex_profileproperty *profileproperty_dup (const edgex_profileproperty *pp)
{
  edgex_profileproperty *result = NULL;
  if (pp)
  {
    result = malloc (sizeof (edgex_profileproperty));
    result->value = propertyvalue_dup (pp->value);
    result->units = units_dup (pp->units);
  }
  return result;
}

static void profileproperty_free (edgex_profileproperty *e)
{
  propertyvalue_free (e->value);
  units_free (e->units);
  free (e);
}

static edgex_deviceresource *deviceresource_read
  (iot_logger_t *lc, const JSON_Object *obj)
{
  edgex_deviceresource *result = NULL;
  edgex_profileproperty *pp = profileproperty_read
    (lc, json_object_get_object (obj, "properties"));
  char *name = get_string (obj, "name");

  if (pp)
  {
    JSON_Object *attributes_obj;

    result = malloc (sizeof (edgex_deviceresource));
    result->name = name;
    result->description = get_string (obj, "description");
    result->tag = get_string (obj, "tag");
    result->properties = pp;
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

static JSON_Value *deviceresource_write (const edgex_deviceresource *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);
  json_object_set_string (obj, "name", e->name);
  json_object_set_string (obj, "description", e->description);
  json_object_set_string (obj, "tag", e->tag);
  json_object_set_value
    (obj, "properties", profileproperty_write (e->properties));
  json_object_set_value (obj, "attributes", nvpairs_write (e->attributes));
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
    result->properties = profileproperty_dup (e->properties);
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
    profileproperty_free (e->properties);
    devsdk_nvpairs_free (e->attributes);
    e = e->next;
    free (current);
  }
}

static edgex_resourceoperation *resourceoperation_read (const JSON_Object *obj)
{
  edgex_resourceoperation *result = malloc (sizeof (edgex_resourceoperation));
  JSON_Object *mappings_obj;

  result->index = get_string (obj, "index");
  result->operation = get_string (obj, "operation");
  result->object = get_string (obj, "object");
  result->deviceResource = get_string (obj, "deviceResource");
  result->property = get_string (obj, "property");
  result->parameter = get_string (obj, "parameter");
  result->resource = get_string (obj, "resource");
  result->deviceCommand = get_string (obj, "deviceCommand");
  result->secondary = array_to_strings
    (json_object_get_array (obj, "secondary"));
  mappings_obj = json_object_get_object (obj, "mappings");
  result->mappings = nvpairs_read (mappings_obj);
  result->next = NULL;
  if (strlen (result->deviceResource) == 0 && strlen (result->object) != 0)
  {
    free (result->deviceResource);
    result->deviceResource = strdup (result->object);
  }
  if (strlen (result->deviceCommand) == 0 && strlen (result->resource) != 0)
  {
    free (result->deviceCommand);
    result->deviceCommand = strdup (result->resource);
  }
  return result;
}

static JSON_Value *resourceoperation_write (const edgex_resourceoperation *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);

  json_object_set_string (obj, "index", e->index);
  json_object_set_string (obj, "operation", e->operation);
  json_object_set_string (obj, "object", e->object);
  json_object_set_string (obj, "deviceResource", e->deviceResource);
  json_object_set_string (obj, "property", e->property);
  json_object_set_string (obj, "parameter", e->parameter);
  json_object_set_string (obj, "resource", e->resource);
  json_object_set_string (obj, "deviceCommand", e->deviceCommand);
  json_object_set_value (obj, "secondary", strings_to_array (e->secondary));
  json_object_set_value (obj, "mappings", nvpairs_write (e->mappings));
  return result;
}

static edgex_resourceoperation *resourceoperation_dup
  (const edgex_resourceoperation *ro)
{
  edgex_resourceoperation *result = NULL;
  if (ro)
  {
    result = malloc (sizeof (edgex_resourceoperation));
    result->index = strdup (ro->index);
    result->operation = strdup (ro->operation);
    result->object = strdup (ro->object);
    result->deviceResource = strdup (ro->deviceResource);
    result->property = strdup (ro->property);
    result->parameter = strdup (ro->parameter);
    result->resource = strdup (ro->resource);
    result->deviceCommand = strdup (ro->deviceCommand);
    result->secondary = devsdk_strings_dup (ro->secondary);
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
    free (e->index);
    free (e->operation);
    free (e->object);
    free (e->deviceResource);
    free (e->property);
    free (e->parameter);
    free (e->resource);
    free (e->deviceCommand);
    devsdk_strings_free (e->secondary);
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
  edgex_resourceoperation **last_ptr = &result->set;
  edgex_resourceoperation **last_ptr2 = &result->get;

  result->name = get_string (obj, "name");
  array = json_object_get_array (obj, "set");
  result->set = NULL;
  count = json_array_get_count (array);
  for (size_t i = 0; i < count; i++)
  {
    edgex_resourceoperation *temp = resourceoperation_read
      (json_array_get_object (array, i));
    *last_ptr = temp;
    last_ptr = &(temp->next);
  }
  array = json_object_get_array (obj, "get");
  result->get = NULL;
  count = json_array_get_count (array);
  for (size_t i = 0; i < count; i++)
  {
    edgex_resourceoperation *temp = resourceoperation_read
      (json_array_get_object (array, i));
    *last_ptr2 = temp;
    last_ptr2 = &(temp->next);
  }

  result->next = NULL;
  return result;
}

static JSON_Value *devicecommand_write (const edgex_devicecommand *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);
  JSON_Value *array_val = json_value_init_array ();
  JSON_Array *array = json_value_get_array (array_val);
  JSON_Value *array_val2 = json_value_init_array ();
  JSON_Array *array2 = json_value_get_array (array_val);
  json_object_set_string (obj, "name", e->name);

  for (edgex_resourceoperation *temp = e->set; temp; temp = temp->next)
  {
    json_array_append_value (array, resourceoperation_write (temp));
  }

  json_object_set_value (obj, "set", array_val);
  for (edgex_resourceoperation *temp = e->get; temp; temp = temp->next)
  {
    json_array_append_value (array2, resourceoperation_write (temp));
  }

  json_object_set_value (obj, "get", array_val2);
  return result;
}

static edgex_devicecommand *devicecommand_dup (const edgex_devicecommand *pr)
{
  edgex_devicecommand *result = NULL;
  if (pr)
  {
    result = malloc (sizeof (edgex_devicecommand));
    result->name = strdup (pr->name);
    result->set = resourceoperation_dup (pr->set);
    result->get = resourceoperation_dup (pr->get);
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
    resourceoperation_free (e->set);
    resourceoperation_free (e->get);
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

  result->id = get_string (obj, "id");
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
    if
    (
      resourceop_validate (lc, temp->set, result->device_resources) &&
      resourceop_validate (lc, temp->get, result->device_resources)
    )
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

static JSON_Value *
deviceprofile_write (const edgex_deviceprofile *e, bool create)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);
  JSON_Value *array_val = json_value_init_array ();
  JSON_Array *array = json_value_get_array (array_val);
  JSON_Value *array_val2 = json_value_init_array ();
  JSON_Array *array2 = json_value_get_array (array_val2);

  if (!create)
  {
    json_object_set_string (obj, "id", e->id);
    json_object_set_uint (obj, "created", e->created);
    json_object_set_uint (obj, "modified", e->modified);
  }
  json_object_set_string (obj, "name", e->name);
  json_object_set_string (obj, "description", e->description);
  json_object_set_uint (obj, "origin", e->origin);
  json_object_set_string (obj, "manufacturer", e->manufacturer);
  json_object_set_string (obj, "model", e->model);
  json_object_set_value (obj, "labels", strings_to_array (e->labels));

  for (edgex_deviceresource *temp = e->device_resources; temp; temp = temp->next)
  {
    json_array_append_value (array, deviceresource_write (temp));
  }

  json_object_set_value (obj, "deviceResources", array_val);

  for (edgex_devicecommand *temp = e->device_commands; temp; temp = temp->next)
  {
    json_array_append_value (array2, devicecommand_write (temp));
  }

  json_object_set_value (obj, "deviceCommands", array_val2);
  return result;
}

static JSON_Value *deviceprofile_write_name (const edgex_deviceprofile *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);

  json_object_set_string (obj, "name", e->name);

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
  result->resource = get_string (obj, "resource");
  result->onChange = get_boolean (obj, "onChange", false);
  result->frequency = get_string  (obj, "frequency");
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
    json_object_set_string (pobj, "resource", ae->resource);
    json_object_set_string (pobj, "frequency", ae->frequency);
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
    result->frequency = strdup (e->frequency);
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
    free (e->frequency);
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

static edgex_addressable *addressable_read (const JSON_Object *obj)
{
  edgex_addressable *result = malloc (sizeof (edgex_addressable));
  result->address = get_string (obj, "address");
  result->created = json_object_get_uint (obj, "created");
  result->id = get_string (obj, "id");
  result->method = get_string (obj, "method");
  result->modified = json_object_get_uint (obj, "modified");
  result->name = get_string (obj, "name");
  result->origin = json_object_get_uint (obj, "origin");
  result->password = get_string (obj, "password");
  result->path = get_string (obj, "path");
  result->port = json_object_get_uint (obj, "port");
  result->protocol = get_string (obj, "protocol");
  result->publisher = get_string (obj, "publisher");
  result->topic = get_string (obj, "topic");
  result->user = get_string (obj, "user");

  return result;
}

static JSON_Value *addressable_write (const edgex_addressable *e, bool create)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);

  if (!create)
  {
    json_object_set_string (obj, "id", e->id);
    json_object_set_uint (obj, "created", e->created);
    json_object_set_uint (obj, "modified", e->modified);
  }

  json_object_set_string (obj, "address", e->address);
  json_object_set_string (obj, "method", e->method);
  json_object_set_string (obj, "name", e->name);
  json_object_set_uint (obj, "origin", e->origin);
  json_object_set_string (obj, "password", e->password);
  json_object_set_string (obj, "path", e->path);
  json_object_set_uint (obj, "port", e->port);
  json_object_set_string (obj, "protocol", e->protocol);
  json_object_set_string (obj, "publisher", e->publisher);
  json_object_set_string (obj, "topic", e->topic);
  json_object_set_string (obj, "user", e->user);

  return result;
}

edgex_addressable *edgex_addressable_dup (const edgex_addressable *addr)
{
  edgex_addressable *result = NULL;
  if (addr)
  {
    result = malloc (sizeof (edgex_addressable));
    result->name = SAFE_STRDUP (addr->name);
    result->id = SAFE_STRDUP (addr->id);
    result->address = SAFE_STRDUP (addr->address);
    result->method = SAFE_STRDUP (addr->method);
    result->path = SAFE_STRDUP (addr->path);
    result->protocol = SAFE_STRDUP (addr->protocol);
    result->user = SAFE_STRDUP (addr->user);
    result->password = SAFE_STRDUP (addr->password);
    result->topic = SAFE_STRDUP (addr->topic);
    result->publisher = SAFE_STRDUP (addr->publisher);
    result->created = addr->created;
    result->modified = addr->modified;
    result->origin = addr->origin;
    result->port = addr->port;
  }
  return result;
}

void edgex_addressable_free (edgex_addressable *e)
{
  if (e)
  {
    free (e->address);
    free (e->id);
    free (e->name);
    free (e->method);
    free (e->password);
    free (e->path);
    free (e->protocol);
    free (e->publisher);
    free (e->topic);
    free (e->user);
    free (e);
  }
}

static edgex_deviceservice *deviceservice_read (const JSON_Object *obj)
{
  edgex_deviceservice *result = malloc (sizeof (edgex_deviceservice));
  result->addressable = addressable_read
    (json_object_get_object (obj, "addressable"));;
  result->adminState = edgex_adminstate_fromstring
    (json_object_get_string (obj, "adminState"));
  result->created = json_object_get_uint (obj, "created");
  result->description = get_string (obj, "description");
  result->id = get_string (obj, "id");
  result->labels = array_to_strings (json_object_get_array (obj, "labels"));
  result->lastConnected = json_object_get_uint (obj, "lastConnected");
  result->lastReported = json_object_get_uint (obj, "lastReported");
  result->modified = json_object_get_uint (obj, "modified");
  result->name = get_string (obj, "name");
  result->operatingState = edgex_operatingstate_fromstring
    (json_object_get_string (obj, "operatingState"));
  result->origin = json_object_get_uint (obj, "origin");

  return result;
}

static JSON_Value *
deviceservice_write (const edgex_deviceservice *e, bool create)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);

  if (!create)
  {
    json_object_set_string (obj, "id", e->id);
    json_object_set_uint (obj, "created", e->created);
    json_object_set_uint (obj, "modified", e->modified);
  }

  json_object_set_value
    (obj, "addressable", addressable_write (e->addressable, false));
  json_object_set_string
    (obj, "adminState", edgex_adminstate_tostring (e->adminState));
  json_object_set_string (obj, "description", e->description);
  json_object_set_value (obj, "labels", strings_to_array (e->labels));
  json_object_set_uint (obj, "lastConnected", e->lastConnected);
  json_object_set_uint (obj, "lastReported", e->lastReported);
  json_object_set_string (obj, "name", e->name);
  json_object_set_string
    (obj, "operatingState", edgex_operatingstate_tostring (e->operatingState));
  json_object_set_uint (obj, "origin", e->origin);

  return result;
}

static JSON_Value *deviceservice_write_name (const edgex_deviceservice *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);

  json_object_set_string (obj, "name", e->name);

  return result;
}

edgex_deviceservice *edgex_deviceservice_dup (const edgex_deviceservice *e)
{
  edgex_deviceservice *res = malloc (sizeof (edgex_deviceservice));
  res->name = strdup (e->name);
  res->id = strdup (e->id);
  res->description = strdup (e->description);
  res->labels = devsdk_strings_dup (e->labels);
  res->addressable = edgex_addressable_dup (e->addressable);
  res->adminState = e->adminState;
  res->operatingState = e->operatingState;
  res->origin = e->origin;
  res->created = e->created;
  res->modified = e->modified;
  res->lastConnected = e->lastConnected;
  res->lastReported = e->lastReported;
  return res;
}

void edgex_deviceservice_free (edgex_deviceservice *e)
{
  if (e)
  {
    edgex_addressable_free (e->addressable);
    free (e->description);
    free (e->id);
    devsdk_strings_free (e->labels);
    free (e->name);
    free (e);
  }
}

char *edgex_deviceprofile_write (const edgex_deviceprofile *e, bool create)
{
  char *result;
  JSON_Value *val;

  val = deviceprofile_write (e, create);
  result = json_serialize_to_string (val);
  json_value_free (val);
  return result;
}

edgex_deviceprofile *edgex_deviceprofile_dup (const edgex_deviceprofile *src)
{
  edgex_deviceprofile *dest = NULL;
  if (src)
  {
    dest = calloc (1, sizeof (edgex_deviceprofile));
    dest->id = strdup (src->id);
    dest->name = strdup (src->name);
    dest->description = strdup (src->description);
    dest->created = src->created;
    dest->modified = src->modified;
    dest->origin = src->origin;
    dest->manufacturer = strdup (src->manufacturer);
    dest->model = strdup (src->model);
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
    free (e->id);
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

char *edgex_deviceservice_write (const edgex_deviceservice *e, bool create)
{
  char *result;
  JSON_Value *val;

  val = deviceservice_write (e, create);
  result = json_serialize_to_string (val);
  json_value_free (val);
  return result;
}

static edgex_device *device_read
  (iot_logger_t *lc, const JSON_Object *obj)
{
  char *name = get_string (obj, "name");
  edgex_deviceprofile *prof = deviceprofile_read (lc, json_object_get_object (obj, "profile"));
  if (prof == NULL)
  {
    iot_log_error (lc, "Device %s has an invalid profile: will not be processed", name);
    free (name);
    return NULL;
  }

  edgex_device *result = malloc (sizeof (edgex_device));
  result->name = name;
  result->profile = prof;
  result->protocols = protocols_read
    (json_object_get_object (obj, "protocols"));
  result->adminState = edgex_adminstate_fromstring
    (json_object_get_string (obj, "adminState"));
  result->created = json_object_get_uint (obj, "created");
  result->description = get_string (obj, "description");
  result->id = get_string (obj, "id");
  result->labels = array_to_strings (json_object_get_array (obj, "labels"));
  result->lastConnected = json_object_get_uint (obj, "lastConnected");
  result->lastReported = json_object_get_uint (obj, "lastReported");
  result->modified = json_object_get_uint (obj, "modified");
  result->operatingState = edgex_operatingstate_fromstring
    (json_object_get_string (obj, "operatingState"));
  result->origin = json_object_get_uint (obj, "origin");
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
  result->service = deviceservice_read
    (json_object_get_object (obj, "service"));
  result->next = NULL;

  return result;
}

static JSON_Value *device_write (const edgex_device *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);

  json_object_set_value (obj, "profile", deviceprofile_write_name (e->profile));
  json_object_set_value (obj, "service", deviceservice_write_name (e->service));
  json_object_set_uint (obj, "lastConnected", 0);
  json_object_set_uint (obj, "lastReported", 0);
  if (e->id)
  {
    json_object_set_string (obj, "id", e->id);
  }
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
  result->id = strdup (e->id);
  result->description = strdup (e->description);
  result->labels = devsdk_strings_dup (e->labels);
  result->protocols = devsdk_protocols_dup (e->protocols);
  result->autos = autoevents_dup (e->autos);
  result->adminState = e->adminState;
  result->operatingState = e->operatingState;
  result->origin = e->origin;
  result->created = e->created;
  result->modified = e->modified;
  result->lastConnected = e->lastConnected;
  result->lastReported = e->lastReported;
  result->service = edgex_deviceservice_dup (e->service);
  result->profile = edgex_deviceprofile_dup (e->profile);
  result->next = NULL;
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
    devsdk_commandrequest *req = malloc (sizeof (devsdk_commandrequest));
    req->resname = strdup (r->name);
    req->attributes = devsdk_nvpairs_dup ((const devsdk_nvpairs *)r->attributes);
    req->type = edgex_propertytype_totypecode (r->properties->value->type);
    entry->request = req;
    entry->readable = r->properties->value->readable;
    entry->writable = r->properties->value->writable;
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
    free (e->id);
    devsdk_strings_free (e->labels);
    free (e->name);
    if (e->profile)
    {
      edgex_deviceprofile_free (e->profile);
    }
    edgex_deviceservice_free (e->service);
    e = e->next;
    free (current);
  }
}

edgex_device *edgex_device_read (iot_logger_t *lc, const char *json)
{
  edgex_device *result = NULL;
  JSON_Value *val = json_parse_string (json);
  JSON_Object *obj;

  obj = json_value_get_object (val);

  if (obj)
  {
    result = device_read (lc, obj);
  }

  json_value_free (val);

  return result;
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
  const char * id,
  const char * description,
  const devsdk_strings * labels,
  const char * profile_name
)
{
  char *json;
  JSON_Value *jval = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (jval);

  if (name) { json_object_set_string (obj, "name", name); }
  if (id) { json_object_set_string (obj, "id", id); }
  if (description) { json_object_set_string (obj, "description", description); }
  if (labels)
  {
    json_object_set_value (obj, "labels", strings_to_array (labels));
  }
  if (profile_name)
  {
    JSON_Value *pval = json_value_init_object ();
    JSON_Object *pobj = json_value_get_object (pval);
    json_object_set_string (pobj, "name", profile_name);
    json_object_set_value (obj, "profile", pval);
  }
  json = json_serialize_to_string (jval);
  json_value_free (jval);

  return json;
}

edgex_device *edgex_devices_read (iot_logger_t *lc, const char *json)
{
  edgex_device *result = NULL;
  JSON_Value *val = json_parse_string (json);
  JSON_Array *array = json_value_get_array (val);
  edgex_device **last_ptr = &result;

  if (array)
  {
    size_t count = json_array_get_count (array);
    for (size_t i = 0; i < count; i++)
    {
      edgex_device *temp = device_read (lc, json_array_get_object (array, i));
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

edgex_addressable *edgex_addressable_read (const char *json)
{
  edgex_addressable *result = NULL;
  JSON_Value *val = json_parse_string (json);
  JSON_Object *obj;

  obj = json_value_get_object (val);

  if (obj)
  {
    result = addressable_read (obj);
  }

  json_value_free (val);

  return result;
}

char *edgex_addressable_write (const edgex_addressable *e, bool create)
{
  char *result;
  JSON_Value *val;

  val = addressable_write (e, create);
  result = json_serialize_to_string (val);
  json_value_free (val);
  return result;
}

static edgex_valuedescriptor *valuedescriptor_read (const JSON_Object *obj)
{
  edgex_valuedescriptor *result = malloc (sizeof (edgex_valuedescriptor));
  result->created = json_object_get_uint (obj, "created");
  result->defaultValue = get_string (obj, "defaultValue");
  result->description = get_string (obj, "description");
  result->formatting = get_string (obj, "formatting");
  result->id = get_string (obj, "id");
  result->labels = array_to_strings (json_object_get_array (obj, "labels"));
  result->max = get_string (obj, "max");
  result->min = get_string (obj, "min");
  result->modified = json_object_get_uint (obj, "modified");
  result->name = get_string (obj, "name");
  result->origin = json_object_get_uint (obj, "origin");
  result->type = get_string (obj, "type");
  result->uomLabel = get_string (obj, "uomLabel");
  result->mediaType = get_string (obj, "mediaType");
  result->floatEncoding = get_string (obj, "floatEncoding");

  return result;
}

static JSON_Value *valuedescriptor_write (const edgex_valuedescriptor *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);
  json_object_set_uint (obj, "created", e->created);
  json_object_set_string (obj, "defaultValue", e->defaultValue);
  json_object_set_string (obj, "description", e->description);
  json_object_set_string (obj, "formatting", e->formatting);
  json_object_set_string (obj, "id", e->id);
  json_object_set_value (obj, "labels", strings_to_array (e->labels));
  json_object_set_string (obj, "max", e->max);
  json_object_set_string (obj, "min", e->min);
  json_object_set_uint (obj, "modified", e->modified);
  json_object_set_string (obj, "name", e->name);
  json_object_set_uint (obj, "origin", e->origin);
  json_object_set_string (obj, "type", e->type);
  json_object_set_string (obj, "uomLabel", e->uomLabel);
  json_object_set_string (obj, "floatEncoding", e->floatEncoding);
  json_object_set_string (obj, "mediaType", e->mediaType);

  return result;
}

void edgex_valuedescriptor_free (edgex_valuedescriptor *e)
{
  free (e->defaultValue);
  free (e->description);
  free (e->formatting);
  free (e->id);
  devsdk_strings_free (e->labels);
  free (e->max);
  free (e->min);
  free (e->name);
  free (e->type);
  free (e->uomLabel);
  free (e->floatEncoding);
  free (e->mediaType);
  free (e);
}

edgex_valuedescriptor *edgex_valuedescriptor_read (const char *json)
{
  edgex_valuedescriptor *result = NULL;
  JSON_Value *val = json_parse_string (json);
  JSON_Object *obj;

  obj = json_value_get_object (val);

  if (obj)
  {
    result = valuedescriptor_read (obj);
  }

  json_value_free (val);

  return result;
}

char *edgex_valuedescriptor_write (const edgex_valuedescriptor *e)
{
  char *result;
  JSON_Value *val;

  val = valuedescriptor_write (e);
  result = json_serialize_to_string (val);
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
  result->id = get_string (obj, "id");
  JSON_Object *idobj = json_object_get_object (obj, "identifiers");
  if (idobj)
  {
    result->identifiers = nvpairs_read (idobj);
  }
  JSON_Object *blockObj = json_object_get_object (obj, "blockingidentifiers");
  if (blockObj)
  {
    result->blocking_identifiers = blocklist_read (blockObj);
  }
  JSON_Object *profObj = json_object_get_object (obj, "profile");
  result->profile = get_string (profObj, "name");
  result->adminstate = edgex_adminstate_fromstring
    (json_object_get_string (obj, "adminState"));

  return result;
}

edgex_watcher *edgex_watcher_read (const char *json)
{
  edgex_watcher *result = NULL;
  JSON_Value *val = json_parse_string (json);
  JSON_Object *obj;

  obj = json_value_get_object (val);

  if (obj)
  {
    result = watcher_read (obj);
  }

  json_value_free (val);

  return result;
}

edgex_watcher *edgex_watchers_read (const char *json)
{
  edgex_watcher *result = NULL;
  JSON_Value *val = json_parse_string (json);
  JSON_Array *array = json_value_get_array (val);
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
  res->id = strdup (e->id);
  res->name = strdup (e->name);
  res->identifiers = devsdk_nvpairs_dup (e->identifiers);
  res->blocking_identifiers = edgex_blocklist_dup (e->blocking_identifiers);
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
    free (ew->id);
    free (ew->name);
    devsdk_nvpairs_free (ew->identifiers);
    edgex_watcher_regexes_free (ew->regs);
    edgex_blocklist_free (ew->blocking_identifiers);
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

#ifdef EDGEX_DEBUG_DUMP
void edgex_addressable_dump (edgex_addressable * e)
{
  if (e)
  {
    printf ("address %s\n", SAFE_STR(e->address));
    printf ("created %lu\n", e->created);
    printf ("id %s\n", SAFE_STR(e->id));
    printf ("method %s\n", SAFE_STR(e->method));
    printf ("modified %lu\n", e->modified);
    printf ("name %s\n", SAFE_STR(e->name));
    printf ("origin %lu\n", e->origin);
    printf ("password %s\n", SAFE_STR(e->password));
    printf ("path %s\n", SAFE_STR(e->path));
    printf ("port %lu\n", e->port);
    printf ("protocol %s\n", SAFE_STR(e->protocol));
    printf ("publisher %s\n", SAFE_STR(e->publisher));
    printf ("topic %s\n", SAFE_STR(e->topic));
    printf ("user %s\n", SAFE_STR(e->user));
  }
}

static void resourceoperation_dump (const edgex_resourceoperation *e)
{
  while (e)
  {
    printf ("index %s\n", SAFE_STR(e->index));
    printf ("operation %s\n", SAFE_STR(e->operation));
    printf ("object %s\n", SAFE_STR(e->object));
    printf ("property %s\n", SAFE_STR(e->property));
    printf ("parameter %s\n", SAFE_STR(e->parameter));
    printf ("resource %s\n", SAFE_STR(e->resource));
    printf ("secondary");
    for (devsdk_strings * tmp = e->secondary; tmp; tmp = tmp->next)
    {
      printf (" %s", tmp->str);
    }
    printf ("\n");
    printf ("mappings");
    for (devsdk_nvpairs * nv = e->mappings; nv; nv = nv->next)
    {
      printf (" [%s, %s]", nv->name, nv->value);
    }
    printf ("\n");
    e = e->next;
  }
}

void edgex_deviceprofile_dump (edgex_deviceprofile * e)
{
  printf ("created %lu\n", e->created);
  printf ("description %s\n", SAFE_STR(e->description));
  printf ("id %s\n", SAFE_STR(e->id));
  printf ("labels:");
  for (devsdk_strings * tmp = e->labels; tmp; tmp = tmp->next)
  {
    printf (" %s", tmp->str);
  }
  printf ("\n");
  printf ("manufacturer %s\n", SAFE_STR(e->manufacturer));
  printf ("model %s\n", SAFE_STR(e->model));
  printf ("modified %lu\n", e->modified);
  printf ("name %s\n", SAFE_STR(e->name));
  printf ("origin %lu\n", e->origin);
  printf ("device resources:\n");
  for (edgex_deviceresource * dr = e->device_resources; dr; dr = dr->next)
  {
    printf ("name %s\n", dr->name);
    printf ("description %s\n", dr->description);
    printf ("tag %s\n", dr->tag);
    printf ("properties\n");
    printf ("type %s\n", dr->properties->value->type);
    printf ("readwrite %s\n", rwstrings[dr->properties->value->readable][dr->properties->value->writable]);
    printf ("minimum %s\n", dr->properties->value->minimum);
    printf ("maximum %s\n", dr->properties->value->maximum);
    printf ("defaultvalue %s\n", dr->properties->value->defaultvalue);
    printf ("size %s\n", dr->properties->value->size);
    printf ("word %s\n", dr->properties->value->word);
    printf ("lsb %s\n", dr->properties->value->lsb);
    printf ("mask %s\n", dr->properties->value->mask);
    printf ("shift %s\n", dr->properties->value->shift);
    printf ("scale %s\n", dr->properties->value->scale);
    printf ("offset %s\n", dr->properties->value->offset);
    printf ("base %s\n", dr->properties->value->base);
    printf ("assertion %s\n", dr->properties->value->assertion);
    printf ("issigned %s\n", dr->properties->value->issigned ? "true" : "false");
    printf ("precision %s\n", dr->properties->value->precision);
    printf ("type %s\n", dr->properties->units->type);
    printf ("readwrite %s\n", dr->properties->units->readwrite);
    printf ("defaultvalue %s\n", dr->properties->units->defaultvalue);
  }
  printf ("device commands:\n");
  for (edgex_devicecommand * cmd = e->device_commands; cmd; cmd = cmd->next)
  {
    printf ("name %s\n", cmd->name);
    printf ("get:\n");
    resourceoperation_dump (cmd->get);
    printf ("put:\n");
    resourceoperation_dump (cmd->put);
  }
}

void edgex_deviceservice_dump (edgex_deviceservice * e)
{
  edgex_addressable_dump (e->addressable);
  printf ("adminState %s\n", e->adminState);
  printf ("created %lu\n", e->created);
  printf ("description %s\n", SAFE_STR(e->description));
  printf ("id %s\n", SAFE_STR(e->id));
  printf ("labels:");
  for (devsdk_strings * tmp = e->labels; tmp; tmp = tmp->next)
  {
    printf (" %s", tmp->str);
  }
  printf ("\n");
  printf ("lastConnected %lu\n", e->lastConnected);
  printf ("lastReported %lu\n", e->lastReported);
  printf ("modified %lu\n", e->modified);
  printf ("name %s\n", SAFE_STR(e->name));
  printf ("operatingState %s\n", SAFE_STR(e->operatingState));
  printf ("origin %lu\n", e->origin);
}

void edgex_device_dump (edgex_device * e)
{
  edgex_addressable_dump (e->addressable);
  printf ("adminState %s\n", e->adminState);
  printf ("created %lu\n", e->created);
  printf ("description %s\n", e->description);
  printf ("id %s\n", e->id);
  printf ("labels:");
  for (devsdk_strings * tmp = e->labels; tmp; tmp = tmp->next)
  {
    printf (" %s", tmp->str);
  }
  printf ("\n");
  printf ("lastConnected %lu\n", e->lastConnected);
  printf ("lastReported %lu\n", e->lastReported);
  printf ("modified %lu\n", e->modified);
  printf ("name %s\n", e->name);
  printf ("operatingState %s\n", e->operatingState);
  printf ("origin %lu\n", e->origin);
  edgex_deviceprofile_dump (e->profile);
  edgex_deviceservice_dump (e->service);
}

static void reading_dump (edgex_reading * e)
{
  printf ("created %lu\n", e->created);
  printf ("id %s\n", e->id);
  printf ("modified %lu\n", e->modified);
  printf ("name %s\n", e->name);
  printf ("origin %lu\n", e->origin);
  printf ("pushed %lu\n", e->pushed);
  printf ("value %s\n", e->value);
}

void edgex_valuedescriptor_dump (edgex_valuedescriptor * e)
{
  printf ("created %lu\n", e->created);
  printf ("defaultValue %s\n", e->defaultValue);
  printf ("description %s\n", e->description);
  printf ("formatting %s\n", e->formatting);
  printf ("id %s\n", e->id);
  printf ("labels:");
  for (devsdk_strings * tmp = e->labels; tmp; tmp = tmp->next)
  {
    printf (" %s", tmp->str);
  }

  printf ("\n");
  printf ("max %s\n", e->max);
  printf ("min %s\n", e->min);
  printf ("modified %lu\n", e->modified);
  printf ("name %s\n", e->name);
  printf ("origin %lu\n", e->origin);
  printf ("type %s\n", e->type);
  printf ("uomLabel %s\n", e->uomLabel);
}
#endif
