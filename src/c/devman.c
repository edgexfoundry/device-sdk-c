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
  char **dev_id;
  char *result = NULL;

  *err = EDGEX_OK;
  pthread_rwlock_rdlock (&svc->deviceslock);
  dev_id = edgex_map_get (&svc->name_to_id, name);
  pthread_rwlock_unlock (&svc->deviceslock);

  if (dev_id)
  {
    iot_log_info (svc->logger, "Device %s already present", name);
    return strdup (*dev_id);
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

  if (err->code == 0)
  {
    result = strdup (newdev->id);
    iot_log_info (svc->logger, "Device %s added with id %s", name, result);
  }
  else
  {
    iot_log_error
      (svc->logger, "Failed to add Device in core-metadata: %s", err->reason);
  }
  edgex_device_free (newdev);
  return result;
}

edgex_device * edgex_device_devices
  (edgex_device_service *svc, edgex_error *err)
{
  *err = EDGEX_OK;
  edgex_device *result = edgex_metadata_client_get_devices
    (svc->logger, &svc->config.endpoints, svc->name, err);

  if (err->code)
  {
    iot_log_error (svc->logger, "Unable to retrieve device list from metadata");
    return NULL;
  }

  pthread_rwlock_wrlock (&svc->deviceslock);
  for (edgex_device *d = result; d; d = d->next)
  {
    if (edgex_map_get (&svc->name_to_id, d->name) == NULL)
    {
      edgex_device *dup = edgex_device_dup (d);
      edgex_map_set (&svc->devices, dup->id, dup);
      edgex_map_set (&svc->name_to_id, dup->name, dup->id);
    }
  }
  pthread_rwlock_unlock (&svc->deviceslock);

  pthread_mutex_lock (&svc->profileslock);
  for (edgex_device *d = result; d; d = d->next)
  {
    if (edgex_map_get (&svc->profiles, d->profile->name) == NULL)
    {
      edgex_deviceprofile *dup = edgex_deviceprofile_dup (d->profile);
      edgex_map_set (&svc->profiles, dup->name, dup);
    }
  }
  pthread_mutex_unlock (&svc->profileslock);

  return result;
}

edgex_device * edgex_device_get_device
  (edgex_device_service *svc, const char *id)
{
  edgex_device *result = NULL;
  pthread_rwlock_rdlock (&svc->deviceslock);
  edgex_device **orig = edgex_map_get (&svc->devices, id);
  if (orig)
  {
    result = edgex_device_dup (*orig);
  }
  pthread_rwlock_unlock (&svc->deviceslock);
  return result;
}

edgex_device * edgex_device_get_device_byname
  (edgex_device_service *svc, const char *name)
{
  edgex_device *result = NULL;
  pthread_rwlock_rdlock (&svc->deviceslock);
  char **id = edgex_map_get (&svc->name_to_id, name);
  if (id)
  {
    result = edgex_device_dup (*edgex_map_get (&svc->devices, *id));
  }
  pthread_rwlock_unlock (&svc->deviceslock);
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
