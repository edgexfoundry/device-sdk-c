/*
 * Copyright (c) 2021
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_DATA_REDSTR_H_
#define _EDGEX_DEVICE_DATA_REDSTR_H_ 1

#include "parson.h"
#include "data.h"

JSON_Value *edgex_redstr_config_json (const iot_data_t *allconf);

edgex_data_client_t *edgex_data_client_new_redstr (const iot_data_t *allconf, iot_logger_t *lc);

#endif
