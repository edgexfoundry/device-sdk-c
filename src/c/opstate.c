/*
 * Copyright (c) 2024
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "opstate.h"
#include "metadata.h"
#include "service.h"
#include "errorlist.h"
#include "cmdinfo.h"

#include <iot/thread.h>

typedef struct devsdk_devret_param_t
{
  devsdk_service_t *svc;
  iot_data_t *name;
  iot_schedule_t *self;
} devsdk_devret_param_t;

static void *devsdk_device_return (void *p);

void devsdk_set_device_opstate (devsdk_service_t *svc, const char *devname, bool operational, devsdk_error *err)
{
  *err = EDGEX_OK;
  edgex_metadata_client_set_device_opstate
  (
    svc->logger,
    &svc->config.endpoints,
    svc->secretstore,
    devname,
    operational ? UP : DOWN,
    err
  );
  if (err->code)
  {
    iot_log_error (svc->logger, "Unable to change operational state for device %s", devname);
  }
}

static void devsdk_devret_param_free (void *p)
{
  devsdk_devret_param_t *param = (devsdk_devret_param_t *)p;
  iot_data_free (param->name);
  free (param);
}

static void devsdk_create_return_schedule (devsdk_service_t *svc, iot_data_t *name, uint64_t wait)
{
  devsdk_devret_param_t *p = malloc (sizeof (devsdk_devret_param_t));
  p->name = name;
  p->svc = svc;
  p->self = iot_schedule_create (svc->scheduler, devsdk_device_return, devsdk_devret_param_free, p, wait, wait, 1, svc->thpool, IOT_THREAD_NO_PRIORITY);
  iot_schedule_add_abort_callback (svc->scheduler, p->self, devsdk_device_return);
  iot_schedule_add (svc->scheduler, p->self);
}

static void *devsdk_device_return (void *p)
{
  devsdk_devret_param_t *param = (devsdk_devret_param_t *)p;
  const char *name = iot_data_string (param->name);
  iot_log_debug (param->svc->logger, "Down-timeout for device %s", name);
  edgex_device *dev = edgex_devmap_device_byname (param->svc->devices, name);
  if (dev)
  {
    if (dev->operatingState == UP)
    {
      iot_log_debug (param->svc->logger, "Device %s already back up", name);
    }
    else
    {
      edgex_cmdinfo *cmd = dev->profile->cmdinfo;
      while (cmd && !cmd->isget && cmd->nreqs > 1)
      {
        cmd = cmd->next;
      }
      if (cmd)
      {
        iot_data_t *e = NULL;
        devsdk_commandresult result = { 0 };
        if (param->svc->userfns.gethandler (param->svc->userdata, dev->devimpl, 1, cmd->reqs, &result, NULL, NULL, &e))
        {
          devsdk_error err;
          iot_log_debug (param->svc->logger, "Device %s responsive: setting operational state to up", name);
          devsdk_set_device_opstate (param->svc, name, true, &err);
        }
        else
        {
          uint64_t wait = IOT_SEC_TO_NS (param->svc->config.device.dev_downtime);
          iot_log_debug (param->svc->logger, "Device %s still unresponsive", name);
          devsdk_create_return_schedule (param->svc, iot_data_add_ref (param->name), wait);
        }
      }
      else
      {
        iot_log_error (param->svc->logger, "Device %s has no readable resources, cannot be set operational automatically", name);
      }
    }
    edgex_device_release (param->svc, dev);
  }
  else
  {
    iot_log_debug (param->svc->logger, "Device %s not found", name);
  }
  iot_schedule_delete (param->svc->scheduler, param->self);
  return NULL;
}

void devsdk_device_request_failed (devsdk_service_t *svc, edgex_device *dev)
{
  if (svc->config.device.allowed_fails && --dev->retries == 0)
  {
    devsdk_error err;
    iot_log_warn (svc->logger, "Marking device %s non-operational", dev->name);
    devsdk_set_device_opstate (svc, dev->name, false, &err);
    if (svc->config.device.dev_downtime)
    {
      uint64_t wait = IOT_SEC_TO_NS (svc->config.device.dev_downtime);
      iot_log_warn (svc->logger, "Will retry device %s in %" PRIu64 " seconds", dev->name, svc->config.device.dev_downtime);
      devsdk_create_return_schedule (svc, iot_data_alloc_string (dev->name, IOT_DATA_COPY), wait);
    }
  }
}

void devsdk_device_request_succeeded (devsdk_service_t *svc, edgex_device *dev)
{
  if (svc->config.device.allowed_fails)
  {
    dev->retries = svc->config.device.allowed_fails;
    if (dev->operatingState == DOWN)
    {
      devsdk_error err;
      devsdk_set_device_opstate (svc, dev->name, true, &err);
    }
  }
}
