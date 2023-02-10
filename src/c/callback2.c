/*
 * Copyright (c) 2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "callback2.h"
#include "profiles.h"
#include "errorlist.h"
#include "parson.h"
#include "profiles.h"
#include "service.h"
#include "metadata.h"
#include "edgex-rest.h"
#include "devutil.h"

#include <microhttpd.h>

void edgex_device_handler_callback_service (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  edgex_deviceservice *ds = edgex_getDSresponse_read (req->data.bytes);
  if (ds)
  {
    if (svc->adminstate != ds->adminState)
    {
      svc->adminstate = ds->adminState;
      iot_log_info (svc->logger, "Service AdminState now %s", svc->adminstate == LOCKED ? "LOCKED" : "UNLOCKED");
    }
    edgex_deviceservice_free (ds);
    edgex_baseresponse br;
    edgex_baseresponse_populate (&br, "v2", MHD_HTTP_OK, "");
    edgex_baseresponse_write (&br, reply);
  }
  else
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "callback: device service: unable to parse %s", req->data.bytes);
  }
}

void edgex_device_handler_callback_profile (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  edgex_deviceprofile *p = edgex_getprofileresponse_read (svc->logger, req->data.bytes);
  if (p)
  {
    edgex_devmap_update_profile (svc, p);
    iot_log_info (svc->logger, "callback: Updated device profile %s", p->name);
    edgex_baseresponse br;
    edgex_baseresponse_populate (&br, "v2", MHD_HTTP_OK, "");
    edgex_baseresponse_write (&br, reply);
  }
  else
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "callback: device profile: unable to parse %s", req->data.bytes);
  }
}

void edgex_device_handler_callback_watcher (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  edgex_watcher *w = edgex_createPWreq_read (req->data.bytes);
  if (w)
  {
    if (req->method == DevSDK_Post)
    {
      iot_log_info (svc->logger, "callback: New provision watcher %s", w->name);
      if (edgex_watchlist_populate (svc->watchlist, w) != 1)
      {
        edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "callback: Duplicate watcher %s not added", w->name);
        edgex_watcher_free (w);
        return;
      }
    }
    else
    {
      iot_log_info (svc->logger, "callback: Update provision watcher %s", w->name);
      edgex_watchlist_update_watcher (svc->watchlist, w);
    }
    edgex_baseresponse br;
    edgex_baseresponse_populate (&br, "v2", MHD_HTTP_OK, "Provision watcher update accepted");
    edgex_baseresponse_write (&br, reply);
    edgex_watcher_free (w);
  }
  else
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "callback: provision watcher: unable to parse %s", req->data.bytes);
  }
}

void edgex_device_handler_callback_device (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  edgex_device *d = edgex_createdevicereq_read (req->data.bytes);
  if (d)
  {
    if (strcmp (d->servicename, svc->name))
    {
      iot_log_info (svc->logger, "callback: Device %s moved to %s", d->name, d->servicename);
      edgex_devmap_removedevice_byname (svc->devices, d->name);
      if (svc->userfns.device_removed)
      {
        svc->userfns.device_removed (svc->userdata, d->name, (const devsdk_protocols *)d->protocols);
      }
    }
    else
    {
      devsdk_error e;
      iot_log_info (svc->logger, "callback: New or updated device %s", d->name);
      if (edgex_deviceprofile_get_internal (svc, d->profile->name, &e) == NULL)
      {
        edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "callback: device: no profile %s available", d->profile->name);
        return;
      }
      switch (edgex_devmap_replace_device (svc->devices, d))
      {
        case CREATED:
          if (svc->userfns.device_added)
          {
            devsdk_device_resources *res = edgex_profile_toresources (d->profile);
            svc->userfns.device_added (svc->userdata, d->name, (const devsdk_protocols *)d->protocols, res, d->adminState);
            devsdk_free_resources (res);
          }
          break;
        case UPDATED_DRIVER:
          if (svc->userfns.device_updated)
          {
            svc->userfns.device_updated (svc->userdata, d->name, (const devsdk_protocols *)d->protocols, d->adminState);
          }
          break;
        case UPDATED_SDK:
          break;
      }
    }
    edgex_baseresponse br;
    edgex_baseresponse_populate (&br, "v2", MHD_HTTP_OK, "Data written successfully");
    edgex_baseresponse_write (&br, reply);
    edgex_device_free (svc, d);
  }
  else
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "callback: device: unable to parse %s", req->data.bytes);
  }
}

void edgex_device_handler_callback_device_name (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  bool found = false;
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  const char *name = devsdk_nvpairs_value (req->params, "name");
  iot_log_info (svc->logger, "callback: Delete device %s", name);
  if (svc->userfns.device_removed)
  {
    edgex_device *dev = edgex_devmap_device_byname (svc->devices, name);
    if (dev)
    {
      found = edgex_devmap_removedevice_byname (svc->devices, dev->name);
      svc->userfns.device_removed (svc->userdata, dev->name, (const devsdk_protocols *)dev->protocols);
      edgex_device_release (svc, dev);
    }
  }
  else
  {
    found = edgex_devmap_removedevice_byname (svc->devices, name);
  }

  if (found)
  {
    edgex_baseresponse br;
    edgex_baseresponse_populate (&br, "v2", MHD_HTTP_OK, "");
    edgex_baseresponse_write (&br, reply);
  }
  else
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_NOT_FOUND, "callback: delete device: no such device %s", name);
  }
}

void edgex_device_handler_callback_watcher_name (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  const char *name = devsdk_nvpairs_value (req->params, "name");
  iot_log_info (svc->logger, "callback: Delete provision watcher %s", name);

  if (edgex_watchlist_remove_watcher (svc->watchlist, name))
  {
    edgex_baseresponse br;
    edgex_baseresponse_populate (&br, "v2", MHD_HTTP_OK, "");
    edgex_baseresponse_write (&br, reply);
  }
  else
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_NOT_FOUND, "callback: delete provision watcher: no such watcher %s", name);
  }
}
