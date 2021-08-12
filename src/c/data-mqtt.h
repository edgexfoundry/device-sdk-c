/*
 * Copyright (c) 2021
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_DATA_MQTT_H_
#define _EDGEX_DEVICE_DATA_MQTT_H_ 1

#include "parson.h"
#include "data.h"
#include "devutil.h"

void edgex_mqtt_config_defaults (iot_data_t *allconf);

JSON_Value *edgex_mqtt_config_json (const iot_data_t *allconf);

edgex_data_client_t *edgex_data_client_new_mqtt (const iot_data_t *allconf, iot_logger_t *lc, const devsdk_timeout *tm, iot_threadpool_t *queue);

#endif
