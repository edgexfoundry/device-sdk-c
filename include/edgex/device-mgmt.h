/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_MGMT_H_
#define _EDGEX_DEVICE_MGMT_H_ 1

/**
 * @file
 * @brief This file defines the functions for device and profile management
 *        provided by the SDK.
 */

#include "edgex/devsdk.h"
#include "edgex/edgex.h"
#include "edgex/error.h"

/**
 * @brief Add a device to the EdgeX system. This will generally be called in
 *        response to a request for device discovery.
 * @param svc The device service.
 * @param name The name of the new device.
 * @param description Optional description of the new device.
 * @param labels Optional labels for the new device.
 * @param profile_name Name of the device profile to be used with this device.
 * @param address Addressable for this device. The addressable will be created
 *        in metadata. The address' name and origin timestamp will be generated
 *        if not set.
 * @param err Nonzero reason codes will be set here in the event of errors.
 * @returns The id of the newly created or existing device, or NULL if an error
 *          occurred.
 */

char * edgex_device_add_device
(
  edgex_device_service *svc,
  const char *name,
  const char *description,
  const edgex_strings *labels,
  const char *profile_name,
  edgex_protocols *protocols,
  edgex_device_autoevents *autos,
  edgex_error *err
);

/**
 * @brief Remove a device from EdgeX. The device will be deleted from the
 *        device service and from core-metadata.
 * @param svc The device service.
 * @param id The id of the device to be removed.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void edgex_device_remove_device
  (edgex_device_service *svc, const char *id, edgex_error *err);

/**
 * @brief Remove a device from EdgeX. The device will be deleted from the
 *        device service and from core-metadata.
 * @param svc The device service.
 * @param name The name of the device to be removed.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void edgex_device_remove_device_byname
  (edgex_device_service *svc, const char *name, edgex_error *err);

/**
 * @brief Update a device's details.
 * @param svc The device service.
 * @param id The id of the device to update. If this is unset, the device will
 *           be located by name.
 * @param name If id is unset, this parameter must be set to the name of the
 *             devce to be updated. Otherwise, it is optional and if set,
 *             specifies a new name for the device.
 * @param description If set, a new description for the device.
 * @param labels If set, a new set of labels for the device.
 * @param profilename If set, a new device profile for the device.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void edgex_device_update_device
(
  edgex_device_service *svc,
  const char *id,
  const char *name,
  const char *description,
  const edgex_strings *labels,
  const char *profilename,
  edgex_error *err
);

/**
 * @brief Obtain a list of devices known to the system.
 * @param svc The device service.
 */

edgex_device * edgex_device_devices (edgex_device_service *svc);

/**
 * @brief Retrieve device information.
 * @param svc The device service.
 * @param id The device id.
 * @returns The requested device metadata or null if the device was not found.
 */

edgex_device * edgex_device_get_device
  (edgex_device_service *svc, const char *id);

/**
 * @brief Retrieve device information.
 * @param svc The device service.
 * @param name The device name.
 * @returns The requested device metadata or null if the device was not found.
 */

edgex_device * edgex_device_get_device_byname
  (edgex_device_service *svc, const char *name);

/**
 * @brief Free a device structure or list of device structures.
 * @param The device or the first device in the list.
 */

void edgex_device_free_device (edgex_device *e);

/**
 * @brief Retrieve the device profiles currently known in the SDK.
 * @param svc The device service.
 * @param profiles Is set to an array of device profiles. This should be
 *        freed using edgex_deviceprofile_free_array() after use.
 * @returns the size of the returned array.
 */

uint32_t edgex_device_service_getprofiles
(
  edgex_device_service *svc,
  edgex_deviceprofile **profiles
);

/**
 * @brief Free an array of device profiles.
 * @param e The array.
 * @param count The number of elements in the array.
 */

void edgex_deviceprofile_free_array (edgex_deviceprofile *e, unsigned count);

#endif
