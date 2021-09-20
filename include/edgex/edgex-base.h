/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_EDGEX_BASE_H_
#define _EDGEX_EDGEX_BASE_H_

#include "iot/data.h"

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
  Edgex_Unused1 = IOT_DATA_NULL,
  Edgex_Binary = IOT_DATA_ARRAY,
  Edgex_Object = IOT_DATA_MAP,
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

typedef struct devsdk_strings
{
  char *str;
  struct devsdk_strings *next;
} devsdk_strings;

typedef struct devsdk_nvpairs
{
  char *name;
  char *value;
  struct devsdk_nvpairs *next;
} devsdk_nvpairs;

/**
 * @brief Creates a new string list, optionally adding to an existing list
 * @param str The string to be added
 * @param list A list that will be extended, or NULL
 * @returns The new string list
 */

devsdk_strings *devsdk_strings_new (const char *str, devsdk_strings *list);

/**
 * @brief Free an strings list.
 * @param p The list to free.
 */

void devsdk_strings_free (devsdk_strings *s);

/**
 * @brief Creates a new name-value pair, optionally placing it at the
 *        start of a list
 * @param name The name for the new pair
 * @param value The value for the new pair
 * @param list A list that the new pair will be placed at the front of, or NULL
 * @returns The new name-value pair
 */

devsdk_nvpairs *devsdk_nvpairs_new (const char *name, const char *value, devsdk_nvpairs *list);

/**
 * @brief Finds a named value in an n-v pair list.
 * @param nvp A list of name-value pairs.
 * @param name The named value to search for.
 * @returns The value corresponding to the given name, or NULL if not found.
 */

const char *devsdk_nvpairs_value (const devsdk_nvpairs *nvp, const char *name);

/**
 * @brief Finds a named value in an n-v pair list.
 * @param nvp A list of name-value pairs.
 * @param name The named value to search for.
 * @param dfl The default for this query.
 * @returns The value corresponding to the given name, or <default> if not found.
 */

const char *devsdk_nvpairs_value_dfl (const devsdk_nvpairs *nvp, const char *name, const char *dfl);

/**
 * @brief Finds a named long value in an n-v pair list.
 * @param nvp A list of name-value pairs.
 * @param name The named value to search for.
 * @param val If the value was found and could be parsed, it is returned here.
 * @returns true if the operation was successful.
 */

bool devsdk_nvpairs_long_value (const devsdk_nvpairs *nvp, const char *name, long *val);

/**
 * @brief Finds a named unsigned long value in an n-v pair list.
 * @param nvp A list of name-value pairs.
 * @param name The named value to search for.
 * @param val If the value was found and could be parsed, it is returned here.
 * @returns true if the operation was successful.
 */

bool devsdk_nvpairs_ulong_value (const devsdk_nvpairs *nvp, const char *name, unsigned long *val);

/**
 * @brief Finds a named float value in an n-v pair list.
 * @param nvp A list of name-value pairs.
 * @param name The named value to search for.
 * @param val If the value was found and could be parsed, it is returned here.
 * @returns true if the operation was successful.
 */

bool devsdk_nvpairs_float_value (const devsdk_nvpairs *nvp, const char *name, float *val);

/**
 * @brief Finds a name for a value in an n-v pair list.
 * @param nvp A list of name-value pairs.
 * @param name The value to search for.
 * @returns The name corresponding to the given value, or NULL if not found.
 */

const char *devsdk_nvpairs_reverse_value (const devsdk_nvpairs *nvp, const char *name);

/**
 * @brief Duplicates a n-v pair list
 * @param e The list to duplicate
 * @returns The new list
 */

devsdk_nvpairs *devsdk_nvpairs_dup (const devsdk_nvpairs *nvp);

/**
 * @brief Free an n-v pair list.
 * @param p The list to free.
 */

void devsdk_nvpairs_free (devsdk_nvpairs *p);

#endif
