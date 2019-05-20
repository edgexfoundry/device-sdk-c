/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "transform.h"

#include <math.h>
#include <limits.h>
#include <float.h>
#include <assert.h>

static bool transformsOn (const edgex_propertyvalue *pv)
{
  return (pv->offset.enabled || pv->scale.enabled || pv->base.enabled || pv->shift.enabled || pv->mask.enabled);
}

static const char *checkMapping (const edgex_nvpairs *map, const char *in)
{
  for (const edgex_nvpairs *pair = map; pair; pair = pair->next)
  {
    if (strcmp (in, pair->name) == 0)
    {
      return pair->value;
    }
  }
  return NULL;
}

static long double getLongDouble (edgex_device_resultvalue value, edgex_propertyvalue *props)
{
  return (props->type == Float64) ? value.f64_result : value.f32_result;
}

static bool setLongDouble (long double ldval, edgex_device_resultvalue *value, edgex_propertyvalue *props)
{
  if (props->type == Float64)
  {
    if (ldval <= DBL_MAX && ldval >= -DBL_MAX)
    {
      value->f64_result = (double)ldval;
      return true;
    }
    return false;
  }
  else
  {
    if (ldval <= FLT_MAX && ldval >= -FLT_MAX)
    {
      value->f32_result = (float)ldval;
      return true;
    }
    return false;
  }
}

static long long int getLLInt (edgex_device_resultvalue value, edgex_propertyvalue *props)
{
  switch (props->type)
  {
    case Uint8: return value.ui8_result; break;
    case Uint16: return value.ui16_result; break;
    case Uint32: return value.ui32_result; break;
    case Uint64: return value.ui64_result; break;
    case Int8: return value.i8_result; break;
    case Int16: return value.i16_result; break;
    case Int32: return value.i32_result; break;
    case Int64: return value.i64_result; break;
    default: assert (0); return 0;
  }
}

static bool setLLInt (long long int llival, edgex_device_resultvalue *value, edgex_propertyvalue *props)
{
  switch (props->type)
  {
    case Uint8:
      if (llival >= 0 && llival <= UCHAR_MAX)
      {
        value->ui8_result = (uint8_t)llival;
        return true;
      }
      break;
    case Uint16:
      if (llival >= 0 && llival <= USHRT_MAX)
      {
        value->ui16_result = (uint16_t)llival;
        return true;
      }
      break;
    case Uint32:
      if (llival >= 0 && llival <= UINT_MAX)
      {
        value->ui32_result = (uint32_t)llival;
        return true;
      }
      break;
    case Uint64:
      if (llival >= 0 && llival <= ULLONG_MAX)
      {
        value->ui64_result = (uint64_t)llival;
        return true;
      }
      break;
    case Int8:
      if (llival >= SCHAR_MIN && llival <= SCHAR_MAX)
      {
        value->i8_result = (int8_t)llival;
        return true;
      }
      break;
    case Int16:
      if (llival >= SHRT_MIN && llival <= SHRT_MAX)
      {
        value->i16_result = (int16_t)llival;
        return true;
      }
      break;
    case Int32:
      if (llival >= INT_MIN && llival <= INT_MAX)
      {
        value->i32_result = (int32_t)llival;
        return true;
      }
      break;
    case Int64:
      if (llival >= LLONG_MIN && llival <= LLONG_MAX)
      {
        value->i64_result = (int64_t)llival;
        return true;
      }
      break;
    default:
      assert (0);
  }
  return false;
}

void edgex_transform_outgoing
  (edgex_device_commandresult *cres, edgex_propertyvalue *props, edgex_nvpairs *mappings)
{
  switch (props->type)
  {
    case Float32:
    case Float64:
    if (transformsOn (props))
    {
      long double result = getLongDouble (cres->value, props);
      if (props->base.enabled) result = powl (props->base.value.dval, result);
      if (props->scale.enabled) result *= props->scale.value.dval;
      if (props->offset.enabled) result += props->offset.value.dval;

      if (!setLongDouble (result, &cres->value, props))
      {
        cres->type = String;
        cres->value.string_result = strdup ("overflow");
      }
    }
    break;
    case Uint8:
    case Uint16:
    case Uint32:
    case Uint64:
    case Int8:
    case Int16:
    case Int32:
    case Int64:
    if (transformsOn (props))
    {
      long long int result = getLLInt (cres->value, props);
      if (props->mask.enabled) result &= props->mask.value.ival;
      if (props->shift.enabled)
      {
        if (props->shift.value.ival < 0)
        {
          result <<= -props->shift.value.ival;
        }
        else
        {
          result >>= props->shift.value.ival;
        }
      }
      if (props->base.enabled) result = powl (props->base.value.ival, result);
      if (props->scale.enabled) result *= props->scale.value.ival;
      if (props->offset.enabled) result += props->offset.value.ival;

      if (!setLLInt (result, &cres->value, props))
      {
        cres->type = String;
        cres->value.string_result = strdup ("overflow");
      }
    }
    break;
    case String:
    {
      const char *remap = checkMapping (mappings, cres->value.string_result);
      if (remap)
      {
        free (cres->value.string_result);
        cres->value.string_result = strdup (remap);
      }
    }
    default:
    break;
  }
}

bool edgex_transform_incoming
  (edgex_device_commandresult *cres, edgex_propertyvalue *props, edgex_nvpairs *mappings)
{
  bool success = true;
  switch (props->type)
  {
    case Float32:
    case Float64:
    if (transformsOn (props))
    {
      long double result = getLongDouble (cres->value, props);
      if (props->offset.enabled) result -= props->offset.value.dval;
      if (props->scale.enabled) result /= props->scale.value.dval;
      if (props->base.enabled) result = logl (result) / logl (props->base.value.dval);
      success = setLongDouble (result, &cres->value, props);
    }
    break;
    case Uint8:
    case Uint16:
    case Uint32:
    case Uint64:
    case Int8:
    case Int16:
    case Int32:
    case Int64:
    if (transformsOn (props))
    {
      long long int result = getLLInt (cres->value, props);
      if (props->offset.enabled) result -= props->offset.value.ival;
      if (props->scale.enabled) result /= props->scale.value.ival;
      if (props->base.enabled) result = llroundl (logl (result) / logl (props->base.value.ival));
      if (props->shift.enabled)
      {
        if (props->shift.value.ival < 0)
        {
          result >>= -props->shift.value.ival;
        }
        else
        {
          result <<= props->shift.value.ival;
        }
      }
      // Mask transform NYI. Possibly will be done in the driver.
      success = setLLInt (result, &cres->value, props);
    }
    break;
    case String:
    {
      const char *remap = checkMapping (mappings, cres->value.string_result);
      if (remap)
      {
        free (cres->value.string_result);
        cres->value.string_result = strdup (remap);
      }
    }
    default:
    break;
  }
  return success;
}
