/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "edgex_rest.h"
#include "rest.h"
#include "data.h"
#include "errorlist.h"
#include "config.h"

void edgex_data_client_add_event
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  char *device,
  uint64_t origin,
  edgex_reading *readings,
  edgex_error *err
)
{
  edgex_event result;
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  char *json;

  memset (&result, 0, sizeof (edgex_event));
  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/event",
    endpoints->data.host,
    endpoints->data.port
  );
  result.device = device;
  result.origin = origin;
  result.readings = readings;

  json = edgex_event_write (&result, true);
  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);

  free (ctx.buff);
  free (json);
}

edgex_valuedescriptor *edgex_data_client_add_valuedescriptor
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const char *name,
  uint64_t origin,
  const char *min,
  const char *max,
  const char *type,
  const char *uomLabel,
  const char *defaultValue,
  const char *formatting,
  const char *description,
  edgex_error *err
)
{
  edgex_valuedescriptor *result = malloc (sizeof (edgex_valuedescriptor));
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  char *json;

  memset (result, 0, sizeof (edgex_valuedescriptor));
  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/valuedescriptor",
    endpoints->data.host,
    endpoints->data.port
  );
  result->origin = origin;
  result->name = strdup (name);
  result->min = strdup (min);
  result->max = strdup (max);
  result->type = strdup (type);
  result->uomLabel = strdup (uomLabel);
  result->defaultValue = strdup (defaultValue);
  result->formatting = strdup (formatting);
  result->description = strdup (description);
  json = edgex_valuedescriptor_write (result);
  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);
  result->id = ctx.buff;
  free (json);

  return result;
}

bool edgex_data_client_ping
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  edgex_error *err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/ping",
    endpoints->data.host,
    endpoints->data.port
  );

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);
  free (ctx.buff);

  return (err->code == 0);
}
