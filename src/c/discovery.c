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

static void *edgex_device_handler_do_discovery (void *p)
{
  devsdk_service_t *svc = (devsdk_service_t *) p;

  pthread_mutex_lock (&svc->discolock);
  svc->userfns.discover (svc->userdata);
  pthread_mutex_unlock (&svc->discolock);
  return NULL;
}

void *edgex_device_periodic_discovery (void *s)
{
  devsdk_service_t *svc = (devsdk_service_t *) s;

  if (svc->adminstate == LOCKED)
  {
    iot_log_error (svc->logger, "Periodic discovery not running: device service locked");
    return NULL;
  }
  if (svc->opstate == DISABLED)
  {
    iot_log_error (svc->logger, "Periodic discovery not running: device service disabled");
    return NULL;
  }

  if (pthread_mutex_trylock (&svc->discolock) == 0)
  {
    iot_log_info (svc->logger, "Running periodic discovery");
    svc->userfns.discover (svc->userdata);
    pthread_mutex_unlock (&svc->discolock);
  }
  else
  {
    iot_log_info (svc->logger, "Periodic discovery skipped: discovery already running");
  }
  return NULL;
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
  else if (pthread_mutex_trylock (&svc->discolock) == 0)
  {
    iot_threadpool_add_work (svc->thpool, edgex_device_handler_do_discovery, svc, -1);
    pthread_mutex_unlock (&svc->discolock);
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
    if (pthread_mutex_trylock (&svc->discolock) == 0)
    {
      iot_threadpool_add_work (svc->thpool, edgex_device_handler_do_discovery, svc, -1);
      pthread_mutex_unlock (&svc->discolock);
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
