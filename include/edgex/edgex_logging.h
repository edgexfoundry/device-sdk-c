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
#include "iot/logging.h"

/* Built-in logger: post to an EdgeX logging service */

extern bool edgex_log_torest
(
  const char *destination,
  const char *subsystem,
  iot_loglevel l,
  time_t timestamp,
  const char *message
);

#endif
