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
  edgex_device *newdev;
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
    if (id)
    {
      switch (method)
      {
        case DELETE:
          iot_log_info (svc->logger, "callback: Delete device %s", id);
          edgex_devmap_removedevice_byid (svc->devices, id);
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
            }
            else
            {
              iot_log_info
                (svc->logger, "callback: New or updated device %s", id);
              edgex_devmap_replace_device (svc->devices, newdev);
            }
            edgex_device_free (newdev);
          }
          break;
        default:
          status = MHD_HTTP_NOT_IMPLEMENTED;
          break;
      }
    }
    else
    {
      iot_log_error (svc->logger, "No device id given for DEVICE callback");
      status = MHD_HTTP_BAD_REQUEST;
    }
  }
  else
  {
    status = MHD_HTTP_NOT_IMPLEMENTED;
  }

  json_value_free (jval);
  return status;
}
