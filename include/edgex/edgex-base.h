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

#include "iot/data.h"
typedef iot_data_type_t edgex_propertytype;

typedef enum { LOCKED, UNLOCKED } edgex_device_adminstate;

typedef enum { ENABLED, DISABLED } edgex_device_operatingstate;

typedef struct edgex_strings
{
  char *str;
  struct edgex_strings *next;
} edgex_strings;

typedef struct edgex_nvpairs
{
  char *name;
  char *value;
  struct edgex_nvpairs *next;
} edgex_nvpairs;

/**
 * @brief Creates a new name-value pair, optionally placing it at the
 *        start of a list
 * @param name The name for the new pair
 * @param value The value for the new pair
 * @param list A list that the new pair will be placed at the front of, or NULL
 * @returns The new name-value pair
 */

edgex_nvpairs *edgex_nvpairs_new (const char *name, const char *value, edgex_nvpairs *list);

/**
 * @brief Finds a named value in an n-v pair list.
 * @param nvp A list of name-value pairs.
 * @param name The named value to search for.
 * @returns The value corresponding to the given name, or NULL if not found.
 */

const char *edgex_nvpairs_value (const edgex_nvpairs *nvp, const char *name);

/**
 * @brief Finds a named long value in an n-v pair list.
 * @param nvp A list of name-value pairs.
 * @param name The named value to search for.
 * @param val If the value was found and could be parsed, it is returned here.
 * @returns true if the operation was successful.
 */

bool edgex_nvpairs_long_value (const edgex_nvpairs *nvp, const char *name, long *val);

/**
 * @brief Finds a named unsigned long value in an n-v pair list.
 * @param nvp A list of name-value pairs.
 * @param name The named value to search for.
 * @param val If the value was found and could be parsed, it is returned here.
 * @returns true if the operation was successful.
 */

bool edgex_nvpairs_ulong_value (const edgex_nvpairs *nvp, const char *name, unsigned long *val);

/**
 * @brief Finds a named float value in an n-v pair list.
 * @param nvp A list of name-value pairs.
 * @param name The named value to search for.
 * @param val If the value was found and could be parsed, it is returned here.
 * @returns true if the operation was successful.
 */

bool edgex_nvpairs_float_value (const edgex_nvpairs *nvp, const char *name, float *val);

typedef struct edgex_blob
{
  size_t size;
  uint8_t *bytes;
} edgex_blob;

typedef union edgex_device_resultvalue
{
  bool bool_result;
  char *string_result;
  uint8_t ui8_result;
  uint16_t ui16_result;
  uint32_t ui32_result;
  uint64_t ui64_result;
  int8_t i8_result;
  int16_t i16_result;
  int32_t i32_result;
  int64_t i64_result;
  float f32_result;
  double f64_result;
  edgex_blob binary_result;
} edgex_device_resultvalue;

typedef struct edgex_protocols
{
  char *name;
  edgex_nvpairs *properties;
  struct edgex_protocols *next;
} edgex_protocols;

/**
 * @brief Finds a protocol's property set in a protocols list.
 * @param prots The protocols to search.
 * @param name The protocol to search for.
 * @returns The protocol properties corresponding to the given name, or NULL if not found.
 */

const edgex_nvpairs *edgex_protocols_properties (const edgex_protocols *prots, const char *name);

/* v1 typename compatibility */

#ifndef DEVSDKV2
#define Int8 IOT_DATA_INT8
#define Uint8 IOT_DATA_UINT8
#define Int16 IOT_DATA_INT16
#define Uint16 IOT_DATA_UINT16
#define Int32 IOT_DATA_INT32
#define Uint32 IOT_DATA_UINT32
#define Int64 IOT_DATA_INT64
#define Uint64 IOT_DATA_UINT64
#define Float32 IOT_DATA_FLOAT32
#define Float64 IOT_DATA_FLOAT64
#define Bool IOT_DATA_BOOL
#define String IOT_DATA_STRING
#define Binary IOT_DATA_ARRAY
#endif

#endif
