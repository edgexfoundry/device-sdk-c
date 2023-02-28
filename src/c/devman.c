/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "service.h"
#include "profiles.h"
#include "metadata.h"
#include "edgex-rest.h"
#include "errorlist.h"
#include "devutil.h"
#include "edgex/devices.h"
#include "edgex/profiles.h"

#include <string.h>
#include <stdlib.h>

/* Device (collection) management functions */

void devsdk_free_resources (devsdk_device_resources *r)
{
  while (r)
  {
    free (r->resname);
    iot_data_free (r->attributes);
    devsdk_device_resources *nextr = r->next;
    free (r);
    r = nextr;
  }
}

void devsdk_free_devices (devsdk_service_t *svc, devsdk_devices *d)
{
  while (d)
  {
    free (d->device->name);
    svc->userfns.free_addr (svc->userdata, d->device->address);
    free (d->device);
    devsdk_free_resources (d->resources);
    devsdk_devices *nextd = d->next;
    free (d);
    d = nextd;
  }
}

void edgex_add_device
(
  devsdk_service_t *svc,
  const char *name,
  const char *description,
  const devsdk_strings *labels,
  const char *profile_name,
  devsdk_protocols *protocols,
  bool locked,
  edgex_device_autoevents *autos,
  devsdk_error *err
)
{
  edgex_device *existing;
  char *result;
  *err = EDGEX_OK;

  existing = edgex_devmap_device_byname (svc->devices, name);

  if (existing)
  {
    iot_log_info (svc->logger, "Device %s already present", name);
    edgex_device_release (svc, existing);
    return;
  }

  result = edgex_metadata_client_add_device
  (
    svc->logger,
    &svc->config.endpoints,
    name,
    description,
    labels,
    locked ? LOCKED : UNLOCKED,
    protocols,
    autos,
    svc->name,
    profile_name,
    err
  );

  if (result)
  {
    devsdk_add_new_device (device_latest, name);
    iot_log_info (svc->logger, "Device %s added with id %s", name, result);
    free (result);
  }
  else
  {
    iot_log_error
      (svc->logger, "Failed to add Device in core-metadata: %s", err->reason);
  }
}

devsdk_devices *devsdk_get_devices (devsdk_service_t *svc)
{
  return edgex_devmap_copydevices_generic (svc->devices);
}

devsdk_devices *devsdk_get_device (devsdk_service_t *svc, const char *name)
{
  edgex_device *internal;
  devsdk_devices *result = NULL;

  internal = edgex_devmap_device_byname (svc->devices, name);
  if (internal)
  {
    result = edgex_device_todevsdk (svc, internal);
    edgex_device_release (svc, internal);
  }
  return result;
}

edgex_device * edgex_devices (devsdk_service_t *svc)
{
  return edgex_devmap_copydevices (svc->devices);
}

edgex_device * edgex_get_device_byname (devsdk_service_t *svc, const char *name)
{
  edgex_device *internal;
  edgex_device *result = NULL;

  internal = edgex_devmap_device_byname (svc->devices, name);
  if (internal)
  {
    result = edgex_device_dup (internal);
    edgex_device_release (svc, internal);
  }
  return result;
}

void edgex_remove_device_byname (devsdk_service_t *svc, const char *name, devsdk_error *err)
{
  *err = EDGEX_OK;
  edgex_metadata_client_delete_device_byname
    (svc->logger, &svc->config.endpoints, name, err);
  if (err->code != 0)
  {
    iot_log_error
      (svc->logger, "Unable to remove device %s from metadata", name);
  }
}

void edgex_update_device
(
  devsdk_service_t *svc,
  const char *name,
  const char *description,
  const devsdk_strings *labels,
  const char *profile_name,
  devsdk_error *err
)
{
  *err = EDGEX_OK;
  edgex_metadata_client_update_device
  (
    svc->logger,
    &svc->config.endpoints,
    name,
    description,
    labels,
    profile_name,
    err
  );
  if (err->code)
  {
    iot_log_error (svc->logger, "Unable to update device %s", name);
  }
}

void edgex_free_device (devsdk_service_t *svc, edgex_device *e)
{
  edgex_device_free (svc, e);
}

void devsdk_add_discovered_devices (devsdk_service_t *svc, uint32_t ndevices, devsdk_discovered_device *devices)
{
  edgex_device *existing;
  for (uint32_t i = 0; i < ndevices; i++)
  {
    existing = edgex_devmap_device_byname (svc->devices, devices[i].name);
    if (existing)
    {
      edgex_device_release (svc, existing);
      continue;
    }

    for (devsdk_protocols *prots = devices[i].protocols; prots; prots = prots->next)
    {
      edgex_watcher *w = edgex_watchlist_match (svc->watchlist, prots->properties);
      if (w)
      {
        devsdk_strings *labels = NULL;
        if (devices[i].properties)
        {
          const iot_data_t *ldata = iot_data_string_map_get (devices[i].properties, "Labels");
          if (ldata)
          {
            iot_data_vector_iter_t iter;
            iot_data_vector_iter (ldata, &iter);
            while (iot_data_vector_iter_next (&iter))
            {
              labels = devsdk_strings_new (iot_data_vector_iter_string (&iter), labels);
            }
          }
        }
        edgex_metadata_client_add_or_modify_device
        (
          svc->logger,
          &svc->config.endpoints,
          devices[i].name,
          devices[i].description,
          labels,
          w->adminstate,
          devices[i].protocols,
          w->autoevents,
          svc->name,
          w->profile
        );
        edgex_watcher_free (w);
        devsdk_strings_free (labels);
        break;
      }
    }
  }
}

void devsdk_set_device_opstate (devsdk_service_t *svc, char *devname, bool operational, devsdk_error *err)
{
  *err = EDGEX_OK;
  edgex_metadata_client_set_device_opstate
  (
    svc->logger,
    &svc->config.endpoints,
    devname,
    operational ? UP : DOWN,
    err
  );
  if (err->code)
  {
    iot_log_error (svc->logger, "Unable to change operational state for device %s", devname);
  }
}
