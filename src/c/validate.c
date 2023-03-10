/*
 * Copyright (c) 2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "validate.h"
#include "service.h"

#include <microhttpd.h>

static devsdk_protocols *protocols_convert (const iot_data_t *obj)
{
  // TODO: make devsdk_protocols an iot_data_t*

  devsdk_protocols *result = NULL;
  iot_data_map_iter_t iter;
  iot_data_map_iter (obj, &iter);
  while (iot_data_map_iter_has_next (&iter))
  {
    devsdk_protocols *prot = malloc (sizeof (devsdk_protocols));
    prot->name = strdup (iot_data_map_iter_string_key (&iter));
    prot->properties = iot_data_add_ref (iot_data_map_iter_value (&iter));
    prot->next = result;
    result = prot;
  }
  return result;
}

int32_t edgex_device_handler_validate_addr_v3 (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;
  int32_t result = 0;

  const iot_data_t *device = iot_data_string_map_get (req, "device");
  const iot_data_t *protocols = iot_data_string_map_get (device, "protocols");
  if (protocols)
  {
    iot_data_t *e = NULL;
    if (svc->userfns.validate_addr)
    {
      devsdk_protocols *p = protocols_convert (protocols);
      svc->userfns.validate_addr (svc->userdata, p, &e);
      devsdk_protocols_free (p);
    }
    if (e)
    {
      char *msg = iot_data_to_json (e);
      *reply = edgex_v3_error_response (svc->logger, "device %s invalid: %s", iot_data_string_map_get_string (req, "name"), msg);
      free (msg);
      iot_data_free (e);
      result = MHD_HTTP_INTERNAL_SERVER_ERROR;
    }
    else
    {
      *reply = edgex_v3_base_response ("Device protocols validated");
    }
  }
  else
  {
    *reply = edgex_v3_error_response (svc->logger, "callback: device: no protocols specified");
    result = MHD_HTTP_BAD_REQUEST;
  }
  return result;
}
