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
#include "errorlist.h"
#include "rest.h"
#include "parson.h"

#define EDGEX_TSIZE 32

static const char *edgex_log_levels[] = {"", "ERROR", "WARNING", "INFO", "DEBUG", "TRACE"};

void edgex_log_tostdout (struct iot_logger_t * logger, iot_loglevel_t l, uint64_t timestamp, const char *message, const void *ctx)
{
  time_t ts;
  struct tm tsparts;
  char ts8601[EDGEX_TSIZE];
  const char *crlid;

  ts = timestamp / 1000000U;
  crlid = edgex_device_get_crlid ();
  gmtime_r (&ts, &tsparts);
  strftime (ts8601, EDGEX_TSIZE, "%FT%TZ", &tsparts);

  printf ("level=%s ts=%s app=%s%s%s msg=\"%s\"\n", edgex_logger_levelname (l), ts8601, (const char *)ctx, crlid ? " correlation-id=" : "", crlid ? crlid : "", message);

  fflush (stdout);
}

const char *edgex_logger_levelname (iot_loglevel_t l)
{
  assert (l >= IOT_LOG_NONE && l <= IOT_LOG_TRACE);
  return edgex_log_levels[l];
}

bool edgex_logger_nametolevel (const char *lstr, iot_loglevel_t *level)
{
  iot_loglevel_t l;
  for (l = IOT_LOG_ERROR; l <= IOT_LOG_TRACE; l++)
  {
    if (strcasecmp (lstr, edgex_log_levels[l]) == 0)
    {
      *level = l;
      return true;
    }
  }
  return false;
}
