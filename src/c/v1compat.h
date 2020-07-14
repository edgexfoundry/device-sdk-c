/*
 * Copyright (c) 2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_V1COMPAT_H_
#define _EDGEX_V1COMPAT_H_ 1

#include "iot/data.h"
#include "iot/logger.h"

bool compat_init (void *impl, struct iot_logger_t *lc, const iot_data_t *config);

#endif
