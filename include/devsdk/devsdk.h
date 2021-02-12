/*
 * Copyright (c) 2020
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
 * @brief Function called when configuration is updated.
 * @param impl The context data passed in when the service was created.
 * @param config A string map containing the new configuration.
 */

typedef void (*devsdk_reconfigure) (void *impl, const iot_data_t *config);

/**
 * @brief Optional callback for dynamic discovery of devices. The implementation should detect devices and register them using
 *        the devsdk_add_device API call.
 * @param impl The context data passed in when the service was created.
 */

typedef void (*devsdk_discover) (void *impl);

/**
 * @brief Callback issued to handle GET requests for device readings.
 * @param impl The context data passed in when the service was created.
 * @param devname The name of the device to be queried.
 * @param protocols The location of the device to be queried.
 * @param nreadings The number of readings requested.
 * @param requests An array specifying the readings that have been requested.
 * @param readings An array in which to return the requested readings.
 * @param qparams Query Parameters which were set for this request.
 * @param exception Set this to an IOT_DATA_STRING to give more information if the operation fails.
 * @return true if the operation was successful, false otherwise.
 */

typedef bool (*devsdk_handle_get)
(
  void *impl,
  const char *devname,
  const devsdk_protocols *protocols,
  uint32_t nreadings,
  const devsdk_commandrequest *requests,
  devsdk_commandresult *readings,
  const devsdk_nvpairs *qparams,
  iot_data_t **exception
);

/**
 * @brief Callback issued to handle PUT requests for setting device values.
 * @param impl The context data passed in when the service was created.
 * @param devname The name of the device to be queried.
 * @param protocols The location of the device to be queried.
 * @param nvalues The number of set operations requested.
 * @param requests An array specifying the resources to which to write.
 * @param values An array specifying the values to be written.
 * @param qparams Query Parameters which were set for this request.
 * @param exception Set this to an IOT_DATA_STRING to give more information if the operation fails.
 * @return true if the operation was successful, false otherwise.
 */

typedef bool (*devsdk_handle_put)
(
  void *impl,
  const char *devname,
  const devsdk_protocols *protocols,
  uint32_t nvalues,
  const devsdk_commandrequest *requests,
  const iot_data_t *values[],
  const devsdk_nvpairs *qparams,
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

typedef struct devsdk_callbacks
{
  devsdk_initialize init;
  devsdk_reconfigure reconfigure;
  devsdk_discover discover;                     /* NULL for no discovery */
  devsdk_handle_get gethandler;
  devsdk_handle_put puthandler;
  devsdk_stop stop;
  devsdk_add_device_callback device_added;      /* May be NULL */
  devsdk_update_device_callback device_updated; /* May be NULL */
  devsdk_remove_device_callback device_removed; /* May be NULL */
  devsdk_autoevent_start_handler ae_starter;    /* NULL for SDK-managed autoevents */
  devsdk_autoevent_stop_handler ae_stopper;     /* NULL for SDK-managed autoevents */
} devsdk_callbacks;


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
  (const char *defaultname, const char *version, void *impldata, devsdk_callbacks implfns, int *argc, char **argv, devsdk_error *err);

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
 * @brief Obtain a device known to the system, by name.
 * @param svc The device service.
 * @param name The name of the device to retrieve.
 */

devsdk_devices *devsdk_get_device (devsdk_service_t *svc, const char *name);

/**
 * @brief Free a device structure or list of device structures.
 * @param d The device list.
 */

void devsdk_free_devices (devsdk_devices *d);

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

#ifdef __cplusplus
}
#endif

#endif
