/*
 * Copyright (c) 2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _DEVSDK_REGISTRY_H_
#define _DEVSDK_REGISTRY_H_ 1

/**
 * @file
 * @brief This file defines the interface relating to pluggable configuration registries.
 */

#include "devsdk/devsdk-base.h"
#include "iot/threadpool.h"

/* Callback for dynamic configuration */

/**
 * @brief A function which is called when the configuration changes.
 * @param updatectx A context specified when this function was registered.
 * @param newconfig The updated configuration.
 */

typedef void (*devsdk_registry_updatefn) (void *updatectx, const devsdk_nvpairs *newconfig);

/* Registry implementation functions */

/**
 * @brief Determine whether the registry service is running.
 * @param lc A logging client to use.
 * @param location The address of the registry service.
 * @param err Nonzero reason codes may be set here in the event of errors.
 * @returns true if the registry service is running.
 */

typedef bool (*devsdk_registry_ping_impl)
  (iot_logger_t *lc, void *location, devsdk_error *err);

/**
 * @brief Retrieve configuration values from the registry.
 * @param lc A logging client to use.
 * @param thpool A threadpool to use.
 * @param location The address of the registry service.
 * @param servicename The name of this device service.
 * @param updater A function to be called when the configuration in the registry
 *                is updated (may be NULL).
 * @param updatectx A parameter to be passed to the updater function.
 * @param updatedone Stop checking for updates when this becomes set to true.
 * @param err Nonzero reason codes may be set here in the event of errors.
 * @returns Configuration retrieved for the named service.
 */

typedef devsdk_nvpairs *(*devsdk_registry_get_config_impl)
(
  iot_logger_t *lc,
  iot_threadpool_t *thpool,
  void *location,
  const char *servicename,
  devsdk_registry_updatefn updater,
  void *updatectx,
  atomic_bool *updatedone,
  devsdk_error *err
);

/**
 * @brief Write configuration values to the registry.
 * @param lc A logging client to use.
 * @param location The address of the registry service.
 * @param servicename The name of this device service.
 * @param config The configuration values to write.
 * @param err Nonzero reason codes may be set here in the event of errors.
 */

typedef void (*devsdk_registry_put_config_impl)
(
  iot_logger_t *lc,
  void *location,
  const char *servicename,
  const iot_data_t *config,
  devsdk_error *err
);

/**
 * @brief Register the current service in the registry.
 * @param lc A logging client to use.
 * @param location The address of the registry service.
 * @param servicename The name of this service.
 * @param hostname The host on which this service is running.
 * @param port The port on which this service may be contacted.
 * @param checkInterval How often the registry should check that this service
 *        is running. Specify an empty string to indicate no checking.
 * @param err Nonzero reason codes may be set here in the event of errors.
 */

typedef void (*devsdk_registry_register_service_impl)
(
  iot_logger_t *lc,
  void *location,
  const char *servicename,
  const char *hostname,
  uint16_t port,
  const char *checkInterval,
  devsdk_error *err
);

/**
 * @brief Deregister the current service from the registry.
 * @param lc A logging client to use.
 * @param location The address of the registry service.
 * @param servicename The name of this service.
 * @param err Nonzero reason codes may be set here in the event of errors.
 */

typedef void (*devsdk_registry_deregister_service_impl)
  (iot_logger_t *lc, void *location, const char *servicename, devsdk_error *err);

/**
 * @brief Retrieve microservice endpoint from the registry.
 * @param lc A logging client to use.
 * @param location The address of the registry service.
 * @param servicename The name of the microservice to query for.
 * @param hostname The host on which the requested service is running. This
 *                 string should be freed after use.
 * @param port The port on which the requested service may be contacted.
 * @param err Nonzero reason codes may be set here in the event of errors.
 */

typedef void (*devsdk_registry_query_service_impl)
(
  iot_logger_t *lc,
  void *location,
  const char *servicename,
  char **hostname,
  uint16_t *port,
  devsdk_error *err
);

/**
 * @brief Parse a location string.
 * @param lc A logging client to use.
 * @param location Part of a URL following the "://" separator.
 * @returns A registry-specific structure holding the parsed address for use in
 *          future requests.
 */

typedef void *(*devsdk_registry_parse_location_impl)
  (iot_logger_t *lc, const char *location);

/**
 * @brief Free the memory used in a location structure.
 * @param location The location structure to free.
 */

typedef void (*devsdk_registry_free_location_impl) (void *location);

typedef struct devsdk_registry_impl
{
  devsdk_registry_ping_impl ping;
  devsdk_registry_get_config_impl get_config;
  devsdk_registry_put_config_impl put_config;
  devsdk_registry_register_service_impl register_service;
  devsdk_registry_deregister_service_impl deregister_service;
  devsdk_registry_query_service_impl query_service;
  devsdk_registry_parse_location_impl parser;
  devsdk_registry_free_location_impl free_location;
} devsdk_registry_impl;

/* Implementation for registries which are addressed as name://host:port */

typedef struct
{
  char *host;
  uint16_t port;
} devsdk_registry_hostport;

void *devsdk_registry_parse_simple_url (iot_logger_t *lc, const char *url);
void devsdk_registry_free_simple_url (void *location);

/* Client functions */

struct devsdk_registry;
typedef struct devsdk_registry devsdk_registry;

/**
 * @brief Plug in a registry implementation.
 * @param name The name of the registry implementation. This will be matched
 *             against the protocol in requested registry URLs.
 * @param impl A structure containing the functions which implement the
               registry client operations.
 * @return true if the operation succeeded, false if an implementation with
 *         this name already exists.
 */

bool devsdk_registry_add_impl (const char *name, devsdk_registry_impl impl);

/**
 * @brief Obtain a registry instance.
 * @param lc A logging client.
 * @param thpool A threadpool.
 * @param url A URL specifying the registry service to connect to.
 * @returns A registry instance, or NULL if the URL could not be parsed.
 *          The caller is responsible for disposing of this pointer using
 *          devsdk_registry_free().
 */

devsdk_registry *devsdk_registry_get_registry
  (iot_logger_t *lc, iot_threadpool_t *thpool, const char *url);

/**
 * @brief Determine whether the registry service is running.
 * @param registry The registry instance.
 * @param err Nonzero reason codes will be set here in the event of errors.
 * @returns true if the registry service is running.
 */

bool devsdk_registry_ping (devsdk_registry *registry, devsdk_error *err);

/**
 * @brief Wait for the registry service to be running. The time to wait is
 *        controlled by the EDGEX_STARTUP_INTERVAL and EDGEX_STARTUP_DURATION
 *        environment variables.
 * @param registry The registry instance.
 * @returns true if the registry service is running after waiting.
 */

bool devsdk_registry_waitfor (devsdk_registry *registry);

/**
 * @brief Retrieve configuration values from the registry.
 * @param registry The registry instance.
 * @param servicename The name of this device service.
 * @param updater A function to be called when the configuration in the registry
 *                is updated (may be NULL).
 * @param updatectx A parameter to be passed to the updater function.
 * @param updatedone The registry will stop checking for updates when this flag is set.
 * @param err Nonzero reason codes will be set here in the event of errors.
 * @returns Configuration retrieved for the named service.
 */

devsdk_nvpairs *devsdk_registry_get_config
(
  devsdk_registry *registry,
  const char *servicename,
  devsdk_registry_updatefn updater,
  void *updatectx,
  atomic_bool *updatedone,
  devsdk_error *err
);

/**
 * @brief Write configuration values to the registry.
 * @param registry The registry instance.
 * @param servicename The name of this device service.
 * @param config The configuration values to write.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void devsdk_registry_put_config
(
  devsdk_registry *registry,
  const char *servicename,
  const iot_data_t *config,
  devsdk_error *err
);

/**
 * @brief Register the current service in the registry.
 * @param registry The registry instance.
 * @param servicename The name of this service.
 * @param hostname The host on which this service is running.
 * @param port The port on which this service may be contacted.
 * @param checkInterval How often the registry should check that this service
 *        is running. The format of this string is specific to the registry
 *        implementation.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void devsdk_registry_register_service
(
  devsdk_registry *registry,
  const char *servicename,
  const char *hostname,
  uint16_t port,
  const char *checkInterval,
  devsdk_error *err
);

/**
 * @brief Deregister the current service from the registry.
 * @param registry The registry instance.
 * @param servicename The name of this service.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void devsdk_registry_deregister_service (devsdk_registry *registry, const char *servicename, devsdk_error *err);

/**
 * @brief Retrieve microservice endpoint from the registry.
 * @param registry The registry instance.
 * @param servicename The name of the microservice to query for.
 * @param hostname The host on which the requested service is running. This
 *                 string should be freed after use.
 * @param port The port on which the requested service may be contacted.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void devsdk_registry_query_service
(
  devsdk_registry *registry,
  const char *servicename,
  char **hostname,
  uint16_t *port,
  devsdk_error *err
);

/**
 * @brief Release memory used by a registry instance.
 * @param registry The registry instance.
 */

void devsdk_registry_free (devsdk_registry *registry);

/**
 * @brief Release memory used by the registry abstraction
 */

void devsdk_registry_fini (void);

#endif
