/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "callback.h"
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
  if (method == PUT)
  {
    JSON_Value *jval = json_parse_string (upload_data);
    if (jval == NULL)
    {
      iot_log_error (svc->logger, "callback: Payload did not parse as JSON");
      return MHD_HTTP_BAD_REQUEST;
    }
    JSON_Object *jobj = json_value_get_object (jval);

    const char *action = json_object_get_string (jobj, "actionType");
    if (strcmp (action, "DEVICE") == 0)
    {
      const char *id = json_object_get_string (jobj, "id");
      pthread_rwlock_rdlock (&svc->deviceslock);
      edgex_device **ourdev = edgex_map_get (&svc->devices, id);
      pthread_rwlock_unlock (&svc->deviceslock);
      if (ourdev)
      {
        edgex_device *newdev = edgex_metadata_client_get_device
          (svc->logger, &svc->config.endpoints, id, &err);
        if (newdev)
        {
          if (strcmp (newdev->adminState, (*ourdev)->adminState))
          {
            free ((*ourdev)->adminState);
            (*ourdev)->adminState = strdup (newdev->adminState);
          }
          else
          {
            status = MHD_HTTP_BAD_REQUEST;
            iot_log_error
              (svc->logger, "Non-adminstate update invoked for device %s", id);
          }
          edgex_device_free (newdev);
        }
        else
        {
          status = MHD_HTTP_NOT_FOUND;
          iot_log_error
            (svc->logger, "callback: unable to retrieve updated device %s", id);
        }
      }
      else
      {
        status = MHD_HTTP_NOT_FOUND;
        iot_log_error
          (svc->logger, "Update requested for nonexistent device %s", id);
      }
    }
    else
    {
      status = MHD_HTTP_BAD_REQUEST;
      iot_log_error
        (svc->logger, "Unsupported device service callback %s invoked", action);
    }
    json_value_free (jval);
  }
  else
  {
    status = MHD_HTTP_NOT_FOUND;
    iot_log_error
      (svc->logger, "Internal error: callback handler invoked for non-PUT");
  }
  return status;
}
