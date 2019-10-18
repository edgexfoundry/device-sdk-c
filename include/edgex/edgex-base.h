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
 * @brief Finds a named value in an n-v pair list.
 * @param nvp A list of name-value pairs.
 * @param name The named value to search for.
 * @returns The value corresponding to the given name, or NULL if not found.
 */

const char *edgex_nvpairs_value (const edgex_nvpairs *nvp, const char *name);

typedef struct edgex_blob
{
  size_t size;
  uint8_t *bytes;
} edgex_blob;

typedef enum
{
  Bool,
  String,
  Binary,
  Uint8, Uint16, Uint32, Uint64,
  Int8, Int16, Int32, Int64,
  Float32, Float64
} edgex_propertytype;

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

#endif
