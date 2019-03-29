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

typedef struct edgex_reading
{
  uint64_t created;
  char *id;
  uint64_t modified;
  char *name;
  uint64_t origin;
  uint64_t pushed;
  char *value;
  struct edgex_reading *next;
} edgex_reading;

typedef struct edgex_event
{
  uint64_t created;
  char *device;
  char *id;
  uint64_t modified;
  uint64_t origin;
  uint64_t pushed;
  edgex_reading *readings;
  struct edgex_event *next;
} edgex_event;

typedef struct
{
  uint64_t created;
  char *defaultValue;
  char *description;
  char *formatting;
  char *id;
  edgex_strings *labels;
  char *max;
  char *min;
  uint64_t modified;
  char *name;
  uint64_t origin;
  char *type;
  char *uomLabel;
  char *mediaType;
  char *floatEncoding;
} edgex_valuedescriptor;

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
  const char *mediaType,
  const char *floatEncoding,
  edgex_error *err
);

bool edgex_data_client_ping
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  edgex_error *err
);

#endif
