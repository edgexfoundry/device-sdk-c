/*
 * Copyright (c) 2024
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_OPSTATE_H_
#define _EDGEX_DEVICE_OPSTATE_H_ 1

#include "devsdk/devsdk.h"
#include "edgex/edgex.h"

void devsdk_device_request_failed (devsdk_service_t *svc, edgex_device *dev);

void devsdk_device_request_succeeded (devsdk_service_t *svc, edgex_device *dev);

#endif
