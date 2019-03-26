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
#include "edgex_rest.h"
#include "errorlist.h"
#include "edgex_time.h"

#include <string.h>
#include <stdlib.h>

/* Device (collection) management functions */

char * edgex_device_add_device
(
  edgex_device_service *svc,
  const char *name,
  const char *description,
  const edgex_strings *labels,
  const char *profile_name,
  edgex_protocols *protocols,
  edgex_error *err
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

  edgex_device *newdev = edgex_metadata_client_add_device
  (
    svc->logger,
    &svc->config.endpoints,
    name,
    description,
    labels,
    protocols,
    svc->name,
    profile_name,
    err
  );

  if (newdev)
  {
    result = strdup (newdev->id);
    edgex_device_free (newdev);
    iot_log_info (svc->logger, "Device %s added with id %s", name, result);
  }
  else
  {
    iot_log_error
      (svc->logger, "Failed to add Device in core-metadata: %s", err->reason);
  }
  return result;
}

edgex_device * edgex_device_devices (edgex_device_service *svc)
{
  return edgex_devmap_copydevices (svc->devices);
}

edgex_device * edgex_device_get_device
  (edgex_device_service *svc, const char *id)
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

edgex_device * edgex_device_get_device_byname
  (edgex_device_service *svc, const char *name)
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

void edgex_device_remove_device
  (edgex_device_service *svc, const char *id, edgex_error *err)
{
  *err = EDGEX_OK;
  edgex_metadata_client_delete_device
    (svc->logger, &svc->config.endpoints, id, err);
  if (err->code != 0)
  {
    iot_log_error (svc->logger, "Unable to remove device %s from metadata", id);
  }
}

void edgex_device_remove_device_byname
  (edgex_device_service *svc, const char *name, edgex_error *err)
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

void edgex_device_update_device
(
  edgex_device_service *svc,
  const char *id,
  const char *name,
  const char *description,
  const edgex_strings *labels,
  const char *profile_name,
  edgex_error *err
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

void edgex_device_free_device (edgex_device *e)
{
  edgex_device_free (e);
}
