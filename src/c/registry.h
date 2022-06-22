/*
 * Copyright (c) 2020-2022
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _DEVSDK_REGISTRY_H_
#define _DEVSDK_REGISTRY_H_ 1

#include "devsdk/devsdk-base.h"
#include "edgex/edgex-base.h"
#include "devutil.h"
#include "iot/threadpool.h"

/* Callback for dynamic configuration */

/**
 * @brief A function which is called when the configuration changes.
 * @param updatectx A context specified when this function was registered.
 * @param newconfig The updated configuration.
 */

typedef void (*devsdk_registry_updatefn) (void *updatectx, const devsdk_nvpairs *newconfig);

typedef struct devsdk_registry_t devsdk_registry_t;

devsdk_registry_t *devsdk_registry_get_consul (void);

bool devsdk_registry_init (devsdk_registry_t *registry, iot_logger_t *lc, iot_threadpool_t *thpool, const char *url);

bool devsdk_registry_waitfor (devsdk_registry_t *registry, const devsdk_timeout *deadline);

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
  devsdk_registry_t *registry,
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

void devsdk_registry_put_config (devsdk_registry_t *registry, const char *servicename, const iot_data_t *config, devsdk_error *err);

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
  devsdk_registry_t *registry,
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

void devsdk_registry_deregister_service (devsdk_registry_t *registry, const char *servicename, devsdk_error *err);

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
  devsdk_registry_t *registry,
  const char *servicename,
  char **hostname,
  uint16_t *port,
  const devsdk_timeout *timeout,
  devsdk_error *err
);

/**
 * @brief Release memory used by a registry instance.
 * @param registry The registry instance.
 */

void devsdk_registry_free (devsdk_registry_t *registry);

#endif
