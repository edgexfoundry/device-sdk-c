/*
 * Copyright (c) 2021-2022
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_SECRETS_H_
#define _EDGEX_SECRETS_H_ 1

#include "rest.h"
#include "rest-server.h"
#include "metrics.h"
#include "iot/scheduler.h"

typedef struct edgex_secret_provider_t edgex_secret_provider_t;

edgex_secret_provider_t *edgex_secrets_get_insecure (void);
edgex_secret_provider_t *edgex_secrets_get_vault (void);

bool edgex_secrets_init
  (edgex_secret_provider_t *sp, iot_logger_t *lc, iot_scheduler_t *sched, iot_threadpool_t *pool, const char *svcname, iot_data_t *config, devsdk_metrics_t *m);
void edgex_secrets_reconfigure (edgex_secret_provider_t *sp, iot_data_t *config);
iot_data_t *edgex_secrets_get (edgex_secret_provider_t *sp, const char *path);
iot_data_t *edgex_secrets_request_jwt (edgex_secret_provider_t *sp);
bool edgex_secrets_is_jwt_valid (edgex_secret_provider_t *sp, const char *jwt);
void edgex_secrets_fini (edgex_secret_provider_t *sp);

void edgex_device_handler_secret (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply);

#endif
