/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "discovery.h"
#include "service.h"
#include "profiles.h"
#include "metadata.h"
#include "edgex-rest.h"
#include "correlation.h"

#include <string.h>
#include <stdlib.h>

#include <microhttpd.h>

typedef struct edgex_device_periodic_discovery_t
{
  iot_logger_t *logger;
  iot_scheduler_t *scheduler;
  iot_schedule_t *schedule;
  iot_threadpool_t *pool;
  devsdk_discover discfn;
  devsdk_discovery_delete disc_delete_fn;
  void *userdata;
  pthread_mutex_t lock;
  uint64_t interval;
  char * request_id;
} edgex_device_periodic_discovery_t;

static void *edgex_device_handler_do_discovery (void *p)
{
  edgex_device_periodic_discovery_t *disc = (edgex_device_periodic_discovery_t *) p;

  pthread_mutex_lock (&disc->lock);
  disc->discfn (disc->userdata, disc->request_id);
  pthread_mutex_unlock (&disc->lock);
  return NULL;
}

static void *edgex_device_periodic_discovery (void *p)
{
  edgex_device_periodic_discovery_t *disc = (edgex_device_periodic_discovery_t *) p;

  if (pthread_mutex_trylock (&disc->lock) == 0)
  {
    iot_log_info (disc->logger, "Running periodic discovery");
    disc->discfn (disc->userdata, disc->request_id);
    pthread_mutex_unlock (&disc->lock);
  }
  else
  {
    iot_log_info (disc->logger, "Periodic discovery skipped: discovery already running");
  }
  return NULL;
}

edgex_device_periodic_discovery_t *edgex_device_periodic_discovery_alloc
  (iot_logger_t *logger, iot_scheduler_t *sched, iot_threadpool_t *pool, devsdk_discover discfn, devsdk_discovery_delete disc_delete_fn, void *userdata)
{
  edgex_device_periodic_discovery_t *result = calloc (1, sizeof (edgex_device_periodic_discovery_t));
  result->logger = logger;
  result->scheduler = sched;
  result->pool = pool;
  result->discfn = discfn;
  result->disc_delete_fn = disc_delete_fn;
  result->userdata = userdata;
  pthread_mutex_init (&result->lock, NULL);
  return result;
}

void edgex_device_periodic_discovery_configure (edgex_device_periodic_discovery_t *disc, bool enabled, uint64_t interval)
{
  if (disc->schedule)
  {
    if (enabled && interval)
    {
      if (interval != disc->interval)
      {
        iot_schedule_delete (disc->scheduler, disc->schedule);
        disc->interval = interval;
        disc->schedule = iot_schedule_create (disc->scheduler, edgex_device_periodic_discovery, NULL, disc, IOT_SEC_TO_NS(interval), 0, 0, disc->pool, -1);
        iot_schedule_add (disc->scheduler, disc->schedule);
      }
    }
    else
    {
      iot_schedule_delete (disc->scheduler, disc->schedule);
      disc->schedule = NULL;
    }
  }
  else
  {
    if (enabled && interval)
    {
      if (disc->discfn)
      {
        disc->interval = interval;
        disc->schedule = iot_schedule_create (disc->scheduler, edgex_device_periodic_discovery, NULL, disc, IOT_SEC_TO_NS(interval), 0, 0, disc->pool, -1);
        iot_schedule_add (disc->scheduler, disc->schedule);
      }
      else
      {
        iot_log_error (disc->logger, "Discovery enabled in configuration but not supported by this device service");
      }
    }
  }
}

void edgex_device_periodic_discovery_stop (edgex_device_periodic_discovery_t *disc)
{
  if (disc->schedule)
  {
    iot_schedule_delete (disc->scheduler, disc->schedule);
    disc->schedule = NULL;
  }
}

void edgex_device_periodic_discovery_free (edgex_device_periodic_discovery_t *disc)
{
  if (disc)
  {
    edgex_device_periodic_discovery_stop (disc);
    pthread_mutex_destroy (&disc->lock);
    free (disc);
  }
}

void edgex_device_handler_discoveryv2 (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  if (svc->userfns.discover == NULL)
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_NOT_IMPLEMENTED, "Dynamic discovery is not implemented in this device service");
  }
  else if (svc->adminstate == LOCKED)
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_LOCKED, "Device service is administratively locked");
  }
  else if (!svc->config.device.discovery_enabled)
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_SERVICE_UNAVAILABLE, "Discovery disabled by configuration");
  }
  else
  {
    if (pthread_mutex_trylock (&svc->discovery->lock) == 0)
    {
      free (svc->discovery->request_id);
      svc->discovery->request_id = strdup (edgex_device_get_crlid());
      iot_threadpool_add_work (svc->thpool, edgex_device_handler_do_discovery, svc->discovery, -1);
      pthread_mutex_unlock (&svc->discovery->lock);
      reply->data.bytes = strdup (svc->discovery->request_id);
    }
    else
    {
      reply->data.bytes = strdup ("Discovery already running; ignoring new request\n");
    }

    reply->code = MHD_HTTP_ACCEPTED;
    reply->data.size = strlen (reply->data.bytes);
    reply->content_type = CONTENT_PLAINTEXT;
  }
}

static edgex_baseresponse *edgex_disc_delete_response_create (uint64_t code, char *msg, const char * req_id)
{
  edgex_baseresponse *res = malloc (sizeof (edgex_baseresponse));
  res->statusCode = code;
  if (msg) res->message = msg;
  res->requestId = req_id;
  res->apiVersion = "v3";
  return res;
}

void edgex_device_handler_discovery_cancel (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;
  const char * req_id = devsdk_nvpairs_value (req->params, "requestId");
  edgex_baseresponse * resp = NULL;
  int err = false;

  if(!svc->discovery->request_id || !req_id ||strcmp (req_id, svc->discovery->request_id) != 0)
  {
    resp = edgex_disc_delete_response_create (MHD_HTTP_NOT_FOUND, "Not Found", req_id);
    err = true;
  }
  else if (svc->userfns.discovery_delete == NULL)
  {
    resp = edgex_disc_delete_response_create (MHD_HTTP_NOT_IMPLEMENTED, "Discovery Cancel is not implemented in this device service", req_id);
    err = true;
  }
  else if (svc->adminstate == LOCKED)
  {
    resp = edgex_disc_delete_response_create (MHD_HTTP_LOCKED, "Device service is administratively locked", req_id);
    err = true;
  }
  else if (!svc->config.device.discovery_enabled)
  {
    resp = edgex_disc_delete_response_create (MHD_HTTP_SERVICE_UNAVAILABLE, "Discovery disabled by configuration", req_id);
    err = true;
  }
  else
  {
    if (!svc->discovery->disc_delete_fn (svc->discovery->userdata, req_id))
    {
      resp = edgex_disc_delete_response_create (MHD_HTTP_INTERNAL_SERVER_ERROR, "Internal Server Error", req_id);
      err = true;
    }
    else
    {
      resp = edgex_disc_delete_response_create (MHD_HTTP_OK, NULL, req_id);
    }
  }

  if (err)
  {
    edgex_errorresponse_write (resp, reply);
  }
  else
  {
    edgex_baseresponse_write(resp, reply);
  }
  free (resp);
}
