/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "edgex/edgex_logging.h"

#include <stdio.h>
#include <string.h>
#include <alloca.h>
#include <stdlib.h>
#include <pthread.h>

#include "errorlist.h"
#include "parson.h"
#include "rest.h"

static const char *levelstrs[] = {"INFO", "TRACE", "DEBUG", "WARNING", "ERROR"};

bool edgex_log_torest
(
  const char *destination,
  const char *subsystem,
  iot_loglevel l,
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
  json_object_set_string (jobj, "logLevel", levelstrs[l]);
  json_object_set_number (jobj, "created", timestamp);
  json_object_set_string (jobj, "message", message);

  json = json_serialize_to_string (jval);
  retcode = edgex_http_post
    (iot_log_default, &ctx, destination, json, NULL, &err);
  json_free_serialized_string (json);

  json_value_free (jval);

  return (retcode == 202 && err.code == EDGEX_OK.code);
}
