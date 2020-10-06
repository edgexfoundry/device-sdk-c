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
#include "service.h"
#include "metadata.h"
#include "edgex-rest.h"
#include "devutil.h"

#include <microhttpd.h>

void edgex_device_handler_callback_profile (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  edgex_deviceprofile *p = edgex_deviceprofile_read (svc->logger, req->data.bytes);
  if (p)
  {
    if (req->method == DevSDK_Post)
    {
      edgex_devmap_add_profile (svc->devices, p);
      iot_log_info (svc->logger, "callback: New device profile %s", p->name);
    }
    else
    {
      edgex_devmap_update_profile (svc, p);
      iot_log_info (svc->logger, "callback: Updated device profile %s", p->name);
    }
    reply->code = MHD_HTTP_NO_CONTENT;
  }
  else
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "callback: device profile: unable to parse %s", req->data.bytes);
  }
}

void edgex_device_handler_callback_watcher (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  edgex_watcher *w = edgex_watcher_read (req->data.bytes);
  if (w)
  {
    if (req->method == DevSDK_Post)
    {
      iot_log_info (svc->logger, "callback: New provision watcher %s, w->name");
      if (edgex_watchlist_populate (svc->watchlist, w) == 1)
      {
        reply->code = MHD_HTTP_NO_CONTENT;
      }
      else
      {
        edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "callback: Duplicate watcher %s (%s) not added", w->name, w->id);
      }
    }
    else
    {
      iot_log_info (svc->logger, "callback: Update provision watcher %s, w->name");
      edgex_watchlist_update_watcher (svc->watchlist, w);
      reply->code = MHD_HTTP_NO_CONTENT;
    }
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

  edgex_device *d = edgex_device_read (svc->logger, req->data.bytes);
  if (d)
  {
    if (strcmp (d->service->name, svc->name))
    {
      iot_log_info (svc->logger, "callback: Device %s moved to %s", d->name, d->service->name);
      edgex_devmap_removedevice_byid (svc->devices, d->id);
      if (svc->userfns.device_removed)
      {
        svc->userfns.device_removed (svc->userdata, d->name, (const devsdk_protocols *)d->protocols);
      }
    }
    else
    {
      iot_log_info (svc->logger, "callback: New or updated device %s", d->name);
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
    reply->code = MHD_HTTP_NO_CONTENT;
    edgex_device_free (d);
  }
  else
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "callback: device: unable to parse %s", req->data.bytes);
  }
}

void edgex_device_handler_callback_device_id (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  bool found = false;
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  const char *id = devsdk_nvpairs_value (req->params, "id");
  iot_log_info (svc->logger, "callback: Delete device %s", id);
  if (svc->userfns.device_removed)
  {
    edgex_device *dev = edgex_devmap_device_byid (svc->devices, id);
    if (dev)
    {
      found = edgex_devmap_removedevice_byid (svc->devices, id);
      svc->userfns.device_removed (svc->userdata, dev->name, (const devsdk_protocols *)dev->protocols);
      edgex_device_release (dev);
    }
  }
  else
  {
    found = edgex_devmap_removedevice_byid (svc->devices, id);
  }

  if (found)
  {
    reply->code = MHD_HTTP_NO_CONTENT;
  }
  else
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_NOT_FOUND, "callback: delete device: no such device %s", id);
  }
}

void edgex_device_handler_callback_profile_id (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  const char *id = devsdk_nvpairs_value (req->params, "id");
  iot_log_info (svc->logger, "callback: Delete profile %s", id);

  if (edgex_devmap_remove_profile (svc->devices, id))
  {
    reply->code = MHD_HTTP_NO_CONTENT;
  }
  else
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "callback: delete profile: profile %s has associated devices. Ignored.", id);
  }
}

void edgex_device_handler_callback_watcher_id (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  const char *id = devsdk_nvpairs_value (req->params, "id");
  iot_log_info (svc->logger, "callback: Delete provision watcher %s", id);

  if (edgex_watchlist_remove_watcher (svc->watchlist, id))
  {
    reply->code = MHD_HTTP_NO_CONTENT;
  }
  else
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_NOT_FOUND, "callback: delete provision watcher: no such watcher %s", id);
  }
}
