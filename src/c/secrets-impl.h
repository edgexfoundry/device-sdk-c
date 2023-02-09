/*
 * Copyright (c) 2021-2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_SECRETS_IMPL_H_
#define _EDGEX_SECRETS_IMPL_H_ 1

#include "edgex/edgex-base.h"
#include "iot/scheduler.h"
#include "rest.h"
#include "metrics.h"

typedef bool (*edgex_secret_init_fn)
  (void *impl, iot_logger_t *lc, iot_scheduler_t *sched, iot_threadpool_t *pool, const char *svcname, iot_data_t *config, devsdk_metrics_t *m);
typedef void (*edgex_secret_reconfigure_fn) (void *impl, iot_data_t *config);
typedef iot_data_t * (*edgex_secret_get_fn) (void *impl, const char *path);
typedef void (*edgex_secret_set_fn) (void *impl, const char *path, const iot_data_t *secrets);
typedef void (*edgex_secret_getregtoken_fn) (void *impl, edgex_ctx *ctx);
typedef void (*edgex_secret_releaseregtoken_fn) (void *impl);
typedef iot_data_t * (*edgex_secret_request_jwt_fn) (void *impl);
typedef bool (*edgex_secret_is_jwt_valid_fn) (void *impl, const char *jwt);
typedef void (*edgex_secret_fini_fn) (void *impl);

typedef struct
{
  edgex_secret_init_fn init;
  edgex_secret_reconfigure_fn reconfigure;
  edgex_secret_get_fn get;
  edgex_secret_set_fn set;
  edgex_secret_getregtoken_fn getregtoken;
  edgex_secret_releaseregtoken_fn releaseregtoken;
  edgex_secret_request_jwt_fn requestjwt;
  edgex_secret_is_jwt_valid_fn isjwtvalid;
  edgex_secret_fini_fn fini;
} edgex_secret_impls;

#endif
