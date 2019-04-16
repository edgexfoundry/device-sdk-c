/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVSDK_H_
#define _EDGEX_DEVSDK_H_ 1

/**
 * @file
 * @brief This file defines the functions and callbacks relating to the SDK.
 */

#include "edgex/edgex-base.h"
#include "edgex/error.h"
#include "edgex/edgex_logging.h"

/**
 * @brief Structure containing information about a device resource which is
 *        the subject of a get or set request.
 */

typedef struct edgex_device_commandrequest
{
  /** The device resource's name */
  const char *resname;
  /** Attributes of the device resource */
  const edgex_nvpairs *attributes;
  /** Type of the data to be read or written */
  edgex_propertytype type;
} edgex_device_commandrequest;

/**
 * @brief Structure containing a parameter (for set) or a result (for get).
 */

typedef struct edgex_device_commandresult
{
  /**
   * The timestamp of the result. Should only be set if the device itself
   * supplies one.
   */
  uint64_t origin;
  /** The type of the parameter or result. */
  edgex_propertytype type;
  /** The value of the parameter or result. */
  edgex_device_resultvalue value;
} edgex_device_commandresult;

/* Callback functions */

/**
 * @brief Function called during service start operation.
 * @param impl The context data passed in when the service was created.
 * @param lc A logging client for the device service.
 * @param config Name-Value pairs in the <service-name>.driver hierarchy. NB
 *               these are represented in .toml configuration fies as a
 *               "Driver" table.
 * @return true if the operation was successful, false otherwise.
 */

typedef bool (*edgex_device_device_initialize)
(
  void *impl,
  struct iot_logging_client *lc,
  const edgex_nvpairs *config
);

/**
 * @brief Optional callback for dynamic discovery of devices. The
 *        implementation should detect devices and register them
 *        using the edgex_device_add_device API call.
 * @param impl The context data passed in when the service was created.
 */

typedef void (*edgex_device_discover)
(
  void *impl
);

/**
 * @brief Callback issued to handle GET requests for device readings.
 * @param impl The context data passed in when the service was created.
 * @param devname The name of the device to be queried.
 * @param protocols The location of the device to be queried.
 * @param nreadings The number of readings requested.
 * @param requests An array specifying the readings that have been requested.
 * @param readings An array in which to return the requested readings.
 * @return true if the operation was successful, false otherwise.
 */

typedef bool (*edgex_device_handle_get)
(
  void *impl,
  const char *devname,
  const edgex_protocols *protocols,
  uint32_t nreadings,
  const edgex_device_commandrequest *requests,
  edgex_device_commandresult *readings
);

/**
 * @brief Callback issued to handle PUT requests for setting device values.
 * @param impl The context data passed in when the service was created.
 * @param devname The name of the device to be queried.
 * @param protocols The location of the device to be queried.
 * @param nvalues The number of set operations requested.
 * @param requests An array specifying the resources to which to write.
 * @param values An array specifying the values to be written.
 * @return true if the operation was successful, false otherwise.
 */

typedef bool (*edgex_device_handle_put)
(
  void *impl,
  const char *devname,
  const edgex_protocols *protocols,
  uint32_t nvalues,
  const edgex_device_commandrequest *requests,
  const edgex_device_commandresult *values
);

/**
 * @brief Unused in the current implementation. In future this may be used to
 *        notify a device service implementation that a device has been removed
 *        and any resources relating to it may be released.
 */

typedef bool (*edgex_device_disconnect_device)
(
  void *impl,
  edgex_protocols *device
);

/**
 * @brief Callback issued during device service shutdown. The implementation
 * should stop processing and release any resources that were being used.
 * @param impl The context data passed in when the service was created.
 * @param force A 'force' stop has been requested. An unclean shutdown may be
 *              performed if necessary.
 */

typedef void (*edgex_device_stop) (void *impl, bool force);

typedef struct edgex_device_callbacks
{
  edgex_device_device_initialize init;
  edgex_device_discover discover;            /* NULL for no discovery */
  edgex_device_handle_get gethandler;
  edgex_device_handle_put puthandler;
  edgex_device_disconnect_device disconnect;
  edgex_device_stop stop;
} edgex_device_callbacks;

/* Device service */

struct edgex_device_service;
typedef struct edgex_device_service edgex_device_service;

/**
 * @brief Create a new device service.
 * @param name The device service name, used in logging, metadata lookups and
 *             and to scope configuration.
 * @param version The version string for this service. For information only.
 * @param impldata An object pointer which will be passed back whenever one of
 *                 the callback functions is invoked.
 * @param implfns Structure containing the device implementation functions. The
 *                SDK will call these functions in order to carry out its
 *                various actions.
 * @param err Nonzero reason codes will be set here in the event of errors.
 * @return The newly instantiated service
 */

edgex_device_service *edgex_device_service_new
(
  const char *name,
  const char *version,
  void *impldata,
  edgex_device_callbacks implfns,
  edgex_error *err
);

/**
 * @brief Start a device service.
 * @param svc The service to start.
 * @param registryURL If set, this identifies a registry implementation for the
 *                    service to use. The service will register itself and
                      obtain configuration from this registry. If no
 *                    configuration is available, it will be read from file and
 *                    uploaded to the registry ready for subsequent runs.
 * @param profile Configuration profile to use (may be null).
 * @param confDir Directory containing configuration files.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void edgex_device_service_start
(
  edgex_device_service *svc,
  const char *registryURL,
  const char *profile,
  const char *confDir,
  edgex_error *err
);

/**
 * @brief Post readings to the core-data service. This method allows readings
 *        to be generated other than in response to a device GET invocation.
 * @param svc The device service.
 * @param device_name The name of the device that the readings have come from.
 * @param resource_name Name of the resource or command which defines the Event.
 * @param values An array of readings. These will be combined into an Event
 *        and submitted to core-data. For readings of String or Binary type,
 *        the SDK takes ownership of the memory containing the string or
 *        byte array.
 */

void edgex_device_post_readings
(
  edgex_device_service *svc,
  const char *device_name,
  const char *resource_name,
  const edgex_device_commandresult *values
);

/**
 * @brief Stop the event service. Any locally-scheduled events will be
 *        cancelled, the rest api for the device will be shutdown, and
 *        resources will be freed.
 * @param svc The device service.
 * @param force Force stop.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void edgex_device_service_stop
(
  edgex_device_service *svc,
  bool force,
  edgex_error *err
);

#endif
