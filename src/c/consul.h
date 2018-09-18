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
#include "edgex/edgex_logging.h"
#include "edgex/error.h"

typedef struct edgex_service_endpoints edgex_service_endpoints;

const char *edgex_consul_client_get_value
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const char *servicename,
  const char *config,
  const char *key,
  edgex_error *err
);

int edgex_consul_client_get_keys
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const char *servicename,
  const char *config,
  const char *base,
  char ***results,
  edgex_error *err
);

#endif
