/*
 * Copyright (c) 2020-2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_API_H_
#define _EDGEX_API_H_ 1

#define EDGEX_API_VERSION "v3"

/* Endpoints */

#define EDGEX_DEV_API_VERSION "/api/version"

#define EDGEX_DEV_API3_PING "/api/v3/ping"
#define EDGEX_DEV_API3_DISCOVERY "/api/v3/discovery"
#define EDGEX_DEV_API3_CONFIG "/api/v3/config"
#define EDGEX_DEV_API3_METRICS "/api/v3/metrics"
#define EDGEX_DEV_API3_SECRET "/api/v3/secret"
#define EDGEX_DEV_API3_DEVICE_NAME "/api/v3/device/name/{name}/{cmd}"

/* Topics */

#define EDGEX_DEV_TOPIC_DEVICE "device/command/request"
#define EDGEX_DEV_TOPIC_VALIDATE "validate/device"
#define EDGEX_DEV_TOPIC_RESPONSE "response"
#define EDGEX_DEV_TOPIC_EVENT "events/device"
#define EDGEX_DEV_TOPIC_METRIC "telemetry"
#define EDGEX_DEV_TOPIC_DISCOVERY "discovery"
#define EDGEX_DEV_TOPIC_ADD_DEV "system-events/core-metadata/device/add"
#define EDGEX_DEV_TOPIC_DEL_DEV "system-events/core-metadata/device/delete"
#define EDGEX_DEV_TOPIC_UPDATE_DEV "system-events/core-metadata/device/update"
#define EDGEX_DEV_TOPIC_ADD_PW "system-events/core-metadata/provisionwatcher/add"
#define EDGEX_DEV_TOPIC_DEL_PW "system-events/core-metadata/provisionwatcher/delete"
#define EDGEX_DEV_TOPIC_UPDATE_PW "system-events/core-metadata/provisionwatcher/update"
#define EDGEX_DEV_TOPIC_DEVICESERVICE "system-events/core-metadata/deviceservice/update"
#define EDGEX_DEV_TOPIC_UPDATE_PROFILE "system-events/core-metadata/deviceprofile/update"

/* Query parameters */

#define DS_PREFIX "ds-"

#define DS_PUSH "ds-pushevent"
#define DS_RETURN "ds-returnevent"

#define DS_PARAMLIST { DS_PUSH, DS_RETURN }

/* Path segments */

#define ALL_SVCS_NODE "all-services"
#define DEV_SVCS_NODE "device-services"

#endif
