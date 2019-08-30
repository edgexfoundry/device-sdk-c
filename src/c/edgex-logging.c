/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "edgex/edgex-logging.h"

#include <stdio.h>
#include <string.h>
#include <alloca.h>
#include <stdlib.h>
#include <pthread.h>

#include "errorlist.h"
#include "parson.h"
#include "rest.h"
#include "correlation.h"

#define EDGEX_TSIZE 32

static const char * edgex_log_levels[] = {"", "ERROR", "WARNING", "INFO", "DEBUG", "TRACE"};

void edgex_log_torest
(
  struct iot_logger_t * logger,
  iot_loglevel_t l,
  time_t timestamp,
  const char *message
)
{
  edgex_ctx ctx;
  char *json;
  edgex_error err = EDGEX_OK;

  memset (&ctx, 0, sizeof (ctx));

  JSON_Value *jval = json_value_init_object ();
  JSON_Object *jobj = json_value_get_object (jval);

  json_object_set_string (jobj, "originService", logger->name);
  json_object_set_string (jobj, "logLevel", edgex_logger_levelname (l));
  json_object_set_number (jobj, "created", timestamp);
  json_object_set_string (jobj, "message", message);

  json = json_serialize_to_string (jval);
  edgex_http_post (iot_logger_default (), &ctx, logger->to, json, NULL, &err);
  json_free_serialized_string (json);

  json_value_free (jval);
}

void edgex_log_tofile
(
  struct iot_logger_t * logger,
  iot_loglevel_t l,
  time_t timestamp,
  const char *message
)
{
  FILE *f;
  struct tm tsparts;
  char ts8601[EDGEX_TSIZE];
  const char *crlid;

  if (strcmp (logger->to, "-") == 0)
  {
    f = stdout;
  }
  else
  {
    f = fopen (logger->to, "a");
    if (!f)
    {
      f = stdout;
    }
  }

  crlid = edgex_device_get_crlid ();
  gmtime_r (&timestamp, &tsparts);
  strftime (ts8601, EDGEX_TSIZE, "%FT%TZ", &tsparts);
  fprintf
  (
    f,
    "level=%s ts=%s app=%s%s%s msg=\"%s\"\n",
    edgex_logger_levelname (l),
    ts8601,
    logger->name ? logger->name : "(default)",
    crlid ? " correlation-id=" : "",
    crlid ? crlid : "",
    message
  );

  if (f == stdout)
  {
    fflush (f);
  }
  else
  {
    fclose (f);
  }
}

const char *edgex_logger_levelname (iot_loglevel_t l)
{
  assert (l >= IOT_LOG_NONE && l <= IOT_LOG_TRACE);
  return edgex_log_levels[l];
}
