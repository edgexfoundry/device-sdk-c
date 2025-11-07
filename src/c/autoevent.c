/*
 * Copyright (c) 2019-2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "autoevent.h"
#include "errorlist.h"
#include "edgex/edgex.h"
#include "device.h"
#include "parson.h"
#include "edgex-rest.h"
#include "devutil.h"
#include "correlation.h"
#include "metadata.h"
#include "data.h"
#include "opstate.h"

#include <microhttpd.h>

typedef struct edgex_autoimpl
{
  devsdk_service_t *svc;
  devsdk_commandresult *last;
  uint64_t interval;
  const edgex_cmdinfo *resource;
  char *device;
  devsdk_protocols *protocols;
  void *handle;
  bool onChange;
} edgex_autoimpl;

static void edgex_autoimpl_release (void *p)
{
  edgex_autoimpl *ai = (edgex_autoimpl *)p;
  free (ai->device);
  devsdk_protocols_free (ai->protocols);
  devsdk_commandresult_free (ai->last, ai->resource->nreqs);
  free (ai);
}

static void *ae_runner (void *p)
{
  edgex_autoimpl *ai = (edgex_autoimpl *)p;

  edgex_device *dev = edgex_devmap_device_byname (ai->svc->devices, ai->device);
  if (dev)
  {
    if (ai->svc->adminstate == LOCKED || dev->adminState == LOCKED || dev->operatingState == DOWN)
    {
      edgex_device_release (ai->svc, dev);
      return NULL;
    }
    edgex_device_alloc_crlid (NULL);
    iot_log_info (ai->svc->logger, "AutoEvent: %s/%s", ai->device, ai->resource->name);
    devsdk_commandresult *results = calloc (ai->resource->nreqs, sizeof (devsdk_commandresult));
    iot_data_t *tags = NULL;
    iot_data_t *exc = NULL;
    if (dev->devimpl->address == NULL)
    {
      dev->devimpl->address = ai->svc->userfns.create_addr (ai->svc->userdata, dev->protocols, &exc);
    }
    if (dev->devimpl->address)
    {
      if (ai->svc->userfns.gethandler (ai->svc->userdata, dev->devimpl, ai->resource->nreqs, ai->resource->reqs, results, &tags, NULL, &exc))
      {
        devsdk_commandresult *resdup = NULL;
        if (!(ai->onChange && ai->last && devsdk_commandresult_equal (results, ai->last, ai->resource->nreqs)))
        {
          devsdk_error err = EDGEX_OK;
          if (ai->onChange)
          {
            resdup = devsdk_commandresult_dup (results, ai->resource->nreqs);
          }
          edgex_event_cooked *event =
            edgex_data_process_event (dev, ai->resource, results, tags, ai->svc->config.device.datatransform);
          if (event)
          {
            if (ai->svc->config.device.maxeventsize && edgex_event_cooked_size (event) > ai->svc->config.device.maxeventsize * 1024)
            {
              iot_log_error (ai->svc->logger, "Auto Event size (%zu KiB) exceeds configured MaxEventSize", edgex_event_cooked_size (event) / 1024);
            }
            else
            {
              edgex_data_client_add_event (ai->svc->msgbus, event, &ai->svc->metrics);
            }
            edgex_event_cooked_free (event);
            if (ai->onChange)
            {
              devsdk_commandresult_free (ai->last, ai->resource->nreqs);
              ai->last = resdup;
              resdup = NULL;
            }
            if (ai->svc->config.device.updatelastconnected)
            {
              edgex_metadata_client_update_lastconnected (ai->svc->logger, &ai->svc->config.endpoints, ai->svc->secretstore, dev->name, &err);
            }
            devsdk_device_request_succeeded (ai->svc, dev);
          }
          else
          {
            iot_log_error (ai->svc->logger, "Assertion failed for device %s. Disabling.", dev->name);
            edgex_metadata_client_set_device_opstate
              (ai->svc->logger, &ai->svc->config.endpoints, ai->svc->secretstore, dev->name, DOWN, &err);
          }
        }
        else
        {
          devsdk_device_request_succeeded (ai->svc, dev);
        }
        devsdk_commandresult_free (resdup, ai->resource->nreqs);
      }
      else
      {
        iot_log_error (ai->svc->logger, "AutoEvent: Driver for %s failed on GET", dev->name);
        devsdk_device_request_failed (ai->svc, dev);
      }
    }
    else
    {
      iot_log_error (ai->svc->logger, "AutoEvent: Address parsing for %s failed", dev->name);
    }
    if (exc)
    {
      char *errstr = iot_data_to_json (exc);
      iot_log_error (ai->svc->logger, "%s", errstr);
      iot_data_free (exc);
      free (errstr);
    }
    devsdk_commandresult_free (results, ai->resource->nreqs);
    edgex_device_free_crlid ();
    edgex_device_release (ai->svc, dev);
  }
  else
  {
    iot_log_error
      (ai->svc->logger, "Autoevent fired for unknown device %s", ai->device);
    if (ai->handle)
    {
      iot_schedule_remove (ai->svc->scheduler, ai->handle);
    }
  }
  return NULL;
}

static void *starter (void *p)
{
  edgex_autoimpl *ai = (edgex_autoimpl *)p;
  ai->handle = ai->svc->userfns.ae_starter
  (
    ai->svc->userdata, ai->device, ai->protocols, ai->resource->name,
    ai->resource->nreqs, ai->resource->reqs, ai->interval, ai->onChange
  );
  return NULL;
}

void edgex_device_autoevent_start (devsdk_service_t *svc, edgex_device *dev)
{
  for (edgex_device_autoevents *ae = dev->autos; ae; ae = ae->next)
  {
    if (ae->impl == NULL)
    {
      const edgex_cmdinfo *cmd = edgex_deviceprofile_findcommand (svc, ae->resource, dev->profile, true);
      if (cmd == NULL)
      {
        iot_log_error
        (
          svc->logger,
          "AutoEvents: device %s: no resource %s.",
          dev->name, ae->resource
        );
        continue;
      }
      uint64_t interval = edgex_parsetime (ae->interval);
      if (interval == 0)
      {
        iot_log_error
        (
          svc->logger,
          "AutoEvents: device %s: unable to parse %s for interval.",
          dev->name, ae->interval
        );
        continue;
      }
      ae->impl = malloc (sizeof (edgex_autoimpl));
      ae->impl->svc = svc;
      ae->impl->last = NULL;
      ae->impl->interval = interval;
      ae->impl->resource = cmd;
      ae->impl->device = strdup (dev->name);
      ae->impl->protocols = devsdk_protocols_dup ((const devsdk_protocols *)dev->protocols);
      ae->impl->handle = NULL;
      ae->impl->onChange = ae->onChange;
    }
    if (ae->impl->svc->userfns.ae_starter)
    {
      iot_threadpool_add_work (svc->thpool, starter, ae->impl, -1);
    }
    else
    {
      ae->impl->handle = iot_schedule_create
        (svc->scheduler, ae_runner, edgex_autoimpl_release, ae->impl, IOT_MS_TO_NS(ae->impl->interval), 0, 0, svc->thpool, -1);
      iot_schedule_add (ae->impl->svc->scheduler, ae->impl->handle);
    }
  }
}

static void stopper (edgex_autoimpl *ai)
{
  void *handle = ai->handle;
  ai->handle = NULL;
  if (ai->svc->userfns.ae_stopper)
  {
    ai->svc->userfns.ae_stopper (ai->svc->userdata, handle);
    edgex_autoimpl_release (ai);
  }
  else
  {
    iot_schedule_delete (ai->svc->scheduler, handle);
  }
}

void edgex_device_autoevent_stop (edgex_device *dev)
{
  for (edgex_device_autoevents *ae = dev->autos; ae; ae = ae->next)
  {
    if (ae->impl)
    {
      stopper (ae->impl);
      ae->impl = NULL;
    }
  }
}
