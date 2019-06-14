/*
 * Copyright (c) 2018, 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_EX_TIME_H_
#define _EDGEX_DEVICE_EX_TIME_H_ 1

#include <inttypes.h>

#define EDGEX_MILLIS 1000U
#define EDGEX_MICROS 1000000U
#define EDGEX_NANOS 1000000000U

extern uint64_t edgex_device_millitime (void);
extern uint64_t edgex_device_nanotime (void);
extern uint64_t edgex_device_nanotime_monotonic (void);

#endif
