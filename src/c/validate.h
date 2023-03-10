/*
 * Copyright (c) 2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_VALIDATE_H_
#define _EDGEX_DEVICE_VALIDATE_H_ 1

#include <iot/data.h>

int32_t edgex_device_handler_validate_addr_v3 (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply);

#endif
