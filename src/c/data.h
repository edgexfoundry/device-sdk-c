/*
 * Copyright (c) 2018-2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DATA_H_
#define _EDGEX_DATA_H_ 1

#include "devsdk/devsdk.h"
#include "metrics.h"
#include "config.h"
#include "parson.h"
#include "cmdinfo.h"
#include "rest-server.h"
#include "iot/threadpool.h"

#define EX_BUS_TYPE "MessageBus/Type"

#define EX_BUS_DISABLED "MessageBus/Disabled"
#define EX_BUS_PROTOCOL "MessageBus/Protocol"
#define EX_BUS_HOST "MessageBus/Host"
#define EX_BUS_PORT "MessageBus/Port"
#define EX_BUS_AUTHMODE "MessageBus/AuthMode"
#define EX_BUS_SECRETNAME "MessageBus/SecretName"
#define EX_BUS_CLIENTID "MessageBus/Optional/ClientId"
#define EX_BUS_QOS "MessageBus/Optional/Qos"
#define EX_BUS_KEEPALIVE "MessageBus/Optional/KeepAlive"
#define EX_BUS_RETAINED "MessageBus/Optional/Retained"
#define EX_BUS_CERTFILE "MessageBus/Optional/CertFile"
#define EX_BUS_KEYFILE "MessageBus/Optional/KeyFile"
#define EX_BUS_SKIPVERIFY "MessageBus/Optional/SkipCertVerify"
#define EX_BUS_TOPIC "MessageBus/Topics/PublishTopicPrefix"
#define EX_BUS_TOPIC_CMDREQ "MessageBus/Topics/CommandRequestTopic"
#define EX_BUS_TOPIC_CMDRESP "MessageBus/Topics/CommandResponseTopicPrefix"

typedef enum { JSON, CBOR} edgex_event_encoding;

typedef struct edgex_event_cooked
{
  atomic_uint_fast32_t refs;
  unsigned nrdgs;
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
size_t edgex_event_cooked_size (edgex_event_cooked *e);
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
typedef void (*edc_metfn) (void *address, const char *mname, const iot_data_t *envelope);

typedef struct edgex_data_client_t
{
  void *address;
  iot_logger_t *lc;
  iot_threadpool_t *queue;
  edc_postfn pf;
  edc_freefn ff;
  edc_metfn mf;
} edgex_data_client_t;

typedef struct edgex_device_service_endpoint edgex_device_service_endpoint;

void edgex_data_client_free (edgex_data_client_t *client);

void edgex_data_client_add_event (edgex_data_client_t *client, edgex_event_cooked *eventval, devsdk_metrics_t *metrics);

void edgex_data_client_add_event_now (edgex_data_client_t *client, edgex_event_cooked *eventval, devsdk_metrics_t *metrics);

void edgex_data_client_publish_metrics (edgex_data_client_t *client, const devsdk_metrics_t *metrics, const edgex_device_metricinfo *cfg);

void devsdk_commandresult_free (devsdk_commandresult *res, int n);

devsdk_commandresult *devsdk_commandresult_dup (const devsdk_commandresult *res, int n);

bool devsdk_commandresult_equal (const devsdk_commandresult *lhs, const devsdk_commandresult *rhs, int n);

#endif
