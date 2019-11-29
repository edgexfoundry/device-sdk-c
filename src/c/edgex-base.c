/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <errno.h>
#include "edgex/edgex-base.h"

edgex_nvpairs *edgex_nvpairs_new (const char *name, const char *value, edgex_nvpairs *list)
{
  edgex_nvpairs *result = malloc (sizeof (edgex_nvpairs));
  result->name = strdup (name);
  result->value = strdup (value);
  result->next = list;
  return result;
}

const char *edgex_nvpairs_value (const edgex_nvpairs *nvp, const char *name)
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

bool edgex_nvpairs_long_value (const edgex_nvpairs *nvp, const char *name, long *val)
{
  bool result = false;
  const char *v = edgex_nvpairs_value (nvp, name);
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

bool edgex_nvpairs_ulong_value (const edgex_nvpairs *nvp, const char *name, unsigned long *val)
{
  bool result = false;
  const char *v = edgex_nvpairs_value (nvp, name);
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

bool edgex_nvpairs_float_value (const edgex_nvpairs *nvp, const char *name, float *val)
{
  bool result = false;
  const char *v = edgex_nvpairs_value (nvp, name);
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

const edgex_nvpairs *edgex_protocols_properties (const edgex_protocols *prots, const char *name)
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
