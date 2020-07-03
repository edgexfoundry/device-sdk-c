/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVSDK_H_
#define _EDGEX_DEVSDK_H_ 1

#define DEVSDKV1
#ifdef DEVSDKV2
#error Only one of edgex/devsdk.h and devsdk/devsdk.h may be included
#endif

/**
 * @file
 * @brief This file defines the functions and callbacks relating to the SDK.
 */

#include "edgex/edgex-base.h"
#include "edgex/error.h"
#include "iot/logger.h"

/**
 * @brief Structure containing startup parameters.
 */

typedef struct edgex_device_svcparams
{
  /** The name of the device service */
  const char *svcname;
  /** The configuration directory to read from */
  const char *confdir;
  /** The location of the registry service, if enabled */
  const char *regURL;
  /** The configuration profile to use */
  const char *profile;
  /** Force conf file to be read to overwrite info in consul */
  bool overwrite;
} edgex_device_svcparams;

/**
 * @brief Extracts startup parameters from command line and environment. The
 *        command-line argument array argc and argv are altered to represent
 *        whatever arguments remain unprocessed.
 * @param argc A pointer to argc as passed into main().
 * @param argv argv as passed into main().
 * @param params The startup parameters to populate.
 * @returns true if there were no errors in processing.
 */

bool edgex_device_service_processparams
  (int *argc, char **argv, edgex_device_svcparams *params);

/**
 * @brief Prints usage information.
 */

void edgex_device_service_usage (void);

/**
 * @brief Structure containing information about a device resource which is
 *        the subject of a get or set request.
 */

typedef struct edgex_device_commandrequest
{
  /** The device resource's name */
  const char *resname;
  /** Attributes of the device resource */
  const devsdk_nvpairs *attributes;
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
  struct iot_logger_t *lc,
  const devsdk_nvpairs *config
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
  const devsdk_protocols *protocols,
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
  const devsdk_protocols *protocols,
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
  devsdk_protocols *device
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
 * @brief Set the overwrite flag to force re-read of configuration.toml.
 * @param svc The service.
 * @param overwrite The boolean to set the overwrite flag.
 */

void edgex_device_service_set_overwrite (edgex_device_service *svc, bool overwrite);

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
 * @brief Stop the event service. Any automatic events will be cancelled
          and the rest api for the device service will be shut down.
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

/**
 * @brief Free the device service object and associated resources.
 * @param svc The device service.
 */

void edgex_device_service_free (edgex_device_service *svc);

// Compatibility for old enum tags

#define Int8 Edgex_Int8
#define Uint8 Edgex_Uint8
#define Int16 Edgex_Int16
#define Uint16 Edgex_Uint16
#define Int32 Edgex_Int32
#define Uint32 Edgex_Uint32
#define Int64 Edgex_Int64
#define Uint64 Edgex_Uint64
#define Float32 Edgex_Float32
#define Float64 Edgex_Float64
#define Bool Edgex_Bool
#define String Edgex_String
#define Binary Edgex_Binary

#endif
