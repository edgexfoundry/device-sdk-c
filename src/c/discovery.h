/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_DISCOVERY_H_
#define _EDGEX_DEVICE_DISCOVERY_H_ 1

#include "devsdk/devsdk.h"
#include "rest-server.h"
#include "iot/scheduler.h"

#include <stddef.h>

typedef struct edgex_device_periodic_discovery_t edgex_device_periodic_discovery_t;

extern edgex_device_periodic_discovery_t *edgex_device_periodic_discovery_alloc
  (iot_logger_t *logger, iot_scheduler_t *sched, iot_threadpool_t *pool, devsdk_discover discfn, devsdk_discovery_delete disc_cacnel_fn, void *userdata);

extern void edgex_device_periodic_discovery_configure (edgex_device_periodic_discovery_t *disc, bool enabled, uint64_t interval);

extern void edgex_device_periodic_discovery_stop (edgex_device_periodic_discovery_t *disc);

extern void edgex_device_periodic_discovery_free (edgex_device_periodic_discovery_t *disc);

extern void edgex_device_handler_discoveryv2 (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply);

extern void edgex_device_handler_discovery_cancel (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply);

#endif
