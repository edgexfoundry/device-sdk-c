/*
 * Copyright (c) 2018-2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "edgex-rest.h"
#include "api.h"
#include "devutil.h"
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

char *devsdk_nvpairs_write (const devsdk_nvpairs *e)
{
  JSON_Value *val = nvpairs_write (e);
  char *result = json_serialize_to_string (val);
  json_value_free (val);
  return result;
}

devsdk_nvpairs *devsdk_nvpairs_read (const JSON_Object *obj)
{
  devsdk_nvpairs *result = NULL;
  size_t count = json_object_get_count (obj);
  for (size_t i = 0; i < count; i++)
  {
    result = devsdk_nvpairs_new (json_object_get_name (obj, i), json_string (json_object_get_value_at (obj, i)), result);
  }
  return result;
}

static iot_data_t *string_map_read (const JSON_Value *val)
{
  iot_data_t *result;
  if (val)
  {
    char *str = json_serialize_to_string (val);
    result = iot_data_from_json (str);
    json_free_serialized_string (str);
  }
  else
  {
    result = iot_data_alloc_map (IOT_DATA_STRING);
  }
  return result;
}

static JSON_Value *string_map_write (const iot_data_t *map)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);

  if (map && (iot_data_type(map) == IOT_DATA_MAP))
  {  
    iot_data_map_iter_t iter;
    iot_data_map_iter (map, &iter);
    while (iot_data_map_iter_next (&iter))
    {
      json_object_set_string (obj, iot_data_map_iter_string_key (&iter), iot_data_map_iter_string_value (&iter));
    }
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
  "Float32", "Float64", "Bool", "Unused1", "String", "Unused2", "Binary", "Unused3", "Unused4", "Unused5", "Object"
};

static const char *arrProptypes[] =
{
  "Int8Array", "Uint8Array", "Int16Array", "Uint16Array", "Int32Array", "Uint32Array", "Int64Array", "Uint64Array",
  "Float32Array", "Float64Array", "BoolArray"
};

const char *edgex_typecode_tostring (iot_typecode_t tc)
{
  return tc.type == IOT_DATA_ARRAY ? arrProptypes[tc.element_type] : proptypes[tc.type];
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

static edgex_deviceresource *edgex_deviceresource_dup (const edgex_deviceresource *e)
{
  edgex_deviceresource *result = NULL;
  edgex_deviceresource **current = &result;
  while (e)
  {
    edgex_deviceresource *elem = malloc (sizeof (edgex_deviceresource));
    elem->name = strdup (e->name);
    elem->description = strdup (e->description);
    elem->tag = strdup (e->tag);
    elem->properties = propertyvalue_dup (e->properties);
    elem->attributes = iot_data_copy (e->attributes);
    elem->parsed_attrs = NULL;
    elem->next = NULL;
    *current = elem;
    current = &(elem->next);
    e = e->next;
  }
  return result;
}

static void deviceresource_free (devsdk_service_t *svc, edgex_deviceresource *e)
{
  while (e)
  {
    edgex_deviceresource *current = e;
    free (e->name);
    free (e->description);
    free (e->tag);
    propertyvalue_free (e->properties);
    iot_data_free (e->attributes);
    if (e->parsed_attrs)
    {
      svc->userfns.free_res (svc->userdata, e->parsed_attrs);
    }
    e = e->next;
    free (current);
  }
}

static edgex_resourceoperation *resourceoperation_dup (const edgex_resourceoperation *ro)
{
  edgex_resourceoperation *result = NULL;
  edgex_resourceoperation **current = &result;
  while (ro)
  {
    edgex_resourceoperation *elem = malloc (sizeof (edgex_resourceoperation));
    elem->deviceResource = strdup (ro->deviceResource);
    elem->defaultValue = strdup (ro->defaultValue);
    elem->mappings = iot_data_add_ref (ro->mappings);
    elem->next = NULL;
    *current = elem;
    current = &(elem->next);
    ro = ro->next;
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
    iot_data_free (e->mappings);
    e = e->next;
    free (current);
  }
}

static edgex_devicecommand *devicecommand_dup (const edgex_devicecommand *pr)
{
  edgex_devicecommand *result = NULL;
  edgex_devicecommand **current = &result;
  while (pr)
  {
    edgex_devicecommand *elem = malloc (sizeof (edgex_devicecommand));
    elem->name = strdup (pr->name);
    elem->readable = pr->readable;
    elem->writable = pr->writable;
    elem->resourceOperations = resourceoperation_dup (pr->resourceOperations);
    elem->next = NULL;
    *current = elem;
    current = &(elem->next);
    pr = pr->next;
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

static void cmdinfo_free (edgex_cmdinfo *inf)
{
  while (inf)
  {
    edgex_cmdinfo *next = inf->next;
    for (unsigned i = 0; i < inf->nreqs; i++)
    {
      free (inf->reqs[i].resource);
    }
    free (inf->reqs);
    free (inf->pvals);
    free (inf->maps);
    free (inf->dfls);
    free (inf);
    inf = next;
  }
}

static edgex_device_autoevents *autoevent_read (const JSON_Object *obj)
{
  edgex_device_autoevents *result = malloc (sizeof (edgex_device_autoevents));
  result->resource = get_string (obj, "sourceName");
  result->onChange = get_boolean (obj, "onChange", false);
  result->onChangeThreshold = json_object_get_number (obj, "onChangeThreshold");
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
    json_object_set_number (pobj, "onChangeThreshold", ae->onChangeThreshold);
    json_array_append_value (arr, pval);
  }
  return result;
}

static edgex_device_autoevents *autoevents_dup
  (const edgex_device_autoevents *e)
{
  edgex_device_autoevents *result = NULL;
  edgex_device_autoevents **current = &result;
  while (e)
  {
    edgex_device_autoevents *elem = malloc (sizeof (edgex_device_autoevents));
    elem->resource = strdup (e->resource);
    elem->interval = strdup (e->interval);
    elem->onChange = e->onChange;
    elem->onChangeThreshold = e->onChangeThreshold;
    elem->impl = NULL;
    elem->next = NULL;
    *current = elem;
    current = &(elem->next);
    e = e->next;
  }
  return result;
}

void edgex_device_autoevents_free (edgex_device_autoevents *e)
{
  while (e)
  {
    edgex_device_autoevents *next = e->next;
    free (e->resource);
    free (e->interval);
    free (e);
    e = next;
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
    prot->properties = string_map_read (pval);
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
    json_object_set_value (obj, prot->name, string_map_write (prot->properties));
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
    newprot->properties = iot_data_copy (p->properties);
    newprot->next = result;
    result = newprot;
  }
  return result;
}

void devsdk_protocols_free (devsdk_protocols *e)
{
  while (e)
  {
    devsdk_protocols *next = e->next;
    free (e->name);
    iot_data_free (e->properties);
    free (e);
    e = next;
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

  json_object_set_string (obj, "apiVersion", EDGEX_API_VERSION);
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
    dest->manufacturer = SAFE_STRDUP (src->manufacturer);
    dest->model = SAFE_STRDUP (src->model);
    dest->labels = devsdk_strings_dup (src->labels);
    dest->device_resources = edgex_deviceresource_dup (src->device_resources);
    dest->device_commands = devicecommand_dup (src->device_commands);
  }
  return dest;
}

void edgex_deviceprofile_free (devsdk_service_t *svc, edgex_deviceprofile *e)
{
  while (e)
  {
    edgex_deviceprofile *next = e->next;
    free (e->name);
    free (e->description);
    free (e->manufacturer);
    free (e->model);
    devsdk_strings_free (e->labels);
    deviceresource_free (svc, e->device_resources);
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

JSON_Value *edgex_wrap_request_single (const char *objName, JSON_Value *payload)
{
  JSON_Value *val = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (val);

  json_object_set_string (obj, "apiVersion", EDGEX_API_VERSION);
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

  json_object_set_string (dsobj, "apiVersion", EDGEX_API_VERSION);
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
  result->parent = get_string (obj, "parent");
  // If the parent is empty, set it to NULL, helps avoid breakage if core-metadata support
  // for the field is not yet in place
  if (result->parent && *result->parent == '\0')
  {
    free (result->parent);
    result->parent = NULL;
  }  
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
  result->devimpl = malloc (sizeof (devsdk_device_t));
  result->devimpl->name = result->name;
  result->devimpl->address = NULL;
  result->next = NULL;

  return result;
}

static JSON_Value *device_write (const edgex_device *e)
{
  JSON_Value *result = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (result);

  json_object_set_string (obj, "apiVersion", EDGEX_API_VERSION);
  json_object_set_string (obj, "profileName", e->profile->name);
  json_object_set_string (obj, "serviceName", e->servicename);
  json_object_set_value (obj, "protocols", protocols_write (e->protocols));
  json_object_set_value (obj, "autoevents", autoevents_write (e->autos));
  json_object_set_string (obj, "adminState", edgex_adminstate_tostring (e->adminState));
  json_object_set_string (obj, "operatingState", edgex_operatingstate_tostring (e->operatingState));
  json_object_set_string (obj, "name", e->name);
  if (e->parent) // omit-if-empty
  {
    json_object_set_string (obj, "parent", e->parent);
  }
  json_object_set_string (obj, "description", e->description);
  json_object_set_value (obj, "labels", strings_to_array (e->labels));
  json_object_set_uint (obj, "origin", e->origin);

  return result;
}

edgex_device *edgex_device_dup (const edgex_device *e)
{
  edgex_device *result = malloc (sizeof (edgex_device));
  result->name = strdup (e->name);
result->parent = e->parent ? strdup(e->parent) : NULL;
  result->description = strdup (e->description);
  result->labels = devsdk_strings_dup (e->labels);
  result->protocols = devsdk_protocols_dup (e->protocols);
  result->autos = autoevents_dup (e->autos);
  result->adminState = e->adminState;
  result->operatingState = e->operatingState;
  result->origin = e->origin;
  result->servicename = strdup (e->servicename);
  result->profile = edgex_deviceprofile_dup (e->profile);
  result->devimpl = malloc (sizeof (devsdk_device_t));
  result->devimpl->name = result->name;
  result->devimpl->address = NULL;
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

devsdk_device_resources *edgex_profile_toresources (const edgex_deviceprofile *p)
{
  devsdk_device_resources *result = NULL;

  for (const edgex_deviceresource *r = p->device_resources; r; r = r->next)
  {
    devsdk_device_resources *entry = malloc (sizeof (devsdk_device_resources));
    entry->resname = strdup (r->name);
    entry->attributes = iot_data_copy (r->attributes);
    entry->type = r->properties->type;
    entry->readable = r->properties->readable;
    entry->writable = r->properties->writable;
    entry->next = result;
    result = entry;
  }
  return result;
}

devsdk_devices *edgex_device_todevsdk (devsdk_service_t *svc, const edgex_device *e)
{
  iot_data_t *exc = NULL;
  devsdk_devices *result = malloc (sizeof (devsdk_devices));
  result->device = malloc (sizeof (devsdk_device_t));
  result->device->name = strdup (e->name);
  result->device->address = svc->userfns.create_addr (svc->userdata, e->protocols, &exc);
  result->resources = edgex_profile_toresources (e->profile);
  result->next = NULL;
  iot_data_free (exc);
  return result;
}

void edgex_device_free (devsdk_service_t *svc, edgex_device *e)
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
      edgex_deviceprofile_free (svc, e->profile);
    }
    free (e->servicename);
    if (e->devimpl->address)
    {
      svc->userfns.free_addr (svc->userdata, e->devimpl->address);
    }
    free (e->devimpl);
    if (e->parent)
    {
      free (e->parent);
    }
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
  const char * parent,
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
  if (parent) { json_object_set_string (obj, "parent", parent); }
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

edgex_watcher *edgex_watcher_dup (const edgex_watcher *e)
{
  edgex_watcher *res = malloc (sizeof (edgex_watcher));
  res->regs = NULL;
  res->name = strdup (e->name);
  res->identifiers = iot_data_add_ref (e->identifiers);
  res->blocking_identifiers = iot_data_add_ref (e->blocking_identifiers);
  res->autoevents = autoevents_dup (e->autoevents);
  res->profile = strdup (e->profile);
  res->adminstate = e->adminstate;
  res->enabled = e->enabled;
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
    iot_data_free (ew->identifiers);
    edgex_watcher_regexes_free (ew->regs);
    iot_data_free (ew->blocking_identifiers);
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
  res->apiVersion = EDGEX_API_VERSION;
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
  json_object_set_string (obj, "serviceName", pr->svcname);
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
  json_object_set_string (obj, "serviceName", cr->svcname);
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
  json_object_set_string (obj, "serviceName", mr->svcname);
  return result;
}

void edgex_metricsresponse_write (const edgex_metricsresponse *mr, devsdk_http_reply *reply)
{
  JSON_Value *val = metricsesponse_write (mr);
  value_write (val, reply);
}
