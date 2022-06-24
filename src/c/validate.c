/*
 * Copyright (c) 2022
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "validate.h"
#include "service.h"
#include "edgex-rest.h"

#include <microhttpd.h>

void edgex_device_handler_validate_addr (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  edgex_device *d = edgex_createdevicereq_read (req->data.bytes);
  if (d)
  {
    iot_data_t *e = NULL;
    if (svc->userfns.validate_addr)
    {
      svc->userfns.validate_addr (svc->userdata, d->protocols, &e);
    }
    if (e)
    {
      char *msg = iot_data_to_json (e);
      edgex_error_response (svc->logger, reply, MHD_HTTP_INTERNAL_SERVER_ERROR, "device %s invalid: %s", d->name, msg);
      free (msg);
      iot_data_free (e);
    }
    else
    {
      edgex_baseresponse br;
      edgex_baseresponse_populate (&br, "v2", MHD_HTTP_OK, "Device protocols validated");
      edgex_baseresponse_write (&br, reply);
    }
    edgex_device_free (svc, d);
  }
  else
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "callback: device: unable to parse %s", req->data.bytes);
  }
}
