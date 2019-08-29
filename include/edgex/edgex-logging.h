/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_LOGGING_H_
#define _EDGEX_LOGGING_H_ 1

#include "edgex/os.h"
#include "iot/logger.h"

/* Built-in logger: post to an EdgeX logging service */

extern void edgex_log_torest
(
  struct iot_logger_t * logger,
  iot_loglevel_t l,
  time_t timestamp,
  const char *message
);

/* Built-in logger: write to a file or stdout */

extern void edgex_log_tofile
(
  struct iot_logger_t * logger,
  iot_loglevel_t l,
  time_t timestamp,
  const char *message
);

/* Get name of log level */

extern const char * edgex_logger_levelname (iot_loglevel_t level);

#endif
