/*
 * Copyright (c) 2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_BUS_H_
#define _EDGEX_BUS_H_ 1

#include "devsdk/devsdk.h"
#include "config.h"
#include "parson.h"
#include "secrets.h"
#include "devutil.h"
#include <iot/threadpool.h>

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
#define EX_BUS_TOPIC "MessageBus/BaseTopicPrefix"

typedef struct edgex_bus_t edgex_bus_t;

typedef int32_t (*edgex_handler_fn) (void *ctx, const iot_data_t *request, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor);

void edgex_bus_config_defaults (iot_data_t *allconf, const char *svcname);
JSON_Value *edgex_bus_config_json (const iot_data_t *allconf);

edgex_bus_t *edgex_bus_create_mqtt
  (iot_logger_t *lc, const char *svcname, const iot_data_t *cfg, edgex_secret_provider_t *secstore, iot_threadpool_t *queue, const devsdk_timeout *tm);

void edgex_bus_register_handler (edgex_bus_t *bus, const char *path, void *ctx, edgex_handler_fn handler);
char *edgex_bus_mktopic (edgex_bus_t *bus, const char *type, const char *param);
void edgex_bus_post (edgex_bus_t *bus, const char *path, const iot_data_t *payload, bool event_is_cbor);
int edgex_bus_rmi (edgex_bus_t *bus, const char *path, const char *svcname, const iot_data_t *request, iot_data_t **reply);

void edgex_bus_free (edgex_bus_t *bus);

#endif
