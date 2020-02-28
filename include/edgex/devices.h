/*
 * Copyright (c) 2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICES_H
#define _EDGEX_DEVICES_H 1

/**
 * @file
 * @brief This file defines the functions for device management provided by the SDK.
 */

#include "devsdk/devsdk.h"
#include "edgex/edgex.h"

/**
 * @brief Add a device to the EdgeX system. This will generally be called in
 *        response to a request for device discovery.
 * @param svc The device service.
 * @param name The name of the new device.
 * @param description Optional description of the new device.
 * @param labels Optional labels for the new device.
 * @param profile_name Name of the device profile to be used with this device.
 * @param protocols Location of the device specified by one or more protocols.
 * @param autos Automatic Events which are to be generated from the device.
 * @param err Nonzero reason codes will be set here in the event of errors.
 * @returns The id of the newly created or existing device, or NULL if an error occurred.
 */

#ifdef __cplusplus
extern "C" {
#endif

char * edgex_add_device
(
  devsdk_service_t *svc,
  const char *name,
  const char *description,
  const devsdk_strings *labels,
  const char *profile_name,
  devsdk_protocols *protocols,
  edgex_device_autoevents *autos,
  devsdk_error *err
);

/**
 * @brief Remove a device from EdgeX. The device will be deleted from the device service and from core-metadata.
 * @param svc The device service.
 * @param id The id of the device to be removed.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void edgex_remove_device (devsdk_service_t *svc, const char *id, devsdk_error *err);

/**
 * @brief Remove a device from EdgeX. The device will be deleted from the device service and from core-metadata.
 * @param svc The device service.
 * @param name The name of the device to be removed.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void edgex_remove_device_byname (devsdk_service_t *svc, const char *name, devsdk_error *err);

/**
 * @brief Update a device's details.
 * @param svc The device service.
 * @param id The id of the device to update. If this is unset, the device will be located by name.
 * @param name If id is unset, this parameter must be set to the name of the device to be updated. Otherwise, it is
 *             optional and if set specifies a new name for the device.
 * @param description If set, a new description for the device.
 * @param labels If set, a new set of labels for the device.
 * @param profilename If set, a new device profile for the device.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void edgex_update_device
(
  devsdk_service_t *svc,
  const char *id,
  const char *name,
  const char *description,
  const devsdk_strings *labels,
  const char *profilename,
  devsdk_error *err
);

/**
 * @brief Obtain a list of devices known to the system.
 * @param svc The device service.
 */

edgex_device * edgex_devices (devsdk_service_t *svc);

/**
 * @brief Retrieve device information.
 * @param svc The device service.
 * @param id The device id.
 * @returns The requested device metadata or null if the device was not found.
 */

edgex_device * edgex_get_device (devsdk_service_t *svc, const char *id);

/**
 * @brief Retrieve device information.
 * @param svc The device service.
 * @param name The device name.
 * @returns The requested device metadata or null if the device was not found.
 */

edgex_device * edgex_get_device_byname (devsdk_service_t *svc, const char *name);

/**
 * @brief Free a device structure or list of device structures.
 * @param d The device or the first device in the list.
 */

void edgex_free_device (edgex_device *d);

#ifdef __cplusplus
}
#endif

#endif
