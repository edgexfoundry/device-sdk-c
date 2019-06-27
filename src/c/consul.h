/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_CONSUL_H_
#define _EDGEX_CONSUL_H_ 1

#include "edgex/edgex.h"
#include "edgex/edgex-logging.h"
#include "edgex/registry.h"
#include "edgex/error.h"

bool edgex_consul_client_ping
(
  iot_logger_t *lc,
  void *location,
  edgex_error *err
);

edgex_nvpairs *edgex_consul_client_get_config
(
  iot_logger_t *lc,
  iot_threadpool_t *thpool,
  void *location,
  const char *servicename,
  const char *profile,
  edgex_registry_updatefn updater,
  void *updatectx,
  atomic_bool *updatedone,
  edgex_error *err
);

void edgex_consul_client_write_config
(
  iot_logger_t *lc,
  void *location,
  const char *servicename,
  const char *profile,
  const edgex_nvpairs *config,
  edgex_error *err
);

void edgex_consul_client_register_service
(
  iot_logger_t *lc,
  void *location,
  const char *servicename,
  const char *host,
  uint16_t port,
  const char *checkInterval,
  edgex_error *err
);

void edgex_consul_client_deregister_service
  (iot_logger_t *lc, void *location, const char *servicename, edgex_error *err);

void edgex_consul_client_query_service
(
  iot_logger_t *lc,
  void *location,
  const char *servicename,
  char **host,
  uint16_t *port,
  edgex_error *err
);

#endif
