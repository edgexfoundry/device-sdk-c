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
#include "edgex_rest.h"

#include <string.h>
#include <stdlib.h>

#include <microhttpd.h>

int edgex_device_handler_discovery
(
  void *ctx,
  const char *url,
  edgex_http_method method,
  const char *upload_data,
  size_t upload_data_size,
  char **reply,
  const char **reply_type
)
{
  edgex_device_service *svc = (edgex_device_service *) ctx;

  if (svc->adminstate == LOCKED || svc->opstate == DISABLED)
  {
    return MHD_HTTP_LOCKED;
  }

  if (pthread_mutex_trylock (&svc->discolock) == 0)
  {
    thpool_add_work (svc->thpool, edgex_device_handler_do_discovery, svc);
    pthread_mutex_unlock (&svc->discolock);
  }
  // else discovery was already running; ignore this request

  *reply = strdup ("Running discovery\n");
  return MHD_HTTP_OK;
}

void edgex_device_handler_do_discovery (void *p)
{
  edgex_device_service *svc = (edgex_device_service *) p;

  pthread_mutex_lock (&svc->discolock);
  svc->userfns.discover (svc->userdata);
  pthread_mutex_unlock (&svc->discolock);
}

void edgex_device_add_device
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
  char *id;

  pthread_rwlock_rdlock (&svc->deviceslock);
  dev_id = edgex_map_get (&svc->name_to_id, name);
  pthread_rwlock_unlock (&svc->deviceslock);

  if (dev_id)
  {
    iot_log_info (svc->logger, "Device %s already present", name);
    return;
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
      newaddr->origin = time (NULL) * 1000UL;
    }
    id = edgex_metadata_client_create_addressable
      (svc->logger, &svc->config.endpoints, newaddr, err);
    iot_log_info (svc->logger, "New addressable %s created", newaddr->name);
    newaddr->id = id;
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
      edgex_map_set (&svc->devices, newdev->id, *newdev);
      edgex_map_set (&svc->name_to_id, name, newdev->id);
      pthread_rwlock_unlock (&svc->deviceslock);
      free (newdev);
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
    }
  }
  else
  {
    iot_log_error
      (svc->logger, "Failed to add Device in core-metadata: %s", err->reason);
    edgex_device_free (newdev);
    edgex_addressable_free (newaddr);
  }
}
