/*
 * Copyright (c) 2020-2022
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_API_H_
#define _EDGEX_API_H_ 1

/* Endpoints */

#define EDGEX_DEV_API_VERSION "/api/version"

#define EDGEX_DEV_API2_PING "/api/v2/ping"
#define EDGEX_DEV_API2_DISCOVERY "/api/v2/discovery"
#define EDGEX_DEV_API2_CONFIG "/api/v2/config"
#define EDGEX_DEV_API2_METRICS "/api/v2/metrics"
#define EDGEX_DEV_API2_SECRET "/api/v2/secret"
#define EDGEX_DEV_API2_DEVICE_NAME "/api/v2/device/name/{name}/{cmd}"
#define EDGEX_DEV_API2_CALLBACK_DEVICE "/api/v2/callback/device"
#define EDGEX_DEV_API2_CALLBACK_DEVICE_NAME "/api/v2/callback/device/name/{name}"
#define EDGEX_DEV_API2_CALLBACK_PROFILE "/api/v2/callback/profile"
#define EDGEX_DEV_API2_CALLBACK_WATCHER "/api/v2/callback/watcher"
#define EDGEX_DEV_API2_CALLBACK_WATCHER_NAME "/api/v2/callback/watcher/name/{name}"
#define EDGEX_DEV_API2_CALLBACK_SERVICE "/api/v2/callback/service"
#define EDGEX_DEV_API2_VALIDATE_ADDR "/api/v2/validate/device"

/* Query parameters */

#define DS_PREFIX "ds-"

#define DS_PUSH "ds-pushevent"
#define DS_RETURN "ds-returnevent"

#define DS_PARAMLIST { DS_PUSH, DS_RETURN }

#endif
