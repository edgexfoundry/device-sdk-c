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
 * @brief This file defines the functions for device and profile management provided by the SDK.
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
 * @param protocols Location of the device specified by one or more protocols.
 * @param autos Automatic Events which are to be generated from the device.
 * @param err Nonzero reason codes will be set here in the event of errors.
 * @returns The id of the newly created or existing device, or NULL if an error occurred.
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
 * @brief Remove a device from EdgeX. The device will be deleted from the device service and from core-metadata.
 * @param svc The device service.
 * @param id The id of the device to be removed.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void edgex_device_remove_device (edgex_device_service *svc, const char *id, edgex_error *err);

/**
 * @brief Remove a device from EdgeX. The device will be deleted from the device service and from core-metadata.
 * @param svc The device service.
 * @param name The name of the device to be removed.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void edgex_device_remove_device_byname (edgex_device_service *svc, const char *name, edgex_error *err);

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

edgex_device * edgex_device_get_device (edgex_device_service *svc, const char *id);

/**
 * @brief Retrieve device information.
 * @param svc The device service.
 * @param name The device name.
 * @returns The requested device metadata or null if the device was not found.
 */

edgex_device * edgex_device_get_device_byname (edgex_device_service *svc, const char *name);

/**
 * @brief Free a device structure or list of device structures.
 * @param d The device or the first device in the list.
 */

void edgex_device_free_device (edgex_device *d);

/**
 * @brief Retrieve the device profiles currently known in the SDK.
 * @param svc The device service.
 * @returns A list of device profiles.
 */

edgex_deviceprofile *edgex_device_profiles (edgex_device_service *svc);

/**
 * @brief Retrieve device profile.
 * @param svc The device service.
 * @param name The device profile name.
 * @returns The requested metadata or null if the profile was not found.
 */

edgex_deviceprofile *edgex_device_get_deviceprofile_byname (edgex_device_service *svc, const char *name);

/**
 * @brief Free a device profile or list of device profiles.
 * @param p The device profile or the first device profile in the list.
 */

void edgex_device_free_deviceprofile (edgex_deviceprofile *p);

/**
 * @brief Install a device profile
 * @param svc The device service.
 * @param fname The name of a yaml file containing the profile definition.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void edgex_device_add_profile (edgex_device_service *svc, const char *fname, edgex_error *err);

/*
 * Callbacks for device addition, updates and removal
 */

/**
 * @brief Callback function indicating that a new device has been added.
 * @param impl The context data passed in when the service was created.
 * @param devname The name of the new device.
 * @param protocols The protocol properties that comprise the device's address.
 * @param state The device's initial adminstate.
 */

typedef void (*edgex_device_add_device_callback)
  (void *impl, const char *devname, const edgex_protocols *protocols, edgex_device_adminstate state);

/**
 * @brief Callback function indicating that a device's address or adminstate has been updated.
 * @param impl The context data passed in when the service was created.
 * @param devname The name of the updated device.
 * @param protocols The protocol properties that comprise the device's address.
 * @param state The device's current adminstate.
 */

typedef void (*edgex_device_update_device_callback)
  (void *impl, const char *devname, const edgex_protocols *protocols, edgex_device_adminstate state);

/**
 * @brief Callback function indicating that a device has been removed.
 * @param impl The context data passed in when the service was created.
 * @param devname The name of the removed device.
 * @param protocols The protocol properties that comprise the device's address.
 */

typedef void (*edgex_device_remove_device_callback)
  (void *impl, const char *devname, const edgex_protocols *protocols);

/**
 * @brief This function allows a device service implementation to register for updates to the
          devices it is associated with. The registered functions will be called when a device is
          added, removed, or modified. Any callback function may be left as null if not required.
 * @param svc The device service.
 * @param add_device Function to be called when a device is added to core-metadata.
 * @param update_device Function to be called when a device is updated in core-metadata.
 * @param remove_device Function to be called when a device is removed from core-metadata.
 */

void edgex_device_register_devicelist_callbacks
(
  edgex_device_service *svc,
  edgex_device_add_device_callback add_device,
  edgex_device_update_device_callback update_device,
  edgex_device_remove_device_callback remove_device
);

#endif
