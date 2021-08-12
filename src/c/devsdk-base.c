/*
 * Copyright (c) 2020-2021
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <errno.h>
#include "devsdk/devsdk.h"
#include "devutil.h"
#include "service.h"

devsdk_strings *devsdk_strings_new (const char *str, devsdk_strings *list)
{
  devsdk_strings *result = malloc (sizeof (devsdk_strings));
  result->str = strdup (str);
  result->next = list;
  return result;
}

devsdk_nvpairs *devsdk_nvpairs_new (const char *name, const char *value, devsdk_nvpairs *list)
{
  devsdk_nvpairs *result = malloc (sizeof (devsdk_nvpairs));
  result->name = strdup (name);
  result->value = strdup (value);
  result->next = list;
  return result;
}

const char *devsdk_nvpairs_reverse_value (const devsdk_nvpairs *nvp, const char *name)
{
  if (name)
  {
    for (; nvp; nvp = nvp->next)
    {
      if (strcmp (nvp->value, name) == 0)
      {
        return nvp->name;
      }
    }
  }
  return NULL;
}

const char *devsdk_nvpairs_value (const devsdk_nvpairs *nvp, const char *name)
{
  if (name)
  {
    for (; nvp; nvp = nvp->next)
    {
      if (strcmp (nvp->name, name) == 0)
      {
        return nvp->value;
      }
    }
  }
  return NULL;
}

const char *devsdk_nvpairs_value_dfl (const devsdk_nvpairs *nvp, const char *name, const char *dfl)
{
  const char *res = devsdk_nvpairs_value (nvp, name);
  return res ? res : dfl;
}

bool devsdk_nvpairs_long_value (const devsdk_nvpairs *nvp, const char *name, long *val)
{
  bool result = false;
  const char *v = devsdk_nvpairs_value (nvp, name);
  if (v && *v)
  {
    char *end = NULL;
    errno = 0;
    long l = strtol (v, &end, 0);
    if (errno == 0 && *end == 0)
    {
      *val = l;
      result = true;
    }
  }
  return result;
}

bool devsdk_nvpairs_ulong_value (const devsdk_nvpairs *nvp, const char *name, unsigned long *val)
{
  bool result = false;
  const char *v = devsdk_nvpairs_value (nvp, name);
  if (v && *v)
  {
    char *end = NULL;
    errno = 0;
    unsigned long l = strtoul (v, &end, 0);
    if (errno == 0 && *end == 0)
    {
      *val = l;
      result = true;
    }
  }
  return result;
}

bool devsdk_nvpairs_float_value (const devsdk_nvpairs *nvp, const char *name, float *val)
{
  bool result = false;
  const char *v = devsdk_nvpairs_value (nvp, name);
  if (v && *v)
  {
    char *end = NULL;
    errno = 0;
    float f = strtof (v, &end);
    if (errno == 0 && *end == 0)
    {
      *val = f;
      result = true;
    }
  }
  return result;
}

devsdk_protocols *devsdk_protocols_new (const char *name, const iot_data_t *properties, devsdk_protocols *list)
{
  devsdk_protocols *result = malloc (sizeof (devsdk_protocols));
  result->name = strdup (name);
  result->properties = iot_data_copy (properties);
  result->next = list;
  return result;
}

const iot_data_t *devsdk_protocols_properties (const devsdk_protocols *prots, const char *name)
{
  if (name)
  {
    for (; prots; prots = prots->next)
    {
      if (strcmp (prots->name, name) == 0)
      {
        return prots->properties;
      }
    }
  }
  return NULL;
}

devsdk_callbacks *devsdk_callbacks_init
(
  devsdk_initialize init,
  devsdk_handle_get gethandler,
  devsdk_handle_put puthandler,
  devsdk_stop stop,
  devsdk_create_address create_addr,
  devsdk_free_address free_addr,
  devsdk_create_resource_attr create_res,
  devsdk_free_resource_attr free_res
)
{
  devsdk_callbacks *cb = calloc (1, sizeof (devsdk_callbacks));
  cb->init = init;
  cb->gethandler = gethandler;
  cb->puthandler = puthandler;
  cb->stop = stop;
  cb->create_addr = create_addr;
  cb->free_addr = free_addr;
  cb->create_res = create_res;
  cb->free_res = free_res;
  return cb;
}

void devsdk_callbacks_set_discovery (devsdk_callbacks *cb, devsdk_discover discover, devsdk_describe describe)
{
  cb->discover = discover;
  cb->describe = describe;
}

void devsdk_callbacks_set_reconfiguration (devsdk_callbacks *cb, devsdk_reconfigure reconf)
{
  cb->reconfigure = reconf;
}

void devsdk_callbacks_set_listeners
  (devsdk_callbacks *cb, devsdk_add_device_callback device_added, devsdk_update_device_callback device_updated, devsdk_remove_device_callback device_removed)
{
  cb->device_added = device_added;
  cb->device_updated = device_updated;
  cb->device_removed = device_removed;
}

void devsdk_callbacks_set_autoevent_handlers (devsdk_callbacks *cb, devsdk_autoevent_start_handler ae_starter, devsdk_autoevent_stop_handler ae_stopper)
{
  cb->ae_starter = ae_starter;
  cb->ae_stopper = ae_stopper;
}

struct sfx_struct
{
  const char *str;
  uint64_t factor;
};

static struct sfx_struct time_suffixes[] =
  { { "ms", 1 }, { "s", 1000 }, { "m", 60000 }, { "h", 3600000 }, { NULL, 0 } };

uint64_t edgex_parsetime (const char *spec)
{
  char *fend;
  uint64_t fnum = strtoul (spec, &fend, 10);
  for (int i = 0; time_suffixes[i].str; i++)
  {
    if (strcmp (fend, time_suffixes[i].str) == 0)
    {
      return fnum * time_suffixes[i].factor;
    }
  }
  return 0;
}

void devsdk_wait_msecs (uint64_t duration)
{
  struct timespec tm, rem;
  tm.tv_sec = duration / 1000;
  tm.tv_nsec = 1000000 * (duration % 1000);
  while (nanosleep (&tm, &rem) == -1 && errno == EINTR)
  {
    tm = rem;
  }
}

unsigned long devsdk_strtoul_dfl (const char *val, unsigned long dfl)
{
  if (val && *val)
  {
    char *end = NULL;
    errno = 0;
    unsigned long l = strtoul (val, &end, 0);
    if (errno == 0 && *end == 0)
    {
      return l;
    }
  }
  return dfl;
}

/* Macro for generating single-linked-list comparison functions.
 * Assumes a "next" pointer and that the key (name) field is a string.
 */

#define LIST_EQUAL_FUNCTION(TYPENAME,NAMEFIELD,CMPFUNC)           \
bool TYPENAME ## _equal (const TYPENAME *l1, const TYPENAME *l2)  \
{                                                                 \
  const TYPENAME *l;                                              \
  const TYPENAME *found;                                          \
  unsigned n1 = 0;                                                \
  unsigned n2 = 0;                                                \
  for (l = l1; l; l = l->next, n1++);                             \
  for (l = l2; l; l = l->next, n2++);                             \
  if (n1 != n2) return false;                                     \
  for (l = l1; l; l = l->next)                                    \
  {                                                               \
    for (found = l2; found; found = found->next)                  \
    {                                                             \
      if (strcmp (l->NAMEFIELD, found->NAMEFIELD) == 0) break;    \
    }                                                             \
    if (!found || !CMPFUNC (l, found)) return false;              \
  }                                                               \
  return true;                                                    \
}

static bool protocol_equal (const devsdk_protocols *p1, const devsdk_protocols *p2)
{
  return iot_data_equal (p1->properties, p2->properties);
}

LIST_EQUAL_FUNCTION(devsdk_protocols, name, protocol_equal)

static bool autoevent_equal (const edgex_device_autoevents *e1, const edgex_device_autoevents *e2)
{
  return strcmp (e1->interval, e2->interval) == 0 && e1->onChange == e2->onChange;
}

LIST_EQUAL_FUNCTION(edgex_device_autoevents, resource, autoevent_equal)
