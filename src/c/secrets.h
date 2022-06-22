/*
 * Copyright (c) 2021-2022
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_SECRETS_H_
#define _EDGEX_SECRETS_H_ 1

#include "rest-server.h"

typedef struct edgex_secret_provider_t edgex_secret_provider_t;

edgex_secret_provider_t *edgex_secrets_get_insecure (void);
edgex_secret_provider_t *edgex_secrets_get_vault (void);

bool edgex_secrets_init (edgex_secret_provider_t *sp, iot_logger_t *lc, const char *svcname, iot_data_t *config);
void edgex_secrets_reconfigure (edgex_secret_provider_t *sp, iot_data_t *config);
iot_data_t *edgex_secrets_get (edgex_secret_provider_t *sp, const char *path);
devsdk_nvpairs *edgex_secrets_getregtoken (edgex_secret_provider_t *sp);
void edgex_secrets_fini (edgex_secret_provider_t *sp);

void edgex_device_handler_secret (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply);

#endif
