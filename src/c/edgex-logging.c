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

bool edgex_log_torest
(
  const char *destination,
  const char *subsystem,
  iot_loglevel_t l,
  time_t timestamp,
  const char *message
)
{
  long retcode;
  edgex_ctx ctx;
  char *json;
  edgex_error err = EDGEX_OK;

  memset (&ctx, 0, sizeof (ctx));

  JSON_Value *jval = json_value_init_object ();
  JSON_Object *jobj = json_value_get_object (jval);

  json_object_set_string (jobj, "originService", subsystem);
  json_object_set_string (jobj, "logLevel", iot_logger_levelname (l));
  json_object_set_number (jobj, "created", timestamp);
  json_object_set_string (jobj, "message", message);

  json = json_serialize_to_string (jval);
  retcode = edgex_http_post
    (iot_log_default (), &ctx, destination, json, NULL, &err);
  json_free_serialized_string (json);

  json_value_free (jval);

  return (retcode == 202 && err.code == EDGEX_OK.code);
}

bool edgex_log_tofile
(
   const char *destination,
   const char *subsystem,
   iot_loglevel_t l,
   time_t timestamp,
   const char *message
)
{
   FILE *f;
   if (strcmp (destination, "-") == 0)
   {
     f = stdout;
   }
   else
   {
     f = fopen (destination, "a");
   }
   if (f)
   {
      struct tm tsparts;
      char ts8601[EDGEX_TSIZE];
      const char *crlid = edgex_device_get_crlid ();
      gmtime_r (&timestamp, &tsparts);
      strftime (ts8601, EDGEX_TSIZE, "%FT%TZ", &tsparts);
      fprintf
      (
        f,
        "level=%s ts=%s app=%s%s%s msg=%s\n",
        iot_logger_levelname (l),
        ts8601,
        subsystem ? subsystem : "(default)",
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
      return true;
   }
   return false;
}
