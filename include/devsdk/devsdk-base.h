/*
 * Copyright (c) 2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _DEVSDK_BASE_H_
#define _DEVSDK_BASE_H_ 1

/**
 * @file
 * @brief This file defines basic types used by SDK functions
 */

#include "iot/data.h"
#include "iot/typecode.h"

#ifdef __cplusplus
extern "C" {
#endif

struct devsdk_service_t;
typedef struct devsdk_service_t devsdk_service_t;

typedef struct devsdk_error
{
  uint32_t code;
  const char *reason;
} devsdk_error;

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

typedef struct devsdk_protocols
{
  char *name;
  devsdk_nvpairs *properties;
  struct devsdk_protocols *next;
} devsdk_protocols;

/**
 * @brief Structure containing information about a device resource which is the subject of a get or set request.
 */

typedef struct devsdk_commandrequest
{
  /** The device resource's name */
  const char *resname;
  /** Attributes of the device resource */
  const devsdk_nvpairs *attributes;
  /** Type of the data to be read or written */
  const iot_typecode_t *type;
  /** Mask to be applied for write requests */
  uint64_t mask;
} devsdk_commandrequest;

/**
 * @brief Structure containing the result of a get operation.
 */

typedef struct devsdk_commandresult
{
  /** * The timestamp of the result. Should only be set if the device itself supplies one.  */
  uint64_t origin;
  /** The result. */
  iot_data_t *value;
} devsdk_commandresult;

typedef struct devsdk_discovered_device
{
  const char *name;
  devsdk_protocols *protocols;
  const char *description;
  iot_data_t *properties;
} devsdk_discovered_device;

/**
 * @brief Linked-list structure containing information about a device's resources.
 */

typedef struct devsdk_device_resources
{
  /** The device resource's name */
  const char *resname;
  /** Attributes of the device resource */
  const devsdk_nvpairs *attributes;
  /** Type of the data that may be read or written */
  const iot_typecode_t *type;
  /** Whether the resource may be read */
  bool readable;
  /** Whether the resource may be written */
  bool writable;
  /** Next element in the list of resources */
  struct devsdk_device_resources *next;
} devsdk_device_resources;

typedef struct devsdk_devices
{
  const char *devname;
  const devsdk_protocols *protocols;
  devsdk_device_resources *resources;
  struct devsdk_devices *next;
} devsdk_devices;

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

/**
 * @brief Finds a protocol's property set in a protocols list.
 * @param prots The protocols to search.
 * @param name The protocol to search for.
 * @returns The protocol properties corresponding to the given name, or NULL if not found.
 */

const devsdk_nvpairs *devsdk_protocols_properties (const devsdk_protocols *prots, const char *name);

/**
 * @brief Creates a protocols object, optionally placing it at the start of a list
 * @param name The name for the new protocol
 * @param properties The properties for the new protocol
 * @param list A protocol list that the new protocol will be placed at the front of, or NULL
 * @returns The new protocols object
 */

devsdk_protocols *devsdk_protocols_new (const char *name, const devsdk_nvpairs *properties, devsdk_protocols *list);

/**
 * @brief Duplicates a protocols object or list
 * @param e The list to duplicate
 * @returns The new protocols object
 */

devsdk_protocols *devsdk_protocols_dup (const devsdk_protocols *e);

/**
 * @brief Free a protocols list.
 * @param p The list to free.
 */

void devsdk_protocols_free (devsdk_protocols *e);

#ifdef __cplusplus
}
#endif

#endif
