/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "edgex_rest.h"
#include "parson.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define SAFE_STR(s) (s ? s : "NULL")
#define SAFE_STRDUP(s) (s ? strdup(s) : NULL)

static char *get_string (const JSON_Object *obj, const char *name)
{
  const char *str = json_object_get_string (obj, name);
  return strdup (str ? str : "");
}

static char *get_array_string (const JSON_Array *array, size_t index)
{
  const char *str = json_array_get_string (array, index);
  return strdup (str ? str : "");
}

static edgex_strings *array_to_strings (const JSON_Array *array)
{
  size_t count = json_array_get_count (array);
  size_t i;
  edgex_strings *result = NULL;
  edgex_strings *temp;
  edgex_strings **last_ptr = &result;

  for (i = 0; i < count; i++)
  {
    temp = (edgex_strings *) malloc (sizeof (edgex_strings));
    temp->str = get_array_string (array, i);
    temp->next = NULL;
    *last_ptr = temp;
    last_ptr = &(temp->next);
  }

  return result;
}

static JSON_Value *strings_to_array (const edgex_strings *s)
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

edgex_strings *edgex_strings_dup (const edgex_strings *strs)
{
  edgex_strings *result = NULL;
  edgex_strings *copy;
  edgex_strings **last = &result;

  while (strs)
  {
    copy = malloc (sizeof (edgex_strings));
    copy->str = strdup (strs->str);
    copy->next = NULL;
    *last = copy;
    last = &(copy->next);
    strs = strs->next;
  }

  return result;
}

void edgex_strings_free (edgex_strings *strs)
{
  while (strs)
  {
    edgex_strings *current = strs;
    free (strs->str);
    strs = strs->next;
    free (current);
  }
}

edgex_nvpairs *edgex_nvpairs_dup (edgex_nvpairs *p)
{
  edgex_nvpairs *result = NULL;
  edgex_nvpairs *copy;
  edgex_nvpairs **last = &result;
  while (p)
  {
    copy = malloc (sizeof (edgex_nvpairs));
    copy->name = strdup (p->name);
    copy->value = strdup (p->value);
    copy->next = NULL;
    *last = copy;
    last = &(copy->next);
    p = p->next;
  }
  return result;
}

void edgex_nvpairs_free (edgex_nvpairs *p)
{
  while (p)
  {
    edgex_nvpairs *current = p;
    free (p->name);
    free (p->value);
    p = p->next;
    free (current);
  }
}

static const char *proptypes[] =
{
  "Bool", "String", "Binary", "Uint8", "Uint16", "Uint32", "Uint64",
  "Int8", "Int16", "Int32", "Int64", "Float32", "Float64"
};

const char *edgex_propertytype_tostring (edgex_propertytype pt)
{
  return proptypes[pt];
}

bool edgex_propertytype_fromstring (edgex_propertytype *res, const char *str)
{
  for (edgex_propertytype i = Bool; i <= Float64; i++)
  {
    if (strcmp (str, proptypes[i]) == 0)
    {
      *res = i;
      return true;
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
  iot_logging_client *lc,
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
// TODO: the workaround below can be removed when edgex-go #855 is resolved
if (strcmp (name, "scale") == 0 && strcmp (str, "1.0") == 0) return true;
if (strcmp (name, "offset") == 0 && strcmp (str, "0.0") == 0) return true;
if (strcmp (name, "base") == 0 && strcmp (str, "0") == 0) return true;

    switch (type)
    {
      case Int8:
      case Int16:
      case Int32:
      case Int64:
      case Uint8:
      case Uint16:
      case Uint32:
      case Uint64:
        errno = 0;
        int64_t i = strtol (str, &end, 0);
        if (errno || (end && *end))
        {
          iot_log_error
          (
            lc,
            "Unable to parse \"%s\" as integer for transform \"%s\"",
            str, name
          );
          ok = false;
        }
        else
        {
          res->enabled = true;
          res->value.ival = i;
        }
        break;
      case Float32:
      case Float64:
        errno = 0;
        double d = strtod (str, &end);
        if (errno || (end && *end))
        {
          iot_log_error
          (
            lc,
            "Unable to parse \"%s\" as float for transform \"%s\"",
            str, name
          );
          ok = false;
        }
        else
        {
          res->enabled = true;
          res->value.dval = d;
        }
        break;
      default:
        iot_log_error
          (lc, "Transform \"%s\" specified for non-numeric data", name);
        ok = false;
    }
  }
  return ok;
}

static edgex_propertyvalue *propertyvalue_read
  (iot_logging_client *lc, const JSON_Object *obj)
{
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
    if (!ok)
    {
      free (result);
      return NULL;
    }
    result->type = pt;
    result->readwrite = get_string (obj, "readWrite");
    result->minimum = get_string (obj, "minimum");
    result->maximum = get_string (obj, "maximum");
    result->defaultvalue = get_string (obj, "defaultValue");
    result->lsb = get_string (obj, "lsb");
    result->mask = get_string (obj, "mask");
    result->shift = get_string (obj, "shift");
    result->assertion = get_string (obj, "assertion");
    result->precision = get_string (obj, "precision");
  }
  else
  {
    iot_log_error (lc, "Unable to parse \"%s\" as data type", tstr);
  }
  return result;
}

static void set_arg
(
  JSON_Object *obj,
  const char *name,
  edgex_transformArg arg,
  edgex_propertytype pt
)
{
  if (arg.enabled)
  {
    char tmp[32];
    if (pt == Float32 || pt == Float64)
    {
      sprintf (tmp, "%f", arg.value.dval);
    }
    else
    {
      sprintf (tmp, "%ld", arg.value.ival);
    }
    json_object_set_string (obj, name, tmp);
  }
}

static JSON_Value *propertyvalue_write (const edgex_propertyvalue *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);
  json_object_set_string (obj, "type", edgex_propertytype_tostring (e->type));
  json_object_set_string (obj, "readWrite", e->readwrite);
  json_object_set_string (obj, "minimum", e->minimum);
  json_object_set_string (obj, "maximum", e->maximum);
  json_object_set_string (obj, "defaultValue", e->defaultvalue);
  json_object_set_string (obj, "lsb", e->lsb);
  json_object_set_string (obj, "mask", e->mask);
  json_object_set_string (obj, "shift", e->shift);
  set_arg (obj, "scale", e->scale, e->type);
  set_arg (obj, "offset", e->offset, e->type);
  set_arg (obj, "base", e->base, e->type);
  json_object_set_string (obj, "assertion", e->assertion);
  json_object_set_string (obj, "precision", e->precision);
  return result;
}

static edgex_propertyvalue *propertyvalue_dup (edgex_propertyvalue *pv)
{
  edgex_propertyvalue *result = NULL;
  if (pv)
  {
    result = malloc (sizeof (edgex_propertyvalue));
    result->type = pv->type;
    result->readwrite = strdup (pv->readwrite);
    result->minimum = strdup (pv->minimum);
    result->maximum = strdup (pv->maximum);
    result->defaultvalue = strdup (pv->defaultvalue);
    result->lsb = strdup (pv->lsb);
    result->mask = strdup (pv->mask);
    result->shift = strdup (pv->shift);
    result->scale = pv->scale;
    result->offset = pv->offset;
    result->base = pv->base;
    result->assertion = strdup (pv->assertion);
    result->precision = strdup (pv->precision);
  }
  return result;
}

static void propertyvalue_free (edgex_propertyvalue *e)
{
  free (e->readwrite);
  free (e->minimum);
  free (e->maximum);
  free (e->defaultvalue);
  free (e->lsb);
  free (e->mask);
  free (e->shift);
  free (e->assertion);
  free (e->precision);
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
  (iot_logging_client *lc, const JSON_Object *obj)
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

static edgex_profileproperty *profileproperty_dup (edgex_profileproperty *pp)
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

static edgex_deviceobject *deviceobject_read
  (iot_logging_client *lc, const JSON_Object *obj)
{
  edgex_deviceobject *result = NULL;
  edgex_profileproperty *pp = profileproperty_read
    (lc, json_object_get_object (obj, "properties"));
  char *name = get_string (obj, "name");

  if (pp)
  {
    JSON_Object *attributes_obj;
    size_t count;
    edgex_nvpairs *nv;
    edgex_nvpairs **nv_last;

    result = malloc (sizeof (edgex_deviceobject));
    result->name = name;
    result->description = get_string (obj, "description");
    result->tag = get_string (obj, "tag");
    result->properties = pp;
    result->attributes = NULL;
    attributes_obj = json_object_get_object (obj, "attributes");
    count = json_object_get_count (attributes_obj);
    nv_last = &result->attributes;
    for (size_t i = 0; i < count; i++)
    {
      nv = malloc (sizeof (edgex_nvpairs));
      nv->name = strdup (json_object_get_name (attributes_obj, i));
      nv->value = strdup
        (json_value_get_string (json_object_get_value_at (attributes_obj, i)));
      nv->next = NULL;
      *nv_last = nv;
      nv_last = &nv->next;
    }
    result->next = NULL;
  }
  else
  {
    iot_log_error (lc, "Error reading property for deviceResource %s", name);
  }
  return result;
}

static JSON_Value *deviceobject_write (const edgex_deviceobject *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);
  json_object_set_string (obj, "name", e->name);
  json_object_set_string (obj, "description", e->description);
  json_object_set_string (obj, "tag", e->tag);
  json_object_set_value
    (obj, "properties", profileproperty_write (e->properties));
  return result;
}

static edgex_deviceobject *edgex_deviceobject_dup (edgex_deviceobject *edo)
{
  edgex_deviceobject *result = NULL;
  if (edo)
  {
    result = malloc (sizeof (edgex_deviceobject));
    result->name = strdup (edo->name);
    result->description = strdup (edo->description);
    result->tag = strdup (edo->tag);
    result->properties = profileproperty_dup (edo->properties);
    result->attributes = edgex_nvpairs_dup (edo->attributes);
    result->next = edgex_deviceobject_dup (edo->next);
  }
  return result;
}

static void deviceobject_free (edgex_deviceobject *e)
{
  while (e)
  {
    edgex_deviceobject *current = e;
    free (e->name);
    free (e->description);
    free (e->tag);
    profileproperty_free (e->properties);
    edgex_nvpairs_free (e->attributes);
    e = e->next;
    free (current);
  }
}

static edgex_response *response_read (const JSON_Object *obj)
{
  edgex_response *result = malloc (sizeof (edgex_response));
  result->code = get_string (obj, "code");
  result->description = get_string (obj, "description");
  result->expectedvalues = array_to_strings
    (json_object_get_array (obj, "expectedValues"));
  result->next = NULL;
  return result;
}

static JSON_Value *response_write (const edgex_response *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);
  json_object_set_string (obj, "code", e->code);
  json_object_set_string (obj, "description", e->description);
  json_object_set_value
    (obj, "expectedValues", strings_to_array (e->expectedvalues));
  return result;
}

static edgex_response *response_dup (edgex_response *r)
{
  edgex_response *result = NULL;
  if (r)
  {
    result = malloc (sizeof (edgex_response));
    result->code = strdup (r->code);
    result->description = strdup (r->description);
    result->expectedvalues = edgex_strings_dup (r->expectedvalues);
    result->next = response_dup (r->next);
  }
  return result;
}

static void response_free (edgex_response *e)
{
  while (e)
  {
    edgex_response *current = e;
    free (e->code);
    free (e->description);
    edgex_strings_free (e->expectedvalues);
    e = e->next;
    free (current);
  }
}

static edgex_get *get_read (const JSON_Object *obj)
{
  edgex_get *result = malloc (sizeof (edgex_get));
  size_t i, count;
  JSON_Array *array;
  edgex_response **last_ptr = &result->responses;
  edgex_response *temp;
  result->path = get_string (obj, "path");
  array = json_object_get_array (obj, "responses");
  result->responses = NULL;
  count = json_array_get_count (array);
  for (i = 0; i < count; i++)
  {
    temp = response_read (json_array_get_object (array, i));
    *last_ptr = temp;
    last_ptr = &(temp->next);
  }
  return result;
}

static JSON_Value *get_write (const edgex_get *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);
  JSON_Value *array_val = json_value_init_array ();
  JSON_Array *array = json_value_get_array (array_val);
  edgex_response *temp;
  json_object_set_string (obj, "path", e->path);
  for (temp = e->responses; temp; temp = temp->next)
  {
    json_array_append_value (array, response_write (temp));
  }
  json_object_set_value (obj, "responses", array_val);
  return result;
}

static edgex_get *get_dup (edgex_get *p)
{
  edgex_get *result = NULL;
  if (p)
  {
    result = malloc (sizeof (edgex_get));
    result->path = strdup (p->path);
    result->responses = response_dup (p->responses);
  }
  return result;
}

static void get_free (edgex_get *e)
{
  free (e->path);
  response_free (e->responses);
  free (e);
}

static edgex_put *put_read (const JSON_Object *obj)
{
  edgex_put *result = malloc (sizeof (edgex_put));
  size_t i, count;
  JSON_Array *array;
  edgex_response **last_ptr = &result->responses;
  edgex_response *temp;
  result->path = get_string (obj, "path");
  array = json_object_get_array (obj, "responses");
  result->responses = NULL;
  count = json_array_get_count (array);
  for (i = 0; i < count; i++)
  {
    temp = response_read (json_array_get_object (array, i));
    *last_ptr = temp;
    last_ptr = &(temp->next);
  }
  result->parameter_names = array_to_strings
    (json_object_get_array (obj, "parameterNames"));
  return result;
}

static JSON_Value *put_write (const edgex_put *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);
  JSON_Value *array_val = json_value_init_array ();
  JSON_Array *array = json_value_get_array (array_val);
  edgex_response *temp;
  json_object_set_string (obj, "path", e->path);
  for (temp = e->responses; temp; temp = temp->next)
  {
    json_array_append_value (array, response_write (temp));
  }
  json_object_set_value (obj, "responses", array_val);
  json_object_set_value
    (obj, "expectedValues", strings_to_array (e->parameter_names));
  return result;
}

static edgex_put *put_dup (edgex_put *p)
{
  edgex_put *result = NULL;
  if (p)
  {
    result = malloc (sizeof (edgex_put));
    result->path = strdup (p->path);
    result->responses = response_dup (p->responses);
    result->parameter_names = edgex_strings_dup (p->parameter_names);
  }
  return result;
}

static void put_free (edgex_put *e)
{
  free (e->path);
  response_free (e->responses);
  edgex_strings_free (e->parameter_names);
  free (e);
}

static edgex_command *command_read (const JSON_Object *obj)
{
  edgex_command *result = malloc (sizeof (edgex_command));

  result->id = get_string (obj, "id");
  result->name = get_string (obj, "name");
  result->created = (uint64_t) json_object_get_number (obj, "created");
  result->modified = (uint64_t) json_object_get_number (obj, "modified");
  result->origin = (uint64_t) json_object_get_number (obj, "origin");
  result->get = get_read (json_object_get_object (obj, "get"));
  result->put = put_read (json_object_get_object (obj, "put"));
  result->next = NULL;
  return result;
}

static JSON_Value *command_write (const edgex_command *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);
  json_object_set_string (obj, "id", e->id);
  json_object_set_string (obj, "name", e->name);
  json_object_set_number (obj, "created", e->created);
  json_object_set_number (obj, "modified", e->modified);
  json_object_set_number (obj, "origin", e->origin);
  json_object_set_value (obj, "get", get_write (e->get));
  json_object_set_value (obj, "put", put_write (e->put));
  return result;
}

static edgex_command *command_dup (edgex_command *c)
{
  edgex_command *result = NULL;
  if (c)
  {
    result = malloc (sizeof (edgex_command));
    result->id = strdup (c->id);
    result->name = strdup (c->name);
    result->created = c->created;
    result->modified = c->modified;
    result->origin = c->origin;
    result->put = put_dup (c->put);
    result->get = get_dup (c->get);
    result->next = command_dup (c->next);
  }
  return result;
}

static void command_free (edgex_command *e)
{
  while (e)
  {
    edgex_command *current = e;
    free (e->id);
    free (e->name);
    get_free (e->get);
    put_free (e->put);
    e = e->next;
    free (current);
  }
}

static edgex_resourceoperation *resourceoperation_read (const JSON_Object *obj)
{
  edgex_resourceoperation *result = malloc (sizeof (edgex_resourceoperation));
  size_t count;
  edgex_nvpairs *nv;
  edgex_nvpairs **nv_last = &result->mappings;
  JSON_Object *mappings_obj;

  result->index = get_string (obj, "index");
  result->operation = get_string (obj, "operation");
  result->object = get_string (obj, "object");
  result->property = get_string (obj, "property");
  result->parameter = get_string (obj, "parameter");
  result->resource = get_string (obj, "resource");
  result->secondary = array_to_strings
    (json_object_get_array (obj, "secondary"));
  result->mappings = NULL;
  mappings_obj = json_object_get_object (obj, "mappings");
  count = json_object_get_count (mappings_obj);
  for (size_t i = 0; i < count; i++)
  {
    nv = malloc (sizeof (edgex_nvpairs));
    nv->name = strdup (json_object_get_name (mappings_obj, i));
    nv->value = strdup
      (json_value_get_string (json_object_get_value_at (mappings_obj, i)));
    nv->next = NULL;
    *nv_last = nv;
    nv_last = &nv->next;
  }
  result->next = NULL;
  return result;
}

static JSON_Value *resourceoperation_write (const edgex_resourceoperation *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);
  JSON_Value *mappings_val = json_value_init_object ();
  JSON_Object *mappings_obj = json_value_get_object (mappings_val);

  json_object_set_string (obj, "index", e->index);
  json_object_set_string (obj, "operation", e->operation);
  json_object_set_string (obj, "object", e->object);
  json_object_set_string (obj, "property", e->property);
  json_object_set_string (obj, "parameter", e->parameter);
  json_object_set_string (obj, "resource", e->resource);
  json_object_set_value (obj, "secondary", strings_to_array (e->secondary));
  for (edgex_nvpairs *nv = e->mappings; nv; nv = nv->next)
  {
    json_object_set_string (mappings_obj, nv->name, nv->value);
  }
  json_object_set_value (obj, "mappings", mappings_val);
  return result;
}

static edgex_resourceoperation *resourceoperation_dup
  (edgex_resourceoperation *ro)
{
  edgex_resourceoperation *result = NULL;
  if (ro)
  {
    result = malloc (sizeof (edgex_resourceoperation));
    result->index = strdup (ro->index);
    result->operation = strdup (ro->operation);
    result->object = strdup (ro->object);
    result->property = strdup (ro->property);
    result->parameter = strdup (ro->parameter);
    result->resource = strdup (ro->resource);
    result->secondary = edgex_strings_dup (ro->secondary);
    result->mappings = edgex_nvpairs_dup (ro->mappings);
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
    free (e->property);
    free (e->parameter);
    free (e->resource);
    edgex_strings_free (e->secondary);
    edgex_nvpairs_free (e->mappings);
    e = e->next;
    free (current);
  }
}

static edgex_profileresource *profileresource_read (const JSON_Object *obj)
{
  edgex_profileresource *result = malloc (sizeof (edgex_profileresource));
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

static JSON_Value *profileresource_write (const edgex_profileresource *e)
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

static edgex_profileresource *profileresource_dup (edgex_profileresource *pr)
{
  edgex_profileresource *result = NULL;
  if (pr)
  {
    result = malloc (sizeof (edgex_profileresource));
    result->name = strdup (pr->name);
    result->set = resourceoperation_dup (pr->set);
    result->get = resourceoperation_dup (pr->get);
    result->next = profileresource_dup (pr->next);
  }
  return result;
}

static void profileresource_free (edgex_profileresource *e)
{
  while (e)
  {
    edgex_profileresource *current = e;
    free (e->name);
    resourceoperation_free (e->set);
    resourceoperation_free (e->get);
    e = e->next;
    free (current);
  }
}

static edgex_deviceprofile *deviceprofile_read
  (iot_logging_client *lc, const JSON_Object *obj)
{
  edgex_deviceprofile *result = malloc (sizeof (edgex_deviceprofile));
  size_t count;
  JSON_Array *array;
  edgex_deviceobject **last_ptr = &result->device_resources;
  edgex_command **last_ptr2 = &result->commands;
  edgex_profileresource **last_ptr3 = &result->resources;

  result->id = get_string (obj, "id");
  result->name = get_string (obj, "name");
  result->description = get_string (obj, "description");
  result->created = (uint64_t) json_object_get_number (obj, "created");
  result->modified = (uint64_t) json_object_get_number (obj, "modified");
  result->origin = (uint64_t) json_object_get_number (obj, "origin");
  result->manufacturer = get_string (obj, "manufacturer");
  result->model = get_string (obj, "model");
  result->labels = array_to_strings (json_object_get_array (obj, "labels"));
  array = json_object_get_array (obj, "deviceResources");
  result->device_resources = NULL;
  result->commands = NULL;
  result->resources = NULL;
  count = json_array_get_count (array);
  for (size_t i = 0; i < count; i++)
  {
    edgex_deviceobject *temp = deviceobject_read
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
  array = json_object_get_array (obj, "commands");
  count = json_array_get_count (array);
  for (size_t i = 0; i < count; i++)
  {
    edgex_command *temp = command_read (json_array_get_object (array, i));
    *last_ptr2 = temp;
    last_ptr2 = &(temp->next);
  }
  array = json_object_get_array (obj, "resources");
  count = json_array_get_count (array);
  for (size_t i = 0; i < count; i++)
  {
    edgex_profileresource *temp = profileresource_read
      (json_array_get_object (array, i));
    *last_ptr3 = temp;
    last_ptr3 = &(temp->next);
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
  JSON_Value *array_val3 = json_value_init_array ();
  JSON_Array *array3 = json_value_get_array (array_val2);

  if (!create)
  {
    json_object_set_string (obj, "id", e->id);
    json_object_set_number (obj, "created", e->created);
    json_object_set_number (obj, "modified", e->modified);
  }
  json_object_set_string (obj, "name", e->name);
  json_object_set_string (obj, "description", e->description);
  json_object_set_number (obj, "origin", e->origin);
  json_object_set_string (obj, "manufacturer", e->manufacturer);
  json_object_set_string (obj, "model", e->model);
  json_object_set_value (obj, "labels", strings_to_array (e->labels));

  for (edgex_deviceobject *temp = e->device_resources; temp; temp = temp->next)
  {
    json_array_append_value (array, deviceobject_write (temp));
  }

  json_object_set_value (obj, "deviceResources", array_val);

  for (edgex_command *temp = e->commands; temp; temp = temp->next)
  {
    json_array_append_value (array2, command_write (temp));
  }

  json_object_set_value (obj, "commands", array_val2);

  for (edgex_profileresource *temp = e->resources; temp; temp = temp->next)
  {
    json_array_append_value (array3, profileresource_write (temp));
  }

  json_object_set_value (obj, "resources", array_val3);
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
  (iot_logging_client *lc, const char *json)
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

static edgex_addressable *addressable_read (const JSON_Object *obj)
{
  edgex_addressable *result = malloc (sizeof (edgex_addressable));
  result->address = get_string (obj, "address");
  result->created = json_object_get_number (obj, "created");
  result->id = get_string (obj, "id");
  result->method = get_string (obj, "method");
  result->modified = json_object_get_number (obj, "modified");
  result->name = get_string (obj, "name");
  result->origin = json_object_get_number (obj, "origin");
  result->password = get_string (obj, "password");
  result->path = get_string (obj, "path");
  result->port = json_object_get_number (obj, "port");
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
    json_object_set_number (obj, "created", e->created);
    json_object_set_number (obj, "modified", e->modified);
  }

  json_object_set_string (obj, "address", e->address);
  json_object_set_string (obj, "method", e->method);
  json_object_set_string (obj, "name", e->name);
  json_object_set_number (obj, "origin", e->origin);
  json_object_set_string (obj, "password", e->password);
  json_object_set_string (obj, "path", e->path);
  json_object_set_number (obj, "port", e->port);
  json_object_set_string (obj, "protocol", e->protocol);
  json_object_set_string (obj, "publisher", e->publisher);
  json_object_set_string (obj, "topic", e->topic);
  json_object_set_string (obj, "user", e->user);

  return result;
}

static JSON_Value *addressable_write_name (const edgex_addressable *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);

  json_object_set_string (obj, "name", e->name);

  return result;
}

edgex_addressable *edgex_addressable_dup (edgex_addressable *addr)
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
  result->created = json_object_get_number (obj, "created");
  result->description = get_string (obj, "description");
  result->id = get_string (obj, "id");
  result->labels = array_to_strings (json_object_get_array (obj, "labels"));
  result->lastConnected = json_object_get_number (obj, "lastConnected");
  result->lastReported = json_object_get_number (obj, "lastReported");
  result->modified = json_object_get_number (obj, "modified");
  result->name = get_string (obj, "name");
  result->operatingState = edgex_operatingstate_fromstring
    (json_object_get_string (obj, "operatingState"));
  result->origin = json_object_get_number (obj, "origin");

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
    json_object_set_number (obj, "created", e->created);
    json_object_set_number (obj, "modified", e->modified);
  }

  json_object_set_value
    (obj, "addressable", addressable_write (e->addressable, false));
  json_object_set_string
    (obj, "adminState", edgex_adminstate_tostring (e->adminState));
  json_object_set_string (obj, "description", e->description);
  json_object_set_value (obj, "labels", strings_to_array (e->labels));
  json_object_set_number (obj, "lastConnected", e->lastConnected);
  json_object_set_number (obj, "lastReported", e->lastReported);
  json_object_set_string (obj, "name", e->name);
  json_object_set_string
    (obj, "operatingState", edgex_operatingstate_tostring (e->operatingState));
  json_object_set_number (obj, "origin", e->origin);

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
  res->labels = edgex_strings_dup (e->labels);
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
    edgex_strings_free (e->labels);
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

edgex_deviceprofile *edgex_deviceprofile_dup (edgex_deviceprofile *dp)
{
  edgex_deviceprofile *result = NULL;
  if (dp)
  {
    result = malloc (sizeof (edgex_deviceprofile));
    result->id = strdup (dp->id);
    result->name = strdup (dp->name);
    result->description = strdup (dp->description);
    result->created = dp->created;
    result->modified = dp->modified;
    result->origin = dp->origin;
    result->manufacturer = strdup (dp->manufacturer);
    result->model = strdup (dp->model);
    result->labels = edgex_strings_dup (dp->labels);
    result->device_resources = edgex_deviceobject_dup (dp->device_resources);
    result->commands = command_dup (dp->commands);
    result->resources = profileresource_dup (dp->resources);
  }
  return result;
}

void edgex_deviceprofile_free (edgex_deviceprofile *e)
{
  free (e->id);
  free (e->name);
  free (e->description);
  free (e->manufacturer);
  free (e->model);
  edgex_strings_free (e->labels);
  deviceobject_free (e->device_resources);
  command_free (e->commands);
  profileresource_free (e->resources);
  free (e);
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
  (iot_logging_client *lc, const JSON_Object *obj)
{
  edgex_device *result = malloc (sizeof (edgex_device));
  result->addressable = addressable_read
    (json_object_get_object (obj, "addressable"));;
  result->adminState = edgex_adminstate_fromstring
    (json_object_get_string (obj, "adminState"));
  result->created = json_object_get_number (obj, "created");
  result->description = get_string (obj, "description");
  result->id = get_string (obj, "id");
  result->labels = array_to_strings (json_object_get_array (obj, "labels"));
  result->lastConnected = json_object_get_number (obj, "lastConnected");
  result->lastReported = json_object_get_number (obj, "lastReported");
  result->modified = json_object_get_number (obj, "modified");
  result->name = get_string (obj, "name");
  result->operatingState = edgex_operatingstate_fromstring
    (json_object_get_string (obj, "operatingState"));
  result->origin = json_object_get_number (obj, "origin");
  result->profile = deviceprofile_read
    (lc, json_object_get_object (obj, "profile"));
  result->service = deviceservice_read
    (json_object_get_object (obj, "service"));
  result->next = NULL;

  return result;
}

static JSON_Value *device_write (const edgex_device *e, bool create)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);

  if (create)
  {
    json_object_set_value
      (obj, "addressable", addressable_write_name (e->addressable));
    json_object_set_value
      (obj, "profile", deviceprofile_write_name (e->profile));
    json_object_set_value
      (obj, "service", deviceservice_write_name (e->service));
    json_object_set_number (obj, "lastConnected", 0);
    json_object_set_number (obj, "lastReported", 0);
  }
  else
  {
    json_object_set_value
      (obj, "addressable", addressable_write (e->addressable, false));
    json_object_set_string
      (obj, "adminState", edgex_adminstate_tostring (e->adminState));
    json_object_set_string
      (obj, "operatingState", edgex_operatingstate_tostring(e->operatingState));
    json_object_set_string (obj, "id", e->id);
    json_object_set_number (obj, "created", e->created);
    json_object_set_number (obj, "modified", e->modified);
    json_object_set_value
      (obj, "profile", deviceprofile_write (e->profile, false));
    json_object_set_value
      (obj, "service", deviceservice_write (e->service, false));
    json_object_set_number (obj, "lastConnected", e->lastConnected);
    json_object_set_number (obj, "lastReported", e->lastReported);
  }

  json_object_set_string
    (obj, "adminState", edgex_adminstate_tostring (e->adminState));
  json_object_set_string (obj, "name", e->name);
  json_object_set_string (obj, "description", e->description);
  json_object_set_value (obj, "labels", strings_to_array (e->labels));
  json_object_set_string
    (obj, "operatingState", edgex_operatingstate_tostring (e->operatingState));
  json_object_set_number (obj, "origin", e->origin);

  return result;
}

edgex_device *edgex_device_dup (const edgex_device *e)
{
  edgex_device *result = malloc (sizeof (edgex_device));
  result->name = strdup (e->name);
  result->id = strdup (e->id);
  result->description = strdup (e->description);
  result->labels = edgex_strings_dup (e->labels);
  result->addressable = edgex_addressable_dup (e->addressable);
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

void edgex_device_free (edgex_device *e)
{
  while (e)
  {
    edgex_device *current = e;
    edgex_addressable_free (e->addressable);
    free (e->description);
    free (e->id);
    edgex_strings_free (e->labels);
    free (e->name);
    edgex_deviceprofile_free (e->profile);
    edgex_deviceservice_free (e->service);
    e = e->next;
    free (current);
  }
}

edgex_device *edgex_device_read (iot_logging_client *lc, const char *json)
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

char *edgex_device_write (const edgex_device *e, bool create)
{
  char *result;
  JSON_Value *val;

  val = device_write (e, create);
  result = json_serialize_to_string (val);
  json_value_free (val);
  return result;
}

char *edgex_device_write_sparse
(
  const char * name,
  const char * id,
  const char * description,
  const edgex_strings * labels,
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

edgex_device *edgex_devices_read (iot_logging_client *lc, const char *json)
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
      *last_ptr = temp;
      last_ptr = &(temp->next);
    }
  }

  json_value_free (val);

  return result;
}

static edgex_scheduleevent *scheduleevent_read (const JSON_Object *obj)
{
  edgex_scheduleevent *result = malloc (sizeof (edgex_scheduleevent));
  result->name = get_string (obj, "name");
  result->id = get_string (obj, "id");
  result->origin = json_object_get_number (obj, "origin");
  result->created = json_object_get_number (obj, "created");
  result->modified = json_object_get_number (obj, "modified");
  result->schedule = get_string (obj, "schedule");
  result->addressable = addressable_read
    (json_object_get_object (obj, "addressable"));
  result->parameters = get_string (obj, "parameters");
  result->service = get_string (obj, "service");
  result->next = NULL;

  return result;
}

edgex_scheduleevent *edgex_scheduleevents_read (const char *json)
{
  edgex_scheduleevent *result = NULL;
  JSON_Value *val = json_parse_string (json);
  JSON_Array *array = json_value_get_array (val);
  edgex_scheduleevent **last_ptr = &result;

  if (array)
  {
    size_t count = json_array_get_count (array);
    for (size_t i = 0; i < count; i++)
    {
      edgex_scheduleevent *temp = scheduleevent_read
        (json_array_get_object (array, i));
      *last_ptr = temp;
      last_ptr = &(temp->next);
    }
  }

  json_value_free (val);

  return result;
}

static JSON_Value *scheduleevent_write
  (const edgex_scheduleevent *e, bool create)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);

  json_object_set_string (obj, "name", e->name);
  json_object_set_number (obj, "origin", e->origin);
  json_object_set_string (obj, "schedule", e->schedule);
  json_object_set_string (obj, "parameters", e->parameters);
  json_object_set_string (obj, "service", e->service);

  if (create)
  {
    json_object_set_value
      (obj, "addressable", addressable_write_name (e->addressable));
  }
  else
  {
    json_object_set_value
      (obj, "addressable", addressable_write (e->addressable, false));
    json_object_set_number (obj, "created", e->created);
    json_object_set_number (obj, "modified", e->modified);
    json_object_set_string (obj, "id", e->id);
  }

  return result;
}

char *edgex_scheduleevent_write (const edgex_scheduleevent *e, bool create)
{
  char *result;
  JSON_Value *val;

  val = scheduleevent_write (e, create);
  result = json_serialize_to_string (val);
  json_value_free (val);
  return result;
}

void edgex_scheduleevent_free (edgex_scheduleevent *e)
{
  free (e->name);
  free (e->id);
  free (e->schedule);
  free (e->parameters);
  free (e->service);
  edgex_addressable_free (e->addressable);
  free (e);
}


static edgex_schedule *schedule_read (const JSON_Object *obj)
{
  edgex_schedule *result = malloc (sizeof (edgex_schedule));
  result->name = get_string (obj, "name");
  result->id = get_string (obj, "id");
  result->origin = json_object_get_number (obj, "origin");
  result->created = json_object_get_number (obj, "created");
  result->modified = json_object_get_number (obj, "modified");
  result->start = get_string (obj, "start");
  result->end = get_string (obj, "end");
  result->frequency = get_string (obj, "frequency");
  result->cron = get_string (obj, "cron");
  result->runOnce = json_object_get_boolean (obj, "runOnce");

  return result;
}

edgex_schedule *edgex_schedule_read (const char *json)
{
  edgex_schedule *result = NULL;
  JSON_Value *val = json_parse_string (json);
  JSON_Object *obj;

  obj = json_value_get_object (val);

  if (obj)
  {
    result = schedule_read (obj);
  }

  json_value_free (val);

  return result;
}

static JSON_Value *schedule_write (const edgex_schedule *e, bool create)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);

  json_object_set_string (obj, "name", e->name);
  json_object_set_number (obj, "origin", e->origin);
  json_object_set_string (obj, "start", e->start);
  json_object_set_string (obj, "end", e->end);
  json_object_set_string (obj, "frequency", e->frequency);
  json_object_set_string (obj, "cron", e->cron);
  json_object_set_boolean (obj, "runOnce", e->runOnce);

  if (!create)
  {
    json_object_set_number (obj, "created", e->created);
    json_object_set_number (obj, "modified", e->modified);
    json_object_set_string (obj, "id", e->id);
  }

  return result;
}

char *edgex_schedule_write (const edgex_schedule *e, bool create)
{
  char *result;
  JSON_Value *val;

  val = schedule_write (e, create);
  result = json_serialize_to_string (val);
  json_value_free (val);
  return result;
}

void edgex_schedule_free (edgex_schedule *e)
{
  free (e->name);
  free (e->id);
  free (e->start);
  free (e->end);
  free (e->frequency);
  free (e->cron);
  free (e);
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
  result->created = json_object_get_number (obj, "created");
  result->defaultValue = get_string (obj, "defaultValue");
  result->description = get_string (obj, "description");
  result->formatting = get_string (obj, "formatting");
  result->id = get_string (obj, "id");
  result->labels = array_to_strings (json_object_get_array (obj, "labels"));
  result->max = get_string (obj, "max");
  result->min = get_string (obj, "min");
  result->modified = json_object_get_number (obj, "modified");
  result->name = get_string (obj, "name");
  result->origin = json_object_get_number (obj, "origin");
  result->type = get_string (obj, "type");
  result->uomLabel = get_string (obj, "uomLabel");

  return result;
}

static JSON_Value *valuedescriptor_write (const edgex_valuedescriptor *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);
  json_object_set_number (obj, "created", e->created);
  json_object_set_string (obj, "defaultValue", e->defaultValue);
  json_object_set_string (obj, "description", e->description);
  json_object_set_string (obj, "formatting", e->formatting);
  json_object_set_string (obj, "id", e->id);
  json_object_set_value (obj, "labels", strings_to_array (e->labels));
  json_object_set_string (obj, "max", e->max);
  json_object_set_string (obj, "min", e->min);
  json_object_set_number (obj, "modified", e->modified);
  json_object_set_string (obj, "name", e->name);
  json_object_set_number (obj, "origin", e->origin);
  json_object_set_string (obj, "type", e->type);
  json_object_set_string (obj, "uomLabel", e->uomLabel);

  return result;
}

void edgex_valuedescriptor_free (edgex_valuedescriptor *e)
{
  free (e->defaultValue);
  free (e->description);
  free (e->formatting);
  free (e->id);
  edgex_strings_free (e->labels);
  free (e->max);
  free (e->min);
  free (e->name);
  free (e->type);
  free (e->uomLabel);
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
    for (edgex_strings * tmp = e->secondary; tmp; tmp = tmp->next)
    {
      printf (" %s", tmp->str);
    }
    printf ("\n");
    printf ("mappings");
    for (edgex_nvpairs * nv = e->mappings; nv; nv = nv->next)
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
  for (edgex_strings * tmp = e->labels; tmp; tmp = tmp->next)
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
  for (edgex_deviceobject * dr = e->device_resources; dr; dr = dr->next)
  {
    printf ("name %s\n", dr->name);
    printf ("description %s\n", dr->description);
    printf ("tag %s\n", dr->tag);
    printf ("properties\n");
    printf ("type %s\n", dr->properties->value->type);
    printf ("readwrite %s\n", dr->properties->value->readwrite);
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
  printf ("commands:\n");
  for (edgex_command * cmd = e->commands; cmd; cmd = cmd->next)
  {
    edgex_response * response;
    printf ("id %s\n", cmd->id);
    printf ("name %s\n", cmd->name);
    printf ("created %lu\n", (long unsigned) cmd->created);
    printf ("modified %lu\n", (long unsigned) cmd->modified);
    printf ("origin %lu\n", (long unsigned) cmd->origin);
    printf ("get:\n");
    printf ("path %s\n", cmd->get->path);
    printf ("responses:\n");
    for (response = cmd->get->responses; response; response = response->next)
    {
      printf ("code %s\n", response->code);
      printf ("description %s\n", response->description);
      printf ("expected values");
      for (edgex_strings * tmp = response->expectedvalues; tmp; tmp = tmp->next)
      {
        printf (" %s", tmp->str);
      }
      printf ("\n");
    }
    printf ("put:\n");
    printf ("path %s\n", cmd->put->path);
    printf ("responses:\n");
    for (response = cmd->put->responses; response; response = response->next)
    {
      printf ("code %s\n", response->code);
      printf ("description %s\n", response->description);
      printf ("expected values");
      for (edgex_strings * tmp = response->expectedvalues; tmp; tmp = tmp->next)
      {
        printf (" %s", tmp->str);
      }
      printf ("\n");
    }
    printf ("parameter names");
    for (edgex_strings * tmp = cmd->put->parameter_names; tmp; tmp = tmp->next)
    {
      printf (" %s", tmp->str);
    }
    printf ("\n");
  }
  printf ("resources:\n");
  for (edgex_profileresource * resource = e->resources; resource; resource = resource->next)
  {
    printf ("name %s\n", resource->name);
    printf ("get:\n");
    resourceoperation_dump (resource->get);
    printf ("put:\n");
    resourceoperation_dump (resource->put);
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
  for (edgex_strings * tmp = e->labels; tmp; tmp = tmp->next)
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
  for (edgex_strings * tmp = e->labels; tmp; tmp = tmp->next)
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
  for (edgex_strings * tmp = e->labels; tmp; tmp = tmp->next)
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
