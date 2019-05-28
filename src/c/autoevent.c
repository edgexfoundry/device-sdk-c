/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "autoevent.h"
#include "errorlist.h"
#include "edgex/eventgen.h"
#include "edgex/edgex.h"
#include "edgex/edgex-logging.h"
#include "device.h"
#include "parson.h"
#include "edgex-rest.h"
#include "correlation.h"
#include "metadata.h"

#include <microhttpd.h>

typedef struct edgex_autoimpl
{
  edgex_device_service *svc;
  edgex_device_commandresult *last;
  uint64_t interval;
  const edgex_cmdinfo *resource;
  char *device;
  edgex_protocols *protocols;
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
    edgex_protocols_free (ai->protocols);
    edgex_device_commandresult_free (ai->last, ai->resource->nreqs);
    free (ai);
  }
}

static void ae_runner (void *p)
{
  edgex_autoimpl *ai = (edgex_autoimpl *)p;
  atomic_fetch_add (&ai->refs, 1);

  edgex_device *dev = edgex_devmap_device_byname (ai->svc->devices, ai->device);
  if (dev)
  {
    if (dev->adminState == LOCKED || dev->operatingState == DISABLED)
    {
      edgex_device_release (dev);
      return;
    }
    edgex_device_alloc_crlid (NULL);
    iot_log_info (ai->svc->logger, "AutoEvent: %s/%s", ai->device, ai->resource->name);
    edgex_device_commandresult *results = calloc (ai->resource->nreqs, sizeof (edgex_device_commandresult));
    if
    (
      ai->svc->userfns.gethandler
        (ai->svc->userdata, dev->name, dev->protocols, ai->resource->nreqs, ai->resource->reqs, results)
    )
    {
      edgex_device_commandresult *resdup = NULL;
      if (!(ai->onChange && ai->last && edgex_device_commandresult_equal (results, ai->last, ai->resource->nreqs)))
      {
        edgex_error err = EDGEX_OK;
        if (ai->onChange)
        {
          resdup = edgex_device_commandresult_dup (results, ai->resource->nreqs);
        }
        edgex_event_cooked *event =
          edgex_data_process_event (dev->name, ai->resource, results, ai->svc->config.device.datatransform);
        if (event)
        {
          edgex_data_client_add_event (ai->svc->logger, &ai->svc->config.endpoints, event, &err);
          if (err.code == 0)
          {
            if (ai->onChange)
            {
              edgex_device_commandresult_free (ai->last, ai->resource->nreqs);
              ai->last = resdup;
              resdup = NULL;
            }
          }
          else
          {
            iot_log_error (ai->svc->logger, "AutoEvent: unable to push new event");
          }
          edgex_event_cooked_free (event);
        }
        else
        {
          iot_log_error (ai->svc->logger, "Assertion failed for device %s. Disabling.", dev->name);
          edgex_metadata_client_set_device_opstate
            (ai->svc->logger, &ai->svc->config.endpoints, dev->id, DISABLED, &err);
        }
      }
      edgex_device_commandresult_free (resdup, ai->resource->nreqs);
    }
    else
    {
      iot_log_error (ai->svc->logger, "AutoEvent: Driver for %s failed on GET", dev->name);
    }
    edgex_device_commandresult_free (results, ai->resource->nreqs);
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
}

static void starter (void *p)
{
  edgex_autoimpl *ai = (edgex_autoimpl *)p;
  ai->handle = ai->svc->autoevstart
  (
    ai->svc->userdata, ai->device, ai->protocols, ai->resource->name,
    ai->resource->nreqs, ai->resource->reqs, ai->interval, ai->onChange
  );
}

void edgex_device_autoevent_start (edgex_device_service *svc, edgex_device *dev)
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
      uint64_t interval = parseTime (ae->frequency);
      if (interval == 0)
      {
        iot_log_error
        (
          svc->logger,
          "AutoEvents: device %s: unable to parse %s for frequency.",
          dev->name, ae->frequency
        );
        continue;
      }
      ae->impl = malloc (sizeof (edgex_autoimpl));
      ae->impl->svc = svc;
      ae->impl->last = NULL;
      ae->impl->interval = interval;
      ae->impl->resource = cmd;
      ae->impl->device = strdup (dev->name);
      ae->impl->protocols = edgex_protocols_dup (dev->protocols);
      ae->impl->handle = NULL;
      atomic_store (&ae->impl->refs, 1);
      ae->impl->onChange = ae->onChange;
    }
    if (ae->impl->svc->autoevstart)
    {
      iot_threadpool_add_work (svc->thpool, starter, ae->impl, NULL);
    }
    else
    {
      ae->impl->handle = iot_schedule_create
        (svc->scheduler, ae_runner, ae->impl, IOT_MS_TO_NS(ae->impl->interval), 0, 0, NULL);
      iot_schedule_add (ae->impl->svc->scheduler, ae->impl->handle);
    }
  }
}

static void stopper (void *p)
{
  edgex_autoimpl *ai = (edgex_autoimpl *)p;
  if (ai->svc->autoevstop)
  {
    ai->svc->autoevstop (ai->svc->userdata, ai->handle);
  }
  else
  {
    iot_schedule_delete (ai->svc->scheduler, ai->handle);
  }
  edgex_autoimpl_release (ai);
}

void edgex_device_autoevent_stop (edgex_device *dev)
{
  for (edgex_device_autoevents *ae = dev->autos; ae; ae = ae->next)
  {
    edgex_autoimpl *ai = ae->impl;
    iot_threadpool_add_work (ai->svc->thpool, stopper, ai, NULL);
  }
}

void edgex_device_register_autoevent_handlers
(
  edgex_device_service *svc,
  edgex_device_autoevent_start_handler starter,
  edgex_device_autoevent_stop_handler stopper
)
{
  if (svc->logger)
  {
    iot_log_error
      (svc->logger, "AutoEvents: must register handlers before service start.");
    return;
  }

  if ((starter == NULL) || (stopper == NULL))
  {
    iot_log_error
      (iot_log_default (), "AutoEvent registration: must specify both handlers");
    return;
  }
  svc->autoevstart = starter;
  svc->autoevstop = stopper;
}
