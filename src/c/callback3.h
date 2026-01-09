/*
 * Copyright (c) 2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_CALLBACK3_H_
#define _EDGEX_DEVICE_CALLBACK3_H_ 1

#include <iot/data.h>

extern int32_t edgex_callback_add_device (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor);
extern int32_t edgex_callback_delete_device (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor);
extern int32_t edgex_callback_update_device (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor);

extern int32_t edgex_callback_add_pw (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor);
extern int32_t edgex_callback_delete_pw (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor);
extern int32_t edgex_callback_update_pw (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor);

extern int32_t edgex_callback_update_deviceservice (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor);

extern int32_t edgex_callback_update_profile (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor);

#endif
