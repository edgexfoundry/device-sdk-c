/*
 * Copyright (c) 2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DTO_READ_H_
#define _EDGEX_DTO_READ_H_ 1

#include <iot/data.h>
#include <iot/logger.h>
#include "edgex/edgex.h"

edgex_device_adminstate edgex_adminstate_read (const iot_data_t *obj);
edgex_device *edgex_device_read (const iot_data_t *obj);
edgex_watcher *edgex_pw_read (const iot_data_t *obj);
edgex_watcher *edgex_pws_read (const iot_data_t *obj);
edgex_deviceprofile *edgex_profile_read (const iot_data_t *obj);

#endif
