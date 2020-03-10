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

int edgex_device_handler_discovery
(
  void *ctx,
  char *url,
  const devsdk_nvpairs *qparams,
  edgex_http_method method,
  const char *upload_data,
  size_t upload_data_size,
  void **reply,
  size_t *reply_size,
  const char **reply_type
)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;
  int retcode;

  if (svc->userfns.discover == NULL)
  {
    *reply = strdup ("Dynamic discovery is not implemented in this device service\n");
    retcode = MHD_HTTP_NOT_IMPLEMENTED;
  }
  else if (svc->adminstate == LOCKED)
  {
    *reply = strdup ("Device service is administratively locked\n");
    retcode = MHD_HTTP_LOCKED;
  }
  else if (svc->opstate == DISABLED)
  {
    *reply = strdup ("Device service is disabled\n");
    retcode = MHD_HTTP_LOCKED;
  }
  else if (!svc->config.device.discovery_enabled)
  {
    *reply = strdup ("Discovery disabled by configuration\n");
    retcode = MHD_HTTP_SERVICE_UNAVAILABLE;
  }
  else if (pthread_mutex_trylock (&svc->discolock) == 0)
  {
    iot_threadpool_add_work (svc->thpool, edgex_device_handler_do_discovery, svc, -1);
    pthread_mutex_unlock (&svc->discolock);
    *reply = strdup ("Running discovery\n");
    retcode = MHD_HTTP_ACCEPTED;
  }
  else
  {
    *reply = strdup ("Discovery already running; ignoring new request\n");
    retcode = MHD_HTTP_ACCEPTED;
  }
  *reply_size = strlen (*reply);
  return retcode;
}
