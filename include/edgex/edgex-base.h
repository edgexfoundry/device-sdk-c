/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_EDGEX_BASE_H_
#define _EDGEX_EDGEX_BASE_H_

#include "edgex/os.h"
#include "devsdk/devsdk-base.h"

typedef enum edgex_propertytype
{
  Edgex_Int8 = IOT_DATA_INT8,
  Edgex_Uint8 = IOT_DATA_UINT8,
  Edgex_Int16 = IOT_DATA_INT16,
  Edgex_Uint16 = IOT_DATA_UINT16,
  Edgex_Int32 = IOT_DATA_INT32,
  Edgex_Uint32 = IOT_DATA_UINT32,
  Edgex_Int64 = IOT_DATA_INT64,
  Edgex_Uint64 = IOT_DATA_UINT64,
  Edgex_Float32 = IOT_DATA_FLOAT32,
  Edgex_Float64 = IOT_DATA_FLOAT64,
  Edgex_Bool = IOT_DATA_BOOL,
  Edgex_String = IOT_DATA_STRING,
  Edgex_Binary = IOT_DATA_ARRAY,
  Edgex_Unused1 = IOT_DATA_MAP,
  Edgex_Unused2 = IOT_DATA_VECTOR,
  Edgex_Int8Array,
  Edgex_Uint8Array,
  Edgex_Int16Array,
  Edgex_Uint16Array,
  Edgex_Int32Array,
  Edgex_Uint32Array,
  Edgex_Int64Array,
  Edgex_Uint64Array,
  Edgex_Float32Array,
  Edgex_Float64Array,
  Edgex_BoolArray
} edgex_propertytype;

edgex_propertytype edgex_propertytype_data (const iot_data_t *data);
edgex_propertytype edgex_propertytype_typecode (const iot_typecode_t *tc);

typedef enum { LOCKED, UNLOCKED } edgex_device_adminstate;

typedef enum { UP, DOWN } edgex_device_operatingstate;

#endif
