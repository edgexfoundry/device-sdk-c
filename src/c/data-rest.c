/*
 * Copyright (c) 2021
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "data-rest.h"
#include "rest.h"
#include "errorlist.h"

static void edc_rest_postfn (iot_logger_t *lc, void *address, edgex_event_cooked *event)
{
  edgex_ctx ctx;
  const char *baseurl = (const char *)address;
  devsdk_error err = EDGEX_OK;

  memset (&ctx, 0, sizeof (edgex_ctx));

  char *url = malloc (strlen (baseurl) + strlen (event->path) + 1);
  strcpy (url, baseurl);
  strcat (url, event->path);
  switch (event->encoding)
  {
    case JSON:
    {
      edgex_http_post (lc, &ctx, url, event->value.json, NULL, &err);
      break;
    }
    case CBOR:
    {
      edgex_http_postbin (lc, &ctx, url, event->value.cbor.data, event->value.cbor.length, CONTENT_CBOR, NULL, &err);
      break;
    }
  }
  edgex_event_cooked_free (event);
  free (url);
}

static void edc_rest_freefn (iot_logger_t *lc, void *address)
{
  free (address);
}

edgex_data_client_t *edgex_data_client_new_rest (const edgex_device_service_endpoint *e, iot_logger_t *lc, iot_threadpool_t *queue)
{
  edgex_data_client_t *result = malloc (sizeof (edgex_data_client_t));
  result->lc = lc;
  result->queue = queue;
  result->pf = edc_rest_postfn;
  result->ff = edc_rest_freefn;
  result->mf = NULL;
  char *url = malloc (URL_BUF_SIZE);
  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/v2/event/", e->host, e->port);
  result->address = url;
  iot_log_info (lc, "Event data will be posted to core-data at %s<profile>/<device>/<source>", url);
  return result;
}
