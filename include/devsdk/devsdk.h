/*
 * Copyright (c) 2020-2022
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _DEVSDK_DEVSDK_H_
#define _DEVSDK_DEVSDK_H_ 1

/**
 * @file
 * @brief This file defines the functions and callbacks relating to the SDK.
 */

#include "devsdk/devsdk-base.h"
#include "iot/logger.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Prints usage information.
 */

void devsdk_usage (void);

/* Callback functions */

/**
 * @brief Function called during service start operation.
 * @param impl The context data passed in when the service was created.
 * @param lc A logging client for the device service.
 * @param config A string map containing the configuration specified in the service's "Driver" section.
 * @return true if the operation was successful, false otherwise.
 */

typedef bool (*devsdk_initialize) (void *impl, struct iot_logger_t *lc, const iot_data_t *config);

/**
 * @brief Optional callback function called when configuration is updated.
 * @param impl The context data passed in when the service was created.
 * @param config A string map containing the new configuration.
 */

typedef void (*devsdk_reconfigure) (void *impl, const iot_data_t *config);

/**
 * @brief Optional callback for dynamic discovery of devices. The implementation should detect devices and register them using
 *        the devsdk_add_discovered_devices API call.
 * @param impl The context data passed in when the service was created.
 * @param request_id The discovery request ID
 */

typedef void (*devsdk_discover) (void *impl, const char * request_id);

/**
 * @brief Optional callback for deleting a discovery request that is in progress.
 * @param impl The context data passed in when the service was created
 * @param request_id The request ID of the discovery request to delete
 */
typedef bool (*devsdk_discovery_delete) (void *impl, const char * request_id);


/**
 * @brief Optional callback for dynamic discovery of device resources.
 * @param impl The context data passed in when the service was created.
 * @param dev The details of the device to be queried.
 * @param options Service specific discovery options map. May be NULL.
 * @param resources The operations supported by the device.
 * @param exception Set this to an IOT_DATA_STRING to give more information if the operation fails.
 * @return true if the operation was successful, false otherwise.
 */

typedef bool (*devsdk_describe)
  (void *impl, const devsdk_device_t *dev, const iot_data_t *options, devsdk_device_resources **resources, iot_data_t **exception);

/**
 * @brief Callback issued for parsing device addresses.
 * @param impl The context data passed in when the service was created.
 * @param protocols The protocol properties for the device.
 * @param exception Set this to an IOT_DATA_STRING to give more information if the operation fails.
 * @return A new object representing the device address in parsed form.
 */

typedef devsdk_address_t (*devsdk_create_address) (void *impl, const devsdk_protocols *protocols, iot_data_t **exception);

/**
 * @brief Callback for freeing memory allocated by devsdk_create_address.
 * @param impl The context data passed in when the service was created.
 * @param address The object to be freed.
 */

typedef void (*devsdk_free_address) (void *impl, devsdk_address_t address);

/**
 * @brief Callback issued for validating device addresses.
 * @param impl The context data passed in when the service was created.
 * @param protocols The protocol properties for the device.
 * @param exception Set this to an IOT_DATA_STRING to give details if the protocol properties are invalid
 */

typedef void (*devsdk_validate_address) (void *impl, const devsdk_protocols *protocols, iot_data_t **exception);

/**
 * @brief Callback issued for parsing device resource attributes. 
 * @param impl The context data passed in when the service was created.
 * @param protocols The attributes of the device resource.
 * @param exception Set this to an IOT_DATA_STRING to give more information if the operation fails.
 * @return A new object representing the attributes in parsed form.
 */

typedef devsdk_resource_attr_t (*devsdk_create_resource_attr) (void *impl, const iot_data_t *attributes, iot_data_t **exception);

/** 
 * @brief Callback for freeing memory allocated by devsdk_create_resource_attr.
 * @param impl The context data passed in when the service was created.
 * @param address The object to be freed.
 */

typedef void (*devsdk_free_resource_attr) (void *impl, devsdk_resource_attr_t resource);

/**
 * @brief Callback issued to handle GET requests for device readings.
 * @param impl The context data passed in when the service was created.
 * @param device The details of the device to be queried.
 * @param nreadings The number of readings requested.
 * @param requests An array specifying the readings that have been requested.
 * @param readings An array in which to return the requested readings.
 * @param options Options which were set for this request.
 * @param exception Set this to an IOT_DATA_STRING to give more information if the operation fails.
 * @return true if the operation was successful, false otherwise.
 */

typedef bool (*devsdk_handle_get)
(
  void *impl,
  const devsdk_device_t *device,
  uint32_t nreadings,
  const devsdk_commandrequest *requests,
  devsdk_commandresult *readings,
  const iot_data_t *options,
  iot_data_t **exception
);

/**
 * @brief Callback issued to handle PUT requests for setting device values.
 * @param impl The context data passed in when the service was created.
 * @param device The details of the device to be queried.
 * @param nvalues The number of set operations requested.
 * @param requests An array specifying the resources to which to write.
 * @param values An array specifying the values to be written.
 * @param options Options which were set for this request.
 * @param exception Set this to an IOT_DATA_STRING to give more information if the operation fails.
 * @return true if the operation was successful, false otherwise.
 */

typedef bool (*devsdk_handle_put)
(
  void *impl,
  const devsdk_device_t *device,
  uint32_t nvalues,
  const devsdk_commandrequest *requests,
  const iot_data_t *values[],
  const iot_data_t *options,
  iot_data_t **exception
);

/**
 * @brief Callback issued during device service shutdown. The implementation should stop processing and release any resources that were being used.
 * @param impl The context data passed in when the service was created.
 * @param force A 'force' stop has been requested. An unclean shutdown may be performed if necessary.
 */

typedef void (*devsdk_stop) (void *impl, bool force);

/**
 * @brief Callback function requesting that automatic events should begin. These should be generated according to the schedule given,
 *        and posted using devsdk_post_readings().
 * @param impl The context data passed in when the service was created.
 * @param devname The name of the device to be queried.
 * @param protocols The location of the device to be queried.
 * @param resource_name The resource on which autoevents have been configured.
 * @param nreadings The number of readings requested.
 * @param requests An array specifying the readings that have been requested.
 * @param interval The time between events, in milliseconds.
 * @param onChange If true, events should only be generated if one or more readings have changed.
 * @return A pointer to a data structure that will be provided in a subsequent call to the stop handler.
 */

typedef void * (*devsdk_autoevent_start_handler)
(
  void *impl,
  const char *devname,
  const devsdk_protocols *protocols,
  const char *resource_name,
  uint32_t nreadings,
  const devsdk_commandrequest *requests,
  uint64_t interval,
  bool onChange
);

/**
 * @brief Callback function requesting that automatic events should cease.
 * @param impl The context data passed in when the service was created.
 * @param handle The data structure returned by a previous call to the start handler.
 */

typedef void (*devsdk_autoevent_stop_handler) (void *impl, void *handle);

/**
 * @brief Callback function indicating that a new device has been added.
 * @param impl The context data passed in when the service was created.
 * @param devname The name of the new device.
 * @param protocols The protocol properties that comprise the device's address.
 * @param resources The operations supported by the device.
 * @param adminEnabled Whether the device is administratively enabled.
 */

typedef void (*devsdk_add_device_callback) (void *impl, const char *devname, const devsdk_protocols *protocols, const devsdk_device_resources *resources, bool adminEnabled);

/**
 * @brief Callback function indicating that a device's address or adminstate has been updated.
 * @param impl The context data passed in when the service was created.
 * @param devname The name of the updated device.
 * @param protocols The protocol properties that comprise the device's address.
 * @param state The device's current adminstate.
 */

typedef void (*devsdk_update_device_callback) (void *impl, const char *devname, const devsdk_protocols *protocols, bool adminEnabled);

/**
 * @brief Callback function indicating that a device has been removed.
 * @param impl The context data passed in when the service was created.
 * @param devname The name of the removed device.
 * @param protocols The protocol properties that comprise the device's address.
 */

typedef void (*devsdk_remove_device_callback) (void *impl, const char *devname, const devsdk_protocols *protocols);

typedef struct devsdk_callbacks devsdk_callbacks;

/**
 * @brief Allocate a callbacks structure with required functions. Other function pointers are initialized to null
 */

devsdk_callbacks *devsdk_callbacks_init
(
  devsdk_initialize init,
  devsdk_handle_get gethandler,
  devsdk_handle_put puthandler,
  devsdk_stop stop,
  devsdk_create_address create_addr,
  devsdk_free_address free_addr,
  devsdk_create_resource_attr create_res,
  devsdk_free_resource_attr free_res
);

/**
 * @brief Populate optional discovery functions
 */

void devsdk_callbacks_set_discovery (devsdk_callbacks *cb, devsdk_discover discover, devsdk_describe describe);

/**
 * @brief Populate optional discovery delete function
 */

void devsdk_callbacks_set_discovery_delete (devsdk_callbacks *cb, devsdk_discovery_delete discovery_delete, devsdk_describe describe);

/**
 * @brief Populate optional reconfiguration function
 */

void devsdk_callbacks_set_reconfiguration (devsdk_callbacks *cb, devsdk_reconfigure reconf);

/**
 * @brief Populate optional device notification functions
 */

void devsdk_callbacks_set_listeners
  (devsdk_callbacks *cb, devsdk_add_device_callback device_added, devsdk_update_device_callback device_updated, devsdk_remove_device_callback device_removed);

/**
 * @brief Populate optional autoevent management functions
 */

void devsdk_callbacks_set_autoevent_handlers (devsdk_callbacks *cb, devsdk_autoevent_start_handler ae_starter, devsdk_autoevent_stop_handler ae_stopper);

/**
 * @brief Populate optional device address validation function
 */

extern void devsdk_callbacks_set_validate_addr (devsdk_callbacks *cb, devsdk_validate_address validate_addr);

/**
 * @brief Create a new device service.
 * @param defaultname The device service name, used in logging, metadata lookups and to scope configuration. This may be overridden via the commandline.
 * @param version The version string for this service. For information only.
 * @param impldata An object pointer which will be passed back whenever one of the callback functions is invoked.
 * @param implfns Structure containing the device implementation functions. The SDK will call these functions in order to carry out its various actions.
 * @param argc A pointer to argc as passed into main(). This will be adjusted to account for arguments consumed by the SDK.
 * @param argv argv as passed into main(). This will be adjusted to account for arguments consumed by the SDK.
 * @param err Nonzero reason codes will be set here in the event of errors.
 * @return The newly instantiated service
 */

devsdk_service_t *devsdk_service_new
  (const char *defaultname, const char *version, void *impldata, devsdk_callbacks *implfns, int *argc, char **argv, devsdk_error *err);

/**
 * @brief Start a device service.
 * @param svc The service to start.
 * @param driverdfls The default implementation-specific configuration.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void devsdk_service_start (devsdk_service_t *svc, iot_data_t *driverdfls, devsdk_error *err);

/**
 * @brief Post readings to the core-data service. This method allows readings to be generated other than in response to a device GET invocation.
 * @param svc The device service.
 * @param device_name The name of the device that the readings have come from.
 * @param resource_name Name of the resource or command which defines the Event.
 * @param values An array of readings. These will be combined into an Event and submitted to core-data.
 */

void devsdk_post_readings (devsdk_service_t *svc, const char *device_name, const char *resource_name, devsdk_commandresult *values);

void devsdk_add_discovered_devices (devsdk_service_t *svc, uint32_t ndevices, devsdk_discovered_device *devices);

/**
 * @brief Obtain a list of devices known to the system.
 * @param svc The device service.
 */

devsdk_devices *devsdk_get_devices (devsdk_service_t *svc);

/**
 * @brief Obtain secret credentials.
 * @param svc The device service.
 * @param name The path to search for secrets.
 */

iot_data_t *devsdk_get_secrets (devsdk_service_t *svc, const char *path);

/**
 * @brief Obtain a device known to the system, by name.
 * @param svc The device service.
 * @param name The name of the device to retrieve.
 */

devsdk_devices *devsdk_get_device (devsdk_service_t *svc, const char *name);

/**
 * @brief Free a device structure or list of device structures.
 * @param svc The device service.
 * @param d The device list.
 */

void devsdk_free_devices (devsdk_service_t *svc, devsdk_devices *d);

/**
 * @brief Set the operational state of a device
 * @param svc The device service.
 * @param devname The device name.
 * @param operational true if the device is operational (enabled)
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void devsdk_set_device_opstate (devsdk_service_t *svc, char *devname, bool operational, devsdk_error *err);


/**
 * @brief Stop the event service. Any automatic events will be cancelled and the rest api for the device service will be shut down.
 * @param svc The device service.
 * @param force Force stop.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void devsdk_service_stop (devsdk_service_t *svc, bool force, devsdk_error *err);

/**
 * @brief Free the device service object and associated resources.
 * @param svc The device service.
 */

void devsdk_service_free (devsdk_service_t *svc);

 /**
  * Publish a discovery event
  * @param svc The device service
  * @param request_id The discovery request ID
  * @param progress Progress number between 0 and 100. -1 for error.
  * @param discovered_devices The number of discovered devices
  */
extern void devsdk_publish_discovery_event (devsdk_service_t *svc, const char * request_id, const int8_t progress, const uint64_t discovered_devices);

/**
 * Publish a system event
 * @param svc The device services
 * @param action The action being published
 * @param details Parameters to be published
 */
extern void devsdk_publish_system_event (devsdk_service_t *svc, const char *action, iot_data_t * details);

#ifdef __cplusplus
}
#endif

#endif
