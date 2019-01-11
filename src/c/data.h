/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DATA_H_
#define _EDGEX_DATA_H_ 1

#include "edgex/devsdk.h"
#include "parson.h"

typedef struct edgex_service_endpoints edgex_service_endpoints;

JSON_Value *edgex_data_generate_event
(
  const char *device_name,
  uint32_t nreadings,
  const edgex_device_commandrequest *sources,
  const edgex_device_commandresult *values,
  bool doTransforms
);

void edgex_data_client_add_event
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  JSON_Value *eventval,
  edgex_error *err
);

edgex_valuedescriptor *edgex_data_client_add_valuedescriptor
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const char *name,
  uint64_t origin,
  const char *min,
  const char *max,
  const char *type,
  const char *uomLabel,
  const char *defaultValue,
  const char *formatting,
  const char *description,
  edgex_error *err
);

bool edgex_data_client_ping
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  edgex_error *err
);

#endif
