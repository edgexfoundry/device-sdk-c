/*
 * Copyright (c) 2018-2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "edgex-logging.h"

#include <stdio.h>
#include <stdlib.h>

#include "correlation.h"

#define EDGEX_TSIZE 32

static const char *edgex_log_levels[] = {"", "ERROR", "WARNING", "INFO", "DEBUG", "TRACE"};

void edgex_log_tofile (struct iot_logger_t * logger, iot_loglevel_t l, time_t timestamp, const char *message)
{
  struct tm tsparts;
  char ts8601[EDGEX_TSIZE];
  const char *crlid;
  const char *lname;

  crlid = edgex_device_get_crlid ();
  lname = logger->name ? logger->name : "(default)";
  gmtime_r (&timestamp, &tsparts);
  strftime (ts8601, EDGEX_TSIZE, "%FT%TZ", &tsparts);

  printf ("level=%s ts=%s app=%s%s%s msg=\"%s\"\n", edgex_logger_levelname (l), ts8601, lname, crlid ? " correlation-id=" : "", crlid ? crlid : "", message);
  fflush (stdout);
}

const char *edgex_logger_levelname (iot_loglevel_t l)
{
  assert (l >= IOT_LOG_NONE && l <= IOT_LOG_TRACE);
  return edgex_log_levels[l];
}
