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
  edgex_addressable *address,
  edgex_error *err
)
{
  const char *postfix = "_addr";
  char **dev_id;

  pthread_rwlock_rdlock (&svc->deviceslock);
  dev_id = edgex_map_get (&svc->name_to_id, name);
  pthread_rwlock_unlock (&svc->deviceslock);

  if (dev_id)
  {
    iot_log_info (svc->logger, "Device %s already present", name);
    return strdup (*dev_id);
  }

  edgex_addressable *newaddr = edgex_addressable_dup (address);
  if (newaddr->name == NULL)
  {
    newaddr->name = malloc (strlen (name) + strlen (postfix) + 1);
    strcpy (newaddr->name, name);
    strcat (newaddr->name, postfix);
  }

  /* Check for existing addressable */

  iot_log_debug
    (svc->logger, "Checking existence of Addressable %s", newaddr->name);
  edgex_addressable *existingaddr =
    edgex_metadata_client_get_addressable
      (svc->logger, &svc->config.endpoints, newaddr->name, err);

  if (existingaddr)
  {
    iot_log_info (svc->logger, "Addressable %s already exists", newaddr->name);
    edgex_addressable_free (existingaddr);
  }
  else
  {
    if (newaddr->origin == 0)
    {
      newaddr->origin = edgex_device_millitime ();
    }
    newaddr->id = edgex_metadata_client_create_addressable
      (svc->logger, &svc->config.endpoints, newaddr, err);
    iot_log_info (svc->logger, "New addressable %s created", newaddr->name);
  }

  err->code = 0;
  edgex_device *newdev = edgex_metadata_client_add_device
  (
    svc->logger,
    &svc->config.endpoints,
    name,
    description,
    labels,
    newaddr->origin,
    newaddr->name,
    svc->name,
    profile_name,
    err
  );

  if (err->code == 0)
  {
    iot_log_info (svc->logger, "Device %s added with id %s", name, newdev->id);
    edgex_deviceprofile *profile = edgex_deviceprofile_get
      (svc, newdev->profile->name, err);
    if (profile)
    {
      free (newdev->profile->name);
      free (newdev->profile);
      newdev->profile = profile;
      free (newdev->addressable->name);
      free (newdev->addressable);
      newdev->addressable = newaddr;
      pthread_rwlock_wrlock (&svc->deviceslock);
      edgex_map_set (&svc->devices, newdev->id, newdev);
      edgex_map_set (&svc->name_to_id, name, newdev->id);
      pthread_rwlock_unlock (&svc->deviceslock);
      return strdup (newdev->id);
    }
    else
    {
      iot_log_error
      (
        svc->logger,
        "No device profile %s found, device %s will not be available",
        newdev->profile->name,
        newdev->name
      );
      edgex_device_free (newdev);
      edgex_addressable_free (newaddr);
      return NULL;
    }
  }
  else
  {
    iot_log_error
      (svc->logger, "Failed to add Device in core-metadata: %s", err->reason);
    edgex_device_free (newdev);
    edgex_addressable_free (newaddr);
    return NULL;
  }
}

edgex_device * edgex_device_devices
  (edgex_device_service *svc, edgex_error *err)
{
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

void edgex_device_remove_device
  (edgex_device_service *svc, const char *id, edgex_error *err)
{
  edgex_metadata_client_delete_device
    (svc->logger, &svc->config.endpoints, id, err);
  if (err->code == 0)
  {
    pthread_rwlock_wrlock (&svc->deviceslock);
    edgex_device **d = edgex_map_get (&svc->devices, id);
    edgex_device *dev = d ? *d : NULL;
    if (d)
    {
      edgex_map_remove (&svc->name_to_id, dev->name);
      edgex_map_remove (&svc->devices, id);
    }
    pthread_rwlock_unlock (&svc->deviceslock);
    if (dev)
    {
      edgex_metadata_client_delete_addressable
        (svc->logger, &svc->config.endpoints, dev->addressable->name, err);
      if (err->code)
      {
        iot_log_error
        (
          svc->logger,
          "Unable to remove addressable %s from metadata",
          (*d)->addressable->name
        );
      }
      edgex_device_free (dev);
    }
  }
  else
  {
    iot_log_error (svc->logger, "Unable to remove device %s from metadata", id);
  }
}

void edgex_device_remove_device_byname
  (edgex_device_service *svc, const char *name, edgex_error *err)
{
  edgex_metadata_client_delete_device_byname
    (svc->logger, &svc->config.endpoints, name, err);
  if (err->code == 0)
  {
    pthread_rwlock_wrlock (&svc->deviceslock);
    char **id = edgex_map_get (&svc->name_to_id, name);
    edgex_device **d = edgex_map_get (&svc->devices, *id);
    edgex_device *dev = d ? *d : NULL;
    if (id)
    {
      edgex_map_remove (&svc->name_to_id, name); 
      edgex_map_remove (&svc->devices, *id);
    }
    pthread_rwlock_unlock (&svc->deviceslock);
    if (dev)
    {
      edgex_metadata_client_delete_addressable
        (svc->logger, &svc->config.endpoints, dev->addressable->name, err);
      if (err->code)
      {
        iot_log_error
        (
          svc->logger,
          "Unable to remove addressable %s from metadata",
          dev->addressable->name
        );
      }
      edgex_device_free (dev);
    }
  }
  else
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
  edgex_device *olddev = NULL;
  edgex_device **od = NULL;

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
  if (err->code == 0)
  {
    pthread_rwlock_wrlock (&svc->deviceslock);
    if (id)
    {
      od = edgex_map_get (&svc->devices, id);
      if (od)
      {
        olddev = *od;
        edgex_map_remove (&svc->name_to_id, olddev->name);
        edgex_map_remove (&svc->devices, id);
      }
    }
    else
    {
      char **eid = edgex_map_get (&svc->name_to_id, name);
      if (eid)
      {
        od = edgex_map_get (&svc->devices, *eid);
        if (od)
        {
          olddev = *od;
        }
        edgex_map_remove (&svc->devices, *eid);
        edgex_map_remove (&svc->name_to_id, name);
      }
    }
    pthread_rwlock_unlock (&svc->deviceslock);

    if (olddev)
    {
      edgex_device_free (olddev);
    }

    edgex_device *newdev;
    if (id)
    {
      newdev = edgex_metadata_client_get_device
        (svc->logger, &svc->config.endpoints, id, err);
    }
    else
    {
      newdev = edgex_metadata_client_get_device_byname
        (svc->logger, &svc->config.endpoints, name, err);
    }

    if (err->code)
    {
      iot_log_error
      (
        svc->logger,
        "Unable to retrieve device %s following update",
        name ? name : id
      );
    }
    else
    {
      pthread_rwlock_wrlock (&svc->deviceslock);
      edgex_map_set (&svc->devices, newdev->id, newdev);
      edgex_map_set (&svc->name_to_id, newdev->name, newdev->id);
      pthread_rwlock_unlock (&svc->deviceslock);
    }
  }
  else
  {
    iot_log_error (svc->logger, "Unable to update device %s", id ? id : name);
  }
}
