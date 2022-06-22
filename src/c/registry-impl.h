/*
 * Copyright (c) 2020-2022
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _DEVSDK_REGISTRY_IMPL_H_
#define _DEVSDK_REGISTRY_IMPL_H_ 1

#include "registry.h"

/* Registry implementation functions */

typedef bool (*devsdk_registry_init_impl) (void *impl, iot_logger_t *logger, iot_threadpool_t *pool, const char *url);

typedef bool (*devsdk_registry_ping_impl) (void *impl);

typedef devsdk_nvpairs *(*devsdk_registry_get_config_impl)
  (void *impl, const char *servicename, devsdk_registry_updatefn updater, void *updatectx, atomic_bool *updatedone, devsdk_error *err);

typedef void (*devsdk_registry_put_config_impl) (void *impl, const char *servicename, const iot_data_t *config, devsdk_error *err);

typedef void (*devsdk_registry_register_service_impl)
  (void *impl, const char *servicename, const char *hostname, uint16_t port, const char *checkInterval, devsdk_error *err);

typedef void (*devsdk_registry_deregister_service_impl) (void *impl, const char *servicename, devsdk_error *err);

typedef void (*devsdk_registry_query_service_impl)
  (void *impl, const char *servicename, char **hostname, uint16_t *port, devsdk_error *err);

typedef void (*devsdk_registry_free_impl) (void *impl);

typedef struct devsdk_registry_impls
{
  devsdk_registry_init_impl init;
  devsdk_registry_ping_impl ping;
  devsdk_registry_get_config_impl get_config;
  devsdk_registry_put_config_impl put_config;
  devsdk_registry_register_service_impl register_service;
  devsdk_registry_deregister_service_impl deregister_service;
  devsdk_registry_query_service_impl query_service;
  devsdk_registry_free_impl free;
} devsdk_registry_impls;

#endif
