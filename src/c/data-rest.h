/*
 * Copyright (c) 2021
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_DATA_REST_H_
#define _EDGEX_DEVICE_DATA_REST_H_ 1

#include "data.h"
#include "config.h"

edgex_data_client_t *edgex_data_client_new_rest (const edgex_device_service_endpoint *e, iot_logger_t *lc, iot_threadpool_t *queue);

#endif
