/*
 * Copyright (c) 2018-2025
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DATA_H_
#define _EDGEX_DATA_H_ 1

#include "devsdk/devsdk.h"
#include "bus.h"
#include "metrics.h"
#include "config.h"
#include "parson.h"
#include "cmdinfo.h"
#include "rest-server.h"
#include "iot/threadpool.h"

typedef enum { JSON, CBOR} edgex_event_encoding;

typedef struct edgex_event_cooked
{
  unsigned nrdgs;
  char *path;
  edgex_event_encoding encoding;
  iot_data_t *value;
} edgex_event_cooked;

size_t edgex_event_cooked_size (edgex_event_cooked *e);
void edgex_event_cooked_write (edgex_event_cooked *e, devsdk_http_reply *rep);
void edgex_event_cooked_free (edgex_event_cooked *e);

edgex_event_cooked *edgex_data_process_event
(
  const edgex_device *device,
  const edgex_cmdinfo *commandinfo,
  devsdk_commandresult *values,
  iot_data_t *tags,
  bool doTransforms,
  bool reducedEvents
);

void edgex_data_client_add_event (edgex_bus_t *bus, edgex_event_cooked *eventval, devsdk_metrics_t *metrics);

void devsdk_commandresult_free (devsdk_commandresult *res, int n);

devsdk_commandresult *devsdk_commandresult_dup (const devsdk_commandresult *res, int n);

bool devsdk_commandresult_equal (const devsdk_commandresult *lhs, const devsdk_commandresult *rhs, int n);

#endif
