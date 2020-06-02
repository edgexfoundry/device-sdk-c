/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <errno.h>
#include "edgex/edgex-base.h"
#include "iot/typecode.h"

static edgex_propertytype typeForArray[] =
{
  Edgex_Int8Array, Edgex_Uint8Array, Edgex_Int16Array, Edgex_Uint16Array, Edgex_Int32Array, Edgex_Uint32Array,
  Edgex_Int64Array, Edgex_Uint64Array, Edgex_Float32Array, Edgex_Float64Array, Edgex_BoolArray
};

edgex_propertytype edgex_propertytype_data (const iot_data_t *data)
{
  edgex_propertytype res = iot_data_type (data);
  if (res == Edgex_Binary && iot_data_get_metadata (data) == NULL)
  {
    iot_data_type_t at = iot_data_array_type (data);
    res = (at <= IOT_DATA_BOOL) ? typeForArray [at] : Edgex_Unused1;
  }
  return res;
}

edgex_propertytype edgex_propertytype_typecode (const iot_typecode_t *tc)
{
  edgex_propertytype res = iot_typecode_type (tc);
  if (res == Edgex_Binary)
  {
    iot_data_type_t at = iot_typecode_type (iot_typecode_element_type (tc));
    res = (at <= IOT_DATA_BOOL) ? typeForArray [at] : Edgex_Unused1;
  }
  return res;
}

edgex_nvpairs *edgex_nvpairs_new (const char *name, const char *value, edgex_nvpairs *list)
{
  return devsdk_nvpairs_new (name, value, list);
}

const char *edgex_nvpairs_value (const edgex_nvpairs *nvp, const char *name)
{
  return devsdk_nvpairs_value (nvp, name);
}

bool edgex_nvpairs_long_value (const edgex_nvpairs *nvp, const char *name, long *val)
{
  return devsdk_nvpairs_long_value (nvp, name, val);
}

bool edgex_nvpairs_ulong_value (const edgex_nvpairs *nvp, const char *name, unsigned long *val)
{
  return devsdk_nvpairs_ulong_value (nvp, name, val);
}

bool edgex_nvpairs_float_value (const edgex_nvpairs *nvp, const char *name, float *val)
{
  return devsdk_nvpairs_float_value (nvp, name, val);
}

const edgex_nvpairs *edgex_protocols_properties (const devsdk_protocols *prots, const char *name)
{
  return devsdk_protocols_properties (prots, name);
}
