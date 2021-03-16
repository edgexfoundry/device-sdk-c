/*
 * Copyright (c) 2018-2020
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
#include "rest-server.h"
#include "iot/threadpool.h"

typedef enum { JSON, CBOR} edgex_event_encoding;

typedef struct edgex_event_cooked
{
  atomic_uint_fast32_t refs;
  char *path;
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

void edgex_event_cooked_add_ref (edgex_event_cooked *e);
void edgex_event_cooked_write (edgex_event_cooked *e, devsdk_http_reply *rep);
void edgex_event_cooked_free (edgex_event_cooked *e);

edgex_event_cooked *edgex_data_process_event
(
  const char *device_name,
  const edgex_cmdinfo *commandinfo,
  devsdk_commandresult *values,
  bool doTransforms
);

typedef void (*edc_freefn) (iot_logger_t *lc, void *address);
typedef void (*edc_postfn) (iot_logger_t *lc, void *address, edgex_event_cooked *event);

typedef struct edgex_data_client_t
{
  void *address;
  iot_logger_t *lc;
  iot_threadpool_t *queue;
  edc_postfn pf;
  edc_freefn ff;
} edgex_data_client_t;

typedef struct edgex_device_service_endpoint edgex_device_service_endpoint;

edgex_data_client_t *edgex_data_client_new_rest (const edgex_device_service_endpoint *e, iot_logger_t *lc, iot_threadpool_t *queue);

void edgex_data_client_free (edgex_data_client_t *client);

void edgex_data_client_add_event (edgex_data_client_t *client, edgex_event_cooked *eventval);

void edgex_data_client_add_event_now (edgex_data_client_t *client, edgex_event_cooked *eventval);

void devsdk_commandresult_free (devsdk_commandresult *res, int n);

devsdk_commandresult *devsdk_commandresult_dup (const devsdk_commandresult *res, int n);

bool devsdk_commandresult_equal (const devsdk_commandresult *lhs, const devsdk_commandresult *rhs, int n);

#endif
