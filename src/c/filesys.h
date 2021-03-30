/*
 * Copyright (c) 2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_FILESYS_H_
#define _EDGEX_FILESYS_H_ 1

#include "devsdk/devsdk-base.h"
#include "iot/logger.h"

devsdk_strings * devsdk_scandir (iot_logger_t *lc, const char *dir, const char *ext);

#endif
