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
#include "edgex-rest.h"

#include <microhttpd.h>

static int updateProfile
(
  devsdk_service_t *svc,
  edgex_http_method method,
  const char *id
)
{
  if (method == POST)
  {
    return MHD_HTTP_OK;  // we'll fetch it on demand (for a new device)
  }
  if (method == PUT || method == DELETE)
  {
    // Update or remove. Should only happen if there are no devices left for this profile.
    // In either case we simply forget the profile.
    if (edgex_devmap_remove_profile (svc->devices, id))
    {
      iot_log_info (svc->logger, "callback: Removed Device Profile %s", id);
      return MHD_HTTP_OK;
    }
    else
    {
      iot_log_error (svc->logger, "Attempt to update/remove profile %s which still has associated devices. Ignored.", id);
      return MHD_HTTP_BAD_REQUEST;
    }
  }
  else
  {
    return MHD_HTTP_NOT_IMPLEMENTED;
  }
}

static int updateWatcher
(
  devsdk_service_t *svc,
  edgex_http_method method,
  const char *id
)
{
  edgex_watcher *w;
  devsdk_error err = EDGEX_OK;
  int status = MHD_HTTP_OK;

  switch (method)
  {
    case DELETE:
      iot_log_info (svc->logger, "callback: Delete watcher %s", id);
      if (!edgex_watchlist_remove_watcher (svc->watchlist, id))
      {
        iot_log_error (svc->logger, "callback: Watcher %s not found for deletion", id);
      }
      break;
    case POST:
      iot_log_info (svc->logger, "callback: New watcher %s", id);
      w = edgex_metadata_client_get_watcher
        (svc->logger, &svc->config.endpoints, id, &err);
      if (w)
      {
        if (edgex_watchlist_populate (svc->watchlist, w) != 1)
        {
          iot_log_error (svc->logger, "callback: Duplicate watcher %s (%s) not added", id, w->name);
        }
        edgex_watcher_free (w);
      }
      break;
    case PUT:
      iot_log_info (svc->logger, "callback: Update watcher %s", id);
      w = edgex_metadata_client_get_watcher
        (svc->logger, &svc->config.endpoints, id, &err);
      if (w)
      {
        edgex_watchlist_update_watcher (svc->watchlist, w);
        edgex_watcher_free (w);
      }
      break;
    default:
      status = MHD_HTTP_NOT_IMPLEMENTED;
      break;
  }

  return status;
}

static int updateDevice
(
  devsdk_service_t *svc,
  edgex_http_method method,
  const char *id
)
{
  edgex_device *newdev;
  devsdk_error err = EDGEX_OK;
  int status = MHD_HTTP_OK;

  switch (method)
  {
    case DELETE:
      iot_log_info (svc->logger, "callback: Delete device %s", id);
      if (svc->userfns.device_removed)
      {
        edgex_device *dev = edgex_devmap_device_byid (svc->devices, id);
        if (dev)
        {
          edgex_devmap_removedevice_byid (svc->devices, id);
          svc->userfns.device_removed (svc->userdata, dev->name, (const devsdk_protocols *)dev->protocols);
          edgex_device_release (dev);
        }
        else
        {
          iot_log_error (svc->logger, "callback: Device %s (for deletion) not found", id);
        }
      }
      else
      {
        edgex_devmap_removedevice_byid (svc->devices, id);
      }
      break;
    case POST:
    case PUT:
      newdev = edgex_metadata_client_get_device
        (svc->logger, &svc->config.endpoints, id, &err);
      if (newdev)
      {
        if (strcmp (newdev->service->name, svc->name))
        {
          iot_log_info (svc->logger, "callback: Device %s moved to %s", id, newdev->service->name);
          edgex_devmap_removedevice_byid (svc->devices, id);
          if (svc->userfns.device_removed)
          {
            svc->userfns.device_removed (svc->userdata, newdev->name, (const devsdk_protocols *)newdev->protocols);
          }
        }
        else
        {
          iot_log_info (svc->logger, "callback: New or updated device %s", id);
          switch (edgex_devmap_replace_device (svc->devices, newdev))
          {
            case CREATED:
              if (svc->userfns.device_added)
              {
                svc->userfns.device_added (svc->userdata, newdev->name, (const devsdk_protocols *)newdev->protocols, newdev->adminState);
              }
              break;
            case UPDATED_DRIVER:
              if (svc->userfns.device_updated)
              {
                svc->userfns.device_updated (svc->userdata, newdev->name, (const devsdk_protocols *)newdev->protocols, newdev->adminState);
              }
              break;
            case UPDATED_SDK:
              break;
          }
        }
        edgex_device_free (newdev);
      }
      break;
    default:
      status = MHD_HTTP_NOT_IMPLEMENTED;
      break;
  }
  return status;
}

int edgex_device_handler_callback
(
  void *ctx,
  char *url,
  const devsdk_nvpairs *qparams,
  edgex_http_method method,
  const char *upload_data,
  size_t upload_data_size,
  void **reply,
  size_t *reply_size,
  const char **reply_type
)
{
  int status = MHD_HTTP_OK;
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  JSON_Value *jval = json_parse_string (upload_data);
  if (jval == NULL)
  {
    iot_log_error (svc->logger, "callback: Payload did not parse as JSON");
    return MHD_HTTP_BAD_REQUEST;
  }

  JSON_Object *jobj = json_value_get_object (jval);
  const char *action = json_object_get_string (jobj, "type");
  const char *id = json_object_get_string (jobj, "id");
  if (!action || !id)
  {
    iot_log_error (svc->logger, "Callback: both 'type' and 'id' must be present");
    iot_log_error (svc->logger, "Callback: JSON was %s", upload_data);
    json_value_free (jval);
    return MHD_HTTP_BAD_REQUEST;
  }

  if (strcmp (action, "DEVICE") == 0)
  {
    status = updateDevice (svc, method, id);
  }
  else if (strcmp (action, "PROVISIONWATCHER") == 0)
  {
    status = updateWatcher (svc, method, id);
  }
  else if (strcmp (action, "PROFILE") == 0)
  {
    status = updateProfile (svc, method, id);
  }
  else
  {
    iot_log_error (svc->logger, "callback: Unexpected object type %s", action);
    status = MHD_HTTP_BAD_REQUEST;
  }

  json_value_free (jval);
  return status;
}
