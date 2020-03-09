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

char * edgex_add_device
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
  char *result = NULL;

  *err = EDGEX_OK;

  existing = edgex_devmap_device_byname (svc->devices, name);

  if (existing)
  {
    iot_log_info (svc->logger, "Device %s already present", name);
    result = strdup (existing->id);
    edgex_device_release (existing);
    return result;
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
    iot_log_info (svc->logger, "Device %s added with id %s", name, result);
  }
  else
  {
    iot_log_error
      (svc->logger, "Failed to add Device in core-metadata: %s", err->reason);
  }
  return result;
}

edgex_device * edgex_devices (devsdk_service_t *svc)
{
  return edgex_devmap_copydevices (svc->devices);
}

edgex_device * edgex_get_device (devsdk_service_t *svc, const char *id)
{
  edgex_device *internal;
  edgex_device *result = NULL;

  internal = edgex_devmap_device_byid (svc->devices, id);
  if (internal)
  {
    result = edgex_device_dup (internal);
    edgex_device_release (internal);
  }
  return result;
}

edgex_device * edgex_get_device_byname (devsdk_service_t *svc, const char *name)
{
  edgex_device *internal;
  edgex_device *result = NULL;

  internal = edgex_devmap_device_byname (svc->devices, name);
  if (internal)
  {
    result = edgex_device_dup (internal);
    edgex_device_release (internal);
  }
  return result;
}

void edgex_remove_device (devsdk_service_t *svc, const char *id, devsdk_error *err)
{
  *err = EDGEX_OK;
  edgex_metadata_client_delete_device
    (svc->logger, &svc->config.endpoints, id, err);
  if (err->code != 0)
  {
    iot_log_error (svc->logger, "Unable to remove device %s from metadata", id);
  }
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
  const char *id,
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
    id,
    description,
    labels,
    profile_name,
    err
  );
  if (err->code)
  {
    iot_log_error (svc->logger, "Unable to update device %s", id ? id : name);
  }
}

void edgex_free_device (edgex_device *e)
{
  edgex_device_free (e);
}

void devsdk_add_discovered_devices (devsdk_service_t *svc, uint32_t ndevices, devsdk_discovered_device *devices)
{
  for (uint32_t i = 0; i < ndevices; i++)
  {
    for (devsdk_protocols *prots = devices[i].protocols; prots; prots = prots->next)
    {
      edgex_watcher *w = edgex_watchlist_match (svc->watchlist, prots->properties);
      if (w)
      {
        devsdk_error e;
        free (edgex_add_device (svc, devices[i].name, devices[i].description, devices[i].labels, w->profile, devices[i].protocols, w->adminstate == LOCKED, NULL, &e));
        edgex_watcher_free (w);
        break;
      }
    }
  }
}
