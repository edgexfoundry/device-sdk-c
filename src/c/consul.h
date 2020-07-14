/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_CONSUL_H_
#define _EDGEX_CONSUL_H_ 1

#include "registry.h"

bool edgex_consul_client_ping
(
  iot_logger_t *lc,
  void *location,
  devsdk_error *err
);

devsdk_nvpairs *edgex_consul_client_get_config
(
  iot_logger_t *lc,
  iot_threadpool_t *thpool,
  void *location,
  const char *servicename,
  const char *profile,
  devsdk_registry_updatefn updater,
  void *updatectx,
  atomic_bool *updatedone,
  devsdk_error *err
);

void edgex_consul_client_write_config
(
  iot_logger_t *lc,
  void *location,
  const char *servicename,
  const char *profile,
  const iot_data_t *config,
  devsdk_error *err
);

void edgex_consul_client_register_service
(
  iot_logger_t *lc,
  void *location,
  const char *servicename,
  const char *host,
  uint16_t port,
  const char *checkInterval,
  devsdk_error *err
);

void edgex_consul_client_deregister_service
  (iot_logger_t *lc, void *location, const char *servicename, devsdk_error *err);

void edgex_consul_client_query_service
(
  iot_logger_t *lc,
  void *location,
  const char *servicename,
  char **host,
  uint16_t *port,
  devsdk_error *err
);

#endif
