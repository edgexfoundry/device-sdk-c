/*
 * Copyright (c) 2019-2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_TRANSFORM_H_
#define _EDGEX_DEVICE_TRANSFORM_H_ 1

#include "devsdk/devsdk.h"
#include "edgex/edgex.h"

void edgex_transform_outgoing (devsdk_commandresult *cres, edgex_propertyvalue *props, devsdk_nvpairs *mappings);

void edgex_transform_incoming (iot_data_t **cres, edgex_propertyvalue *props, devsdk_nvpairs *mappings);

bool edgex_transform_validate (const iot_data_t *val, const edgex_propertyvalue *props);

#endif
