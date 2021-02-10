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
  void *userdata;
  pthread_mutex_t lock;
  uint64_t interval;
} edgex_device_periodic_discovery_t;

static void *edgex_device_handler_do_discovery (void *p)
{
  edgex_device_periodic_discovery_t *disc = (edgex_device_periodic_discovery_t *) p;

  pthread_mutex_lock (&disc->lock);
  disc->discfn (disc->userdata);
  pthread_mutex_unlock (&disc->lock);
  return NULL;
}

static void *edgex_device_periodic_discovery (void *p)
{
  edgex_device_periodic_discovery_t *disc = (edgex_device_periodic_discovery_t *) p;

  if (pthread_mutex_trylock (&disc->lock) == 0)
  {
    iot_log_info (disc->logger, "Running periodic discovery");
    disc->discfn (disc->userdata);
    pthread_mutex_unlock (&disc->lock);
  }
  else
  {
    iot_log_info (disc->logger, "Periodic discovery skipped: discovery already running");
  }
  return NULL;
}

edgex_device_periodic_discovery_t *edgex_device_periodic_discovery_alloc
  (iot_logger_t *logger, iot_scheduler_t *sched, iot_threadpool_t *pool, devsdk_discover discfn, void *userdata)
{
  edgex_device_periodic_discovery_t *result = calloc (1, sizeof (edgex_device_periodic_discovery_t));
  result->logger = logger;
  result->scheduler = sched;
  result->pool = pool;
  result->discfn = discfn;
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

void edgex_device_handler_discovery (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;
  char *ret;

  if (svc->userfns.discover == NULL)
  {
    ret = strdup ("Dynamic discovery is not implemented in this device service\n");
    reply->code = MHD_HTTP_NOT_IMPLEMENTED;
  }
  else if (svc->adminstate == LOCKED)
  {
    ret = strdup ("Device service is administratively locked\n");
    reply->code = MHD_HTTP_LOCKED;
  }
  else if (svc->opstate == DISABLED)
  {
    ret = strdup ("Device service is disabled\n");
    reply->code = MHD_HTTP_LOCKED;
  }
  else if (!svc->config.device.discovery_enabled)
  {
    ret = strdup ("Discovery disabled by configuration\n");
    reply->code = MHD_HTTP_SERVICE_UNAVAILABLE;
  }
  else if (pthread_mutex_trylock (&svc->discovery->lock) == 0)
  {
    iot_threadpool_add_work (svc->thpool, edgex_device_handler_do_discovery, svc->discovery, -1);
    pthread_mutex_unlock (&svc->discovery->lock);
    ret = strdup ("Running discovery\n");
    reply->code = MHD_HTTP_ACCEPTED;
  }
  else
  {
    ret = strdup ("Discovery already running; ignoring new request\n");
    reply->code = MHD_HTTP_ACCEPTED;
  }

  reply->data.bytes = ret;
  reply->data.size = strlen (ret);
  reply->content_type = CONTENT_PLAINTEXT;
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
  else if (svc->opstate == DISABLED)
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_LOCKED, "Device service is disabled");
  }
  else if (!svc->config.device.discovery_enabled)
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_SERVICE_UNAVAILABLE, "Discovery disabled by configuration");
  }
  else
  {
    if (pthread_mutex_trylock (&svc->discovery->lock) == 0)
    {
      iot_threadpool_add_work (svc->thpool, edgex_device_handler_do_discovery, svc->discovery, -1);
      pthread_mutex_unlock (&svc->discovery->lock);
      reply->data.bytes = strdup ("Running discovery\n");
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
