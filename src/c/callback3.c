/*
 * Copyright (c) 2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "callback3.h"

#include "service.h"
#include "profiles.h"
#include "dto-read.h"
#include "edgex-rest.h"
#include "autoevent.h"
#include <microhttpd.h>

int32_t edgex_callback_add_device (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;
  devsdk_error e;
  const edgex_deviceprofile *prof;

  edgex_device *d = edgex_device_read (iot_data_string_map_get (req, "details"));
  iot_log_info (svc->logger, "callback: New device %s", d->name);
  prof = edgex_deviceprofile_get_internal (svc, d->profile->name, &e);
  if (prof == NULL)
  {
    edgex_device_free (svc, d);
    iot_log_error (svc->logger, "callback: device: no profile %s available", d->profile->name);
    return MHD_HTTP_BAD_REQUEST;
  }
  edgex_devmap_replace_device (svc->devices, d);
  if (svc->userfns.device_added)
  {
    devsdk_device_resources *res = edgex_profile_toresources (prof);
    svc->userfns.device_added (svc->userdata, d->name, (const devsdk_protocols *)d->protocols, res, d->adminState);
    devsdk_free_resources (res);
  }
  edgex_device_free (svc, d);

  return 0;
}

int32_t edgex_callback_delete_device (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;
  bool found = false;

  const iot_data_t *details = iot_data_string_map_get (req, "details");
  const char *devname = iot_data_string_map_get_string (details, "name");

  iot_log_info (svc->logger, "callback: Delete device %s", devname);
  if (svc->userfns.device_removed)
  {
    edgex_device *dev = edgex_devmap_device_byname (svc->devices, devname);
    if (dev)
    {
      found = edgex_devmap_removedevice_byname (svc->devices, dev->name);
      svc->userfns.device_removed (svc->userdata, dev->name, (const devsdk_protocols *)dev->protocols);
      edgex_device_release (svc, dev);
    }
  }
  else
  {
    found = edgex_devmap_removedevice_byname (svc->devices, devname);
  }

  if (!found)
  {
    iot_log_error (svc->logger, "callback: delete device: no such device %s", devname);
  }
  return 0;
}

int32_t edgex_callback_update_device (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;
  devsdk_error e;

  edgex_device *d = edgex_device_read (iot_data_string_map_get (req, "details"));
  iot_log_info (svc->logger, "callback: Update device %s", d->name);
  if (edgex_deviceprofile_get_internal (svc, d->profile->name, &e) == NULL)
  {
    edgex_device_free (svc, d);
    iot_log_error (svc->logger, "callback: device: no profile %s available", d->profile->name);
    return MHD_HTTP_BAD_REQUEST;
  }
  if (edgex_devmap_replace_device (svc->devices, d) == UPDATED_DRIVER && svc->userfns.device_updated)
  {
    svc->userfns.device_updated (svc->userdata, d->name, (const devsdk_protocols *)d->protocols, d->adminState);
  }

  bool has_autoevents = (d->autos != NULL);
  if(has_autoevents){
    edgex_device *updated_dev = edgex_devmap_device_byname(svc->devices, d->name);
    edgex_device_autoevent_stop(updated_dev);
    edgex_device_autoevent_start(svc, updated_dev);
    edgex_device_release(svc, updated_dev);
  }
  edgex_device_free (svc, d);

  return 0;
}

int32_t edgex_callback_update_deviceservice (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  const iot_data_t *details = iot_data_string_map_get (req, "details");
  const iot_data_t *as = iot_data_string_map_get (details, "adminState");
  if (as)
  {
    edgex_device_adminstate newAS = edgex_adminstate_read (as);
    if (svc->adminstate != newAS)
    {
      svc->adminstate = newAS;
      iot_log_info (svc->logger, "Service AdminState now %s", svc->adminstate == LOCKED ? "LOCKED" : "UNLOCKED");
    }
  }
  return 0;
}

int32_t edgex_callback_add_pw (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  edgex_watcher *w = edgex_pw_read (iot_data_string_map_get (req, "details"));
  iot_log_info (svc->logger, "callback: New provision watcher %s", w->name);
  if (edgex_watchlist_populate (svc->watchlist, w) != 1)
  {
    iot_log_error (svc->logger, "callback: Duplicate watcher %s not added", w->name);
  }
  edgex_watcher_free (w);
  return 0;
}

int32_t edgex_callback_update_pw (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  edgex_watcher *w = edgex_pw_read (iot_data_string_map_get (req, "details"));
  iot_log_info (svc->logger, "callback: Update provision watcher %s", w->name);
  edgex_watchlist_update_watcher (svc->watchlist, w);
  edgex_watcher_free (w);
  return 0;
}

int32_t edgex_callback_delete_pw (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  const iot_data_t *details = iot_data_string_map_get (req, "details");
  const char *name = iot_data_string_map_get_string (details, "name");

  iot_log_info (svc->logger, "callback: Delete provision watcher %s", name);
  if (!edgex_watchlist_remove_watcher (svc->watchlist, name))
  {
    iot_log_error (svc->logger, "callback: delete provision watcher: no such watcher %s", name);
  }
  return 0;
}

extern int32_t edgex_callback_update_profile (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

  edgex_deviceprofile *p = edgex_profile_read (iot_data_string_map_get (req, "details"));
  edgex_devmap_update_profile (svc, p);
  iot_log_info (svc->logger, "callback: Updated device profile %s", p->name);
  // For now jst checking if listener defined, then call the listener
  // To be improved: Check and call the listener only for specific conditions (device resource/command update) only
  if ( svc->userfns.profile_updated )
  {
    iot_log_info( svc->logger, "service listener callback trigger: device profile %s", p->name );
    svc->userfns.profile_updated( svc->userdata, p->name );
  }
  return 0;
}
