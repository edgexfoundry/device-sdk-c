/*
 * Copyright (c) 2019-2020
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

static long double getLongDouble (const iot_data_t *value, edgex_propertytype pt)
{
  return (pt == Edgex_Float64) ? iot_data_f64 (value) : iot_data_f32 (value);
}

static iot_data_t *setLongDouble (long double ldval, edgex_propertytype pt)
{
  if (pt == Edgex_Float64)
  {
    return (ldval <= DBL_MAX && ldval >= -DBL_MAX) ? iot_data_alloc_f64 (ldval) : NULL;
  }
  else
  {
    return (ldval <= FLT_MAX && ldval >= -FLT_MAX) ? iot_data_alloc_f32 (ldval) : NULL;
  }
}

static long long int getLLInt (const iot_data_t *value, edgex_propertytype pt)
{
  switch (pt)
  {
    case Edgex_Int8: return iot_data_i8 (value); break;
    case Edgex_Uint8: return iot_data_ui8 (value); break;
    case Edgex_Int16: return iot_data_i16 (value); break;
    case Edgex_Uint16: return iot_data_ui16 (value); break;
    case Edgex_Int32: return iot_data_i32 (value); break;
    case Edgex_Uint32: return iot_data_ui32 (value); break;
    case Edgex_Int64: return iot_data_i64 (value); break;
    case Edgex_Uint64: return iot_data_ui64 (value); break;
    default: assert (0); return 0;
  }
}

static iot_data_t *setLLInt (long long int llival, edgex_propertytype pt)
{
  switch (pt)
  {
    case Edgex_Int8:
      return (llival >= SCHAR_MIN && llival <= SCHAR_MAX) ? iot_data_alloc_i8 (llival) : NULL;
      break;
    case Edgex_Uint8:
      return (llival >= 0 && llival <= UCHAR_MAX) ? iot_data_alloc_ui8 (llival) : NULL;
      break;
    case Edgex_Int16:
      return (llival >= SHRT_MIN && llival <= SHRT_MAX) ? iot_data_alloc_i16 (llival) : NULL;
      break;
    case Edgex_Uint16:
      return (llival >= 0 && llival <= USHRT_MAX) ? iot_data_alloc_ui16 (llival) : NULL;
      break;
    case Edgex_Int32:
      return (llival >= INT_MIN && llival <= INT_MAX) ? iot_data_alloc_i32 (llival) : NULL;
      break;
    case Edgex_Uint32:
      return (llival >= 0 && llival <= UINT_MAX) ? iot_data_alloc_ui32 (llival) : NULL;
      break;
    case Edgex_Int64:
      return (llival >= LLONG_MIN && llival <= LLONG_MAX) ? iot_data_alloc_i64 (llival) : NULL;
      break;
    case Edgex_Uint64:
      return (llival >= 0 && llival <= ULLONG_MAX) ? iot_data_alloc_ui64 (llival) : NULL;
      break;
    default:
      assert (0);
      return NULL;
  }
}

void edgex_transform_outgoing
  (devsdk_commandresult *cres, edgex_propertyvalue *props, devsdk_nvpairs *mappings)
{
  edgex_propertytype pt = edgex_propertytype_data (cres->value);
  switch (pt)
  {
    case Edgex_Float32:
    case Edgex_Float64:
    if (transformsOn (props))
    {
      long double result = getLongDouble (cres->value, pt);
      if (props->base.enabled) result = powl (props->base.value.dval, result);
      if (props->scale.enabled) result *= props->scale.value.dval;
      if (props->offset.enabled) result += props->offset.value.dval;

      iot_data_free (cres->value);
      cres->value = setLongDouble (result, pt);
      if (cres->value == NULL)
      {
        cres->value = iot_data_alloc_string ("overflow", IOT_DATA_REF);
      }
    }
    break;
    case Edgex_Int8:
    case Edgex_Uint8:
    case Edgex_Int16:
    case Edgex_Uint16:
    case Edgex_Int32:
    case Edgex_Uint32:
    case Edgex_Int64:
    case Edgex_Uint64:
    if (transformsOn (props))
    {
      long long int result = getLLInt (cres->value, pt);
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

      iot_data_free (cres->value);
      cres->value = setLLInt (result, pt);
      if (cres->value == NULL)
      {
        cres->value = iot_data_alloc_string ("overflow", IOT_DATA_REF);
      }
    }
    break;
    case Edgex_String:
    {
      const char *remap = devsdk_nvpairs_value (mappings, iot_data_string (cres->value));
      if (remap)
      {
        iot_data_free (cres->value);
        cres->value = iot_data_alloc_string (remap, IOT_DATA_REF);
      }
    }
    default:
    break;
  }
}

void edgex_transform_incoming (iot_data_t **cres, edgex_propertyvalue *props, devsdk_nvpairs *mappings)
{
  switch (props->type)
  {
    case IOT_DATA_FLOAT32:
    case IOT_DATA_FLOAT64:
    if (transformsOn (props))
    {
      long double result = getLongDouble (*cres, props->type);
      if (props->offset.enabled) result -= props->offset.value.dval;
      if (props->scale.enabled) result /= props->scale.value.dval;
      if (props->base.enabled) result = logl (result) / logl (props->base.value.dval);
      iot_data_free (*cres);
      *cres = setLongDouble (result, props->type);
    }
    break;
    case IOT_DATA_INT8:
    case IOT_DATA_UINT8:
    case IOT_DATA_INT16:
    case IOT_DATA_UINT16:
    case IOT_DATA_INT32:
    case IOT_DATA_UINT32:
    case IOT_DATA_INT64:
    case IOT_DATA_UINT64:
    if (transformsOn (props))
    {
      long long int result = getLLInt (*cres, props->type);
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
      if (props->mask.enabled) result &= props->mask.value.ival;
      iot_data_free (*cres);
      *cres = setLLInt (result, props->type);
    }
    break;
    case IOT_DATA_STRING:
    {
      const char *remap = devsdk_nvpairs_value (mappings, iot_data_string (*cres));
      if (remap)
      {
        iot_data_free (*cres);
        *cres = iot_data_alloc_string (remap, IOT_DATA_REF);
      }
    }
    default:
    break;
  }
}

bool edgex_transform_validate (const iot_data_t *val, const edgex_propertyvalue *props)
{
  bool result = true;
  if (props->minimum.enabled || props->maximum.enabled)
  {
    switch (props->type)
    {
      case IOT_DATA_FLOAT32:
      case IOT_DATA_FLOAT64:
      {
        long double ldval = getLongDouble (val, props->type);
        if (props->maximum.enabled)
        {
          result = (ldval <= props->maximum.value.dval);
        }
        if (result && props->minimum.enabled)
        {
          result = (ldval >= props->minimum.value.dval);
        }
      }
      break;
      case IOT_DATA_INT8:
      case IOT_DATA_UINT8:
      case IOT_DATA_INT16:
      case IOT_DATA_UINT16:
      case IOT_DATA_INT32:
      case IOT_DATA_UINT32:
      case IOT_DATA_INT64:
      case IOT_DATA_UINT64:
      {
        long long int llval = getLLInt (val, props->type);
        if (props->maximum.enabled)
        {
          result = (llval <= props->maximum.value.ival);
        }
        if (result && props->minimum.enabled)
        {
          result = (llval >= props->minimum.value.ival);
        }
      }
      break;
      default:
      break;
    }
  }
  return result;
}
