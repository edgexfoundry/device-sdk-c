/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "callback.h"
#include "profiles.h"
#include "errorlist.h"
#include "parson.h"
#include "service.h"
#include "metadata.h"
#include "edgex_rest.h"

#include <microhttpd.h>

int edgex_device_handler_callback
(
  void *ctx,
  char *url,
  edgex_http_method method,
  const char *upload_data,
  size_t upload_data_size,
  char **reply,
  const char **reply_type
)
{
  edgex_error err = EDGEX_OK;
  int status = MHD_HTTP_OK;
  edgex_device_service *svc = (edgex_device_service *) ctx;

  JSON_Value *jval = json_parse_string (upload_data);
  if (jval == NULL)
  {
    iot_log_error (svc->logger, "callback: Payload did not parse as JSON");
    return MHD_HTTP_BAD_REQUEST;
  }
  JSON_Object *jobj = json_value_get_object (jval);

  const char *action = json_object_get_string (jobj, "type");
  if (action && strcmp (action, "DEVICE") == 0)
  {
    const char *id = json_object_get_string (jobj, "id");
    pthread_rwlock_rdlock (&svc->deviceslock);
    edgex_device **ourdev = edgex_map_get (&svc->devices, id);
    pthread_rwlock_unlock (&svc->deviceslock);
    edgex_device *newdev = edgex_metadata_client_get_device
      (svc->logger, &svc->config.endpoints, id, &err);
    if (newdev)
    {
      if (ourdev)
      {
        if (method == PUT)
        {
          iot_log_info
            (svc->logger, "callback: Update device %s", newdev->name);
          if (strcmp (newdev->adminState, (*ourdev)->adminState))
          {
            free ((*ourdev)->adminState);
            (*ourdev)->adminState = strdup (newdev->adminState);
          }
          else
          {
            status = MHD_HTTP_BAD_REQUEST;
            iot_log_error
            (
              svc->logger,
              "callback: Non-adminstate update invoked for device %s",
              newdev->name
            );
          }
        }
        else
        {
          status = MHD_HTTP_BAD_REQUEST;
          iot_log_error
          (
            svc->logger,
            "callback: Ignoring non-PUT request for existing device %s",
            newdev->name
          );
        }
        edgex_device_free (newdev);
      }
      else
      {
        if (method == POST)
        {
          iot_log_info (svc->logger, "callback: New device %s", newdev->name);
          edgex_deviceprofile *profile = edgex_deviceprofile_get
            (svc, newdev->profile->name, &err);
          if (profile)
          {
            free (newdev->profile->name);
            free (newdev->profile);
            newdev->profile = profile;
            pthread_rwlock_wrlock (&svc->deviceslock);
            edgex_map_set (&svc->devices, newdev->id, newdev);
            edgex_map_set (&svc->name_to_id, newdev->name, newdev->id);
            pthread_rwlock_unlock (&svc->deviceslock);
          }
          else
          {
            status = MHD_HTTP_NOT_FOUND;
            iot_log_error
            (
              svc->logger,
              "callback: No device profile %s found, device %s unavailable",
              newdev->profile->name,
              newdev->name
            );
            edgex_device_free (newdev);
          }
        }
        else
        {
          status = MHD_HTTP_BAD_REQUEST;
          iot_log_error
          (
            svc->logger,
            "callback: Ignoring non-POST request for new device %s",
            newdev->name
          );
        }
      }
    }
    else
    {
      if (method == DELETE)
      {
        if (ourdev)
        {
          iot_log_info
            (svc->logger, "callback: Delete device %s", (*ourdev)->name);
          pthread_rwlock_wrlock (&svc->deviceslock);
          edgex_map_remove (&svc->name_to_id, (*ourdev)->name);
          edgex_map_remove (&svc->devices, id);
          pthread_rwlock_unlock (&svc->deviceslock);
          edgex_device_free (*ourdev);
        }
        else
        {
          status = MHD_HTTP_NOT_FOUND;
          iot_log_error
          (
            svc->logger,
            "callback: DELETE request for unknown device %s",
            id
          );
        }
      }
      else
      {
        status = MHD_HTTP_BAD_REQUEST;;
        iot_log_error
        (
          svc->logger,
          "callback: Ignoring non-DELETE request for missing device %s",
          ourdev ? (*ourdev)->name : id
        );
      }
    }
  }
  else
  {
    status = MHD_HTTP_BAD_REQUEST;
    iot_log_error
    (
      svc->logger,
      "callback: Unsupported action %s requested",
      action ? action : "(null)"
    );
  }

  json_value_free (jval);
  return status;
}
