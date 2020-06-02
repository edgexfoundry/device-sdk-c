/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DATA_H_
#define _EDGEX_DATA_H_ 1

#include "devsdk/devsdk.h"
#include "parson.h"
#include "cmdinfo.h"

typedef struct edgex_reading
{
  uint64_t created;
  char *id;
  uint64_t modified;
  const char *name;
  uint64_t origin;
  uint64_t pushed;
  devsdk_commandresult value;
  bool binfloat;
  struct edgex_reading *next;
} edgex_reading;

typedef struct edgex_event
{
  uint64_t created;
  const char *device;
  char *id;
  uint64_t modified;
  uint64_t origin;
  uint64_t pushed;
  edgex_reading *readings;
  struct edgex_event *next;
} edgex_event;

typedef enum { JSON, CBOR} edgex_event_encoding;

typedef struct edgex_event_cooked
{
  edgex_event_encoding encoding;
  union
  {
    char *json;
    struct
    {
      unsigned char *data;
      size_t length;
    } cbor;
  } value;
} edgex_event_cooked;

typedef struct
{
  uint64_t created;
  char *defaultValue;
  char *description;
  char *formatting;
  char *id;
  devsdk_strings *labels;
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

void edgex_event_cooked_free (edgex_event_cooked *e);

edgex_event_cooked *edgex_data_process_event
(
  const char *device_name,
  const edgex_cmdinfo *commandinfo,
  devsdk_commandresult *values,
  bool doTransforms
);

void edgex_data_client_add_event
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  edgex_event_cooked *eventval,
  devsdk_error *err
);

edgex_valuedescriptor *edgex_data_client_add_valuedescriptor
(
  iot_logger_t *lc,
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
  devsdk_error *err
);

void devsdk_commandresult_free (devsdk_commandresult *res, int n);

devsdk_commandresult *devsdk_commandresult_dup (const devsdk_commandresult *res, int n);

bool devsdk_commandresult_equal (const devsdk_commandresult *lhs, const devsdk_commandresult *rhs, int n);

#endif
