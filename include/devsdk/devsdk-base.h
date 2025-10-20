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

typedef struct devsdk_protocols devsdk_protocols;

typedef void* devsdk_address_t;

typedef void* devsdk_resource_attr_t;
typedef void* devsdk_resource_tag_t;

typedef struct devsdk_device_t
{
  char *name;
  devsdk_address_t address;
} devsdk_device_t;

typedef struct devsdk_resource_t
{
  char *name;
  devsdk_resource_attr_t attrs;
  iot_data_t *tags;
  iot_typecode_t type;
} devsdk_resource_t;

/**
 * @brief Structure containing information about a device resource which is the subject of a get or set request.
 */

typedef struct devsdk_commandrequest
{
  /** The resource to be read or written */
  devsdk_resource_t *resource;
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
  const char *parent;
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
  char *resname;
  /** Attributes of the device resource */
  iot_data_t *attributes;
  /** Tags of the device resource */
  iot_data_t *tags;
  /** Type of the data that may be read or written */
  iot_typecode_t type;
  /** Whether the resource may be read */
  bool readable;
  /** Whether the resource may be written */
  bool writable;
  /** Next element in the list of resources */
  struct devsdk_device_resources *next;
} devsdk_device_resources;

typedef struct devsdk_devices
{
  devsdk_device_t *device;
  devsdk_device_resources *resources;
  struct devsdk_devices *next;
} devsdk_devices;

/**
 * @brief Finds a protocol's property set in a protocols list.
 * @param prots The protocols to search.
 * @param name The protocol to search for.
 * @returns The protocol properties corresponding to the given name, or NULL if not found.
 */

const iot_data_t *devsdk_protocols_properties (const devsdk_protocols *prots, const char *name);

/**
 * @brief Creates a protocols object, optionally placing it at the start of a list
 * @param name The name for the new protocol
 * @param properties The properties for the new protocol
 * @param list A protocol list that the new protocol will be placed at the front of, or NULL
 * @returns The new protocols object
 */

devsdk_protocols *devsdk_protocols_new (const char *name, const iot_data_t *properties, devsdk_protocols *list);

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
