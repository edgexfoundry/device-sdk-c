/*
 * Copyright (c) 2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_API_H_
#define _EDGEX_API_H_ 1

/* Endpoints */

#define EDGEX_DEV_API_PING "/api/v1/ping"
#define EDGEX_DEV_API_VERSION "/api/version"
#define EDGEX_DEV_API_DISCOVERY "/api/v1/discovery"
#define EDGEX_DEV_API_DEVICE "/api/v1/device/{id}/{cmd}"
#define EDGEX_DEV_API_DEVICE_NAME "/api/v1/device/name/{name}/{cmd}"
#define EDGEX_DEV_API_DEVICE_ALL "/api/v1/device/all/{cmd}"
#define EDGEX_DEV_API_CALLBACK "/api/v1/callback"
#define EDGEX_DEV_API_CONFIG "/api/v1/config"
#define EDGEX_DEV_API_METRICS "/api/v1/metrics"

#define EDGEX_DEV_API2_PING "/api/v2/ping"
#define EDGEX_DEV_API2_DISCOVERY "/api/v2/discovery"
#define EDGEX_DEV_API2_CONFIG "/api/v2/config"
#define EDGEX_DEV_API2_METRICS "/api/v2/metrics"
#define EDGEX_DEV_API2_DEVICE "/api/v2/device/{id}/{cmd}"
#define EDGEX_DEV_API2_DEVICE_NAME "/api/v2/device/name/{name}/{cmd}"

/* Query parameters */

#define DS_PREFIX "ds-"

#define DS_POST "ds-postevent"
#define DS_RETURN "ds-returnevent"

#define DS_PARAMLIST { DS_POST, DS_RETURN }

#endif
