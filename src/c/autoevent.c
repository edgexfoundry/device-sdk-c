/*
 * Copyright (c) 2019
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
  atomic_uint_fast32_t refs;
  bool onChange;
} edgex_autoimpl;

struct sfxstruct
{
  const char *str;
  uint64_t factor;
};

static struct sfxstruct suffixes[] =
  { { "ms", 1 }, { "s", 1000 }, { "m", 60000 }, { "h", 3600000 }, { NULL, 0 } };

static uint64_t parseTime (const char *spec)
{
  char *fend;
  uint64_t fnum = strtoul (spec, &fend, 10);
  for (int i = 0; suffixes[i].str; i++)
  {
    if (strcmp (fend, suffixes[i].str) == 0)
    {
      return fnum * suffixes[i].factor;
    }
  }
  return 0;
}

static void edgex_autoimpl_release (edgex_autoimpl *ai)
{
  if (atomic_fetch_add (&ai->refs, -1) == 1)
  {
    free (ai->device);
    devsdk_protocols_free (ai->protocols);
    devsdk_commandresult_free (ai->last, ai->resource->nreqs);
    free (ai);
  }
}

static void *ae_runner (void *p)
{
  edgex_autoimpl *ai = (edgex_autoimpl *)p;
  atomic_fetch_add (&ai->refs, 1);

  edgex_device *dev = edgex_devmap_device_byname (ai->svc->devices, ai->device);
  if (dev)
  {
    if (ai->svc->adminstate == LOCKED || dev->adminState == LOCKED || dev->operatingState == DOWN)
    {
      edgex_device_release (dev);
      edgex_autoimpl_release (ai);
      return NULL;
    }
    edgex_device_alloc_crlid (NULL);
    iot_log_info (ai->svc->logger, "AutoEvent: %s/%s", ai->device, ai->resource->name);
    devsdk_commandresult *results = calloc (ai->resource->nreqs, sizeof (devsdk_commandresult));
    iot_data_t *exc = NULL;
    if
    (
      ai->svc->userfns.gethandler
        (ai->svc->userdata, dev->name, (devsdk_protocols *)dev->protocols, ai->resource->nreqs, ai->resource->reqs, results, NULL, &exc)
    )
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
          edgex_data_process_event (dev->name, ai->resource, results, ai->svc->config.device.datatransform);
        if (event)
        {
          edgex_data_client_add_event (ai->svc->dataclient, event);
          if (ai->onChange)
          {
            devsdk_commandresult_free (ai->last, ai->resource->nreqs);
            ai->last = resdup;
            resdup = NULL;
          }
          if (ai->svc->config.device.updatelastconnected)
          {
            edgex_metadata_client_update_lastconnected (ai->svc->logger, &ai->svc->config.endpoints, dev->name, &err);
          }
        }
        else
        {
          iot_log_error (ai->svc->logger, "Assertion failed for device %s. Disabling.", dev->name);
          edgex_metadata_client_set_device_opstate
            (ai->svc->logger, &ai->svc->config.endpoints, dev->name, DOWN, &err);
        }
      }
      devsdk_commandresult_free (resdup, ai->resource->nreqs);
    }
    else
    {
      iot_log_error (ai->svc->logger, "AutoEvent: Driver for %s failed on GET", dev->name);
    }
    iot_data_free (exc);
    devsdk_commandresult_free (results, ai->resource->nreqs);
    edgex_device_free_crlid ();
    edgex_device_release (dev);
  }
  else
  {
    iot_log_error
      (ai->svc->logger, "Autoevent fired for unknown device %s", ai->device);
    iot_schedule_remove (ai->svc->scheduler, ai->handle);
  }
  edgex_autoimpl_release (ai);
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
      const edgex_cmdinfo *cmd = edgex_deviceprofile_findcommand
        (ae->resource, dev->profile, true);
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
      uint64_t interval = parseTime (ae->interval);
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
      atomic_store (&ae->impl->refs, 1);
      ae->impl->onChange = ae->onChange;
    }
    if (ae->impl->svc->userfns.ae_starter)
    {
      iot_threadpool_add_work (svc->thpool, starter, ae->impl, -1);
    }
    else
    {
      ae->impl->handle = iot_schedule_create
        (svc->scheduler, ae_runner, NULL, ae->impl, IOT_MS_TO_NS(ae->impl->interval), 0, 0, svc->thpool, -1);
      iot_schedule_add (ae->impl->svc->scheduler, ae->impl->handle);
    }
  }
}

static void *stopper (void *p)
{
  edgex_autoimpl *ai = (edgex_autoimpl *)p;
  if (ai->svc->userfns.ae_stopper)
  {
    ai->svc->userfns.ae_stopper (ai->svc->userdata, ai->handle);
  }
  else
  {
    iot_schedule_delete (ai->svc->scheduler, ai->handle);
  }
  edgex_autoimpl_release (ai);
  return NULL;
}

void edgex_device_autoevent_stop (edgex_device *dev)
{
  for (edgex_device_autoevents *ae = dev->autos; ae; ae = ae->next)
  {
    if (ae->impl)
    {
      edgex_autoimpl *ai = ae->impl;
      iot_threadpool_add_work (ai->svc->thpool, stopper, ai, -1);
    }
  }
}

void edgex_device_autoevent_stop_now (edgex_device *dev)
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
