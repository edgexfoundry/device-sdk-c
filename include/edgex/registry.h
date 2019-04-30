/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_REGISTRY_H_
#define _EDGEX_REGISTRY_H_ 1

/**
 * @file
 * @brief This file defines the interface relating to Edgex
 *        pluggable registries.
 */

#include "edgex/edgex-base.h"
#include "edgex/error.h"
#include "edgex/edgex_logging.h"

/* Registry implementation functions */

/**
 * @brief Determine whether the registry service is running.
 * @param lc A logging client to use.
 * @param location The address of the registry service.
 * @param err Nonzero reason codes may be set here in the event of errors.
 * @returns true if the registry service is running.
 */

typedef bool (*edgex_registry_ping_impl)
  (iot_logger_t *lc, void *location, edgex_error *err);

/**
 * @brief Retrieve configuration values from the registry.
 * @param lc A logging client to use.
 * @param location The address of the registry service.
 * @param servicename The name of this device service.
 * @param profile The name of the configuration profile (may be NULL).
 * @param err Nonzero reason codes may be set here in the event of errors.
 * @returns Configuration retrieved for the named service.
 */

typedef edgex_nvpairs *(*edgex_registry_get_config_impl)
(
  iot_logger_t *lc,
  void *location,
  const char *servicename,
  const char *profile,
  edgex_error *err
);

/**
 * @brief Write configuration values to the registry.
 * @param lc A logging client to use.
 * @param location The address of the registry service.
 * @param servicename The name of this device service.
 * @param profile The name of the configuration profile (may be NULL).
 * @param config The configuration values to write.
 * @param err Nonzero reason codes may be set here in the event of errors.
 */

typedef void (*edgex_registry_put_config_impl)
(
  iot_logger_t *lc,
  void *location,
  const char *servicename,
  const char *profile,
  const edgex_nvpairs *config,
  edgex_error *err
);

/**
 * @brief Register the current service in the registry.
 * @param lc A logging client to use.
 * @param location The address of the registry service.
 * @param servicename The name of this service.
 * @param hostname The host on which this service is running.
 * @param port The port on which this service may be contacted.
 * @param checkInterval How often the registry should check that this service
 *        is running.
 * @param err Nonzero reason codes may be set here in the event of errors.
 */

typedef void (*edgex_registry_register_service_impl)
(
  iot_logger_t *lc,
  void *location,
  const char *servicename,
  const char *hostname,
  uint16_t port,
  const char *checkInterval,
  edgex_error *err
);

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

typedef void (*edgex_registry_query_service_impl)
(
  iot_logger_t *lc,
  void *location,
  const char *servicename,
  char **hostname,
  uint16_t *port,
  edgex_error *err
);

/**
 * @brief Parse a location string.
 * @param lc A logging client to use.
 * @param location Part of a URL following the "://" separator.
 * @returns A registry-specific structure holding the parsed address for use in
 *          future requests.
 */

typedef void *(*edgex_registry_parse_location_impl)
  (iot_logger_t *lc, const char *location);

/**
 * @brief Free the memory used in a location structure.
 * @param location The location structure to free.
 */

typedef void (*edgex_registry_free_location_impl) (void *location);

typedef struct edgex_registry_impl
{
  edgex_registry_ping_impl ping;
  edgex_registry_get_config_impl get_config;
  edgex_registry_put_config_impl put_config;
  edgex_registry_register_service_impl register_service;
  edgex_registry_query_service_impl query_service;
  edgex_registry_parse_location_impl parser;
  edgex_registry_free_location_impl free_location;
} edgex_registry_impl;

/* Implementation for registries which are addressed as name://host:port */

typedef struct
{
  char *host;
  uint16_t port;
} edgex_registry_hostport;

void *edgex_registry_parse_simple_url (iot_logger_t *lc, const char *url);
void edgex_registry_free_simple_url (void *location);

/* Client functions */

struct edgex_registry;
typedef struct edgex_registry edgex_registry;

/**
 * @brief Plug in a registry implementation.
 * @param name The name of the registry implementation. This will be matched
 *             against the protocol in requested registry URLs.
 * @param impl A structure containing the functions which implement the
               registry client operations.
 * @return true if the operation succeeded, false if an implementation with
 *         this name already exists.
 */

bool edgex_registry_add_impl (const char *name, edgex_registry_impl impl);

/**
 * @brief Obtain a registry instance.
 * @param lc A logging client.
 * @param url A URL specifying the registry service to connect to.
 * @returns A registry instance, or NULL if the URL could not be parsed.
 *          The caller is responsible for disposing of this pointer using
 *          edgex_registry_free().
 */

edgex_registry *edgex_registry_get_registry
  (struct iot_logger_t *lc, const char *url);

/**
 * @brief Determine whether the registry service is running.
 * @param registry The registry instance.
 * @param err Nonzero reason codes will be set here in the event of errors.
 * @returns true if the registry service is running.
 */

bool edgex_registry_ping (edgex_registry *registry, edgex_error *err);

/**
 * @brief Retrieve configuration values from the registry.
 * @param registry The registry instance.
 * @param servicename The name of this device service.
 * @param profile The name of the configuration profile (may be NULL).
 * @param err Nonzero reason codes will be set here in the event of errors.
 * @returns Configuration retrieved for the named service.
 */

edgex_nvpairs *edgex_registry_get_config
(
  edgex_registry *registry,
  const char *servicename,
  const char *profile,
  edgex_error *err
);

/**
 * @brief Write configuration values to the registry.
 * @param registry The registry instance.
 * @param servicename The name of this device service.
 * @param profile The name of the configuration profile (may be NULL).
 * @param config The configuration values to write.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void edgex_registry_put_config
(
  edgex_registry *registry,
  const char *servicename,
  const char *profile,
  const edgex_nvpairs *config,
  edgex_error *err
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

void edgex_registry_register_service
(
  edgex_registry *registry,
  const char *servicename,
  const char *hostname,
  uint16_t port,
  const char *checkInterval,
  edgex_error *err
);

/**
 * @brief Retrieve microservice endpoint from the registry.
 * @param registry The registry instance.
 * @param servicename The name of the microservice to query for.
 * @param hostname The host on which the requested service is running. This
 *                 string should be freed after use.
 * @param port The port on which the requested service may be contacted.
 * @param err Nonzero reason codes will be set here in the event of errors.
 */

void edgex_registry_query_service
(
  edgex_registry *registry,
  const char *servicename,
  char **hostname,
  uint16_t *port,
  edgex_error *err
);

/**
 * @brief Release memory used by a registry instance.
 * @param registry The registry instance.
 */

void edgex_registry_free (edgex_registry *registry);

/**
 * @brief Release memory used by the registry abstraction
 */

void edgex_registry_fini (void);

#endif
