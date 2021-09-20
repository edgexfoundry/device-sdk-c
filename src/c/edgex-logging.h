/*
 * Copyright (c) 2018-2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_LOGGING_H_
#define _EDGEX_LOGGING_H_ 1

#include "iot/logger.h"

/* Write to stdout */

extern void edgex_log_tostdout (struct iot_logger_t *logger, iot_loglevel_t l, uint64_t timestamp, const char *message);

/* Get name of log level */

extern const char * edgex_logger_levelname (iot_loglevel_t level);

/* Get level for name */

extern bool edgex_logger_nametolevel (const char *lstr, iot_loglevel_t *level);

#endif
