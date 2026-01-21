/*
 * Copyright (c) 2018-2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_SERVICE_H_
#define _EDGEX_DEVICE_SERVICE_H_ 1

#include "devsdk/devsdk.h"
#include "registry.h"
#include "config.h"
#include "bus.h"
#include "secrets.h"
#include "devmap.h"
#include "watchers.h"
#include "discovery.h"
#include "rest-server.h"
#include "iot/threadpool.h"
#include "iot/scheduler.h"
#include "request_auth.h"

struct devsdk_callbacks
{
  devsdk_initialize init;
  devsdk_reconfigure reconfigure;
  devsdk_handle_get gethandler;
  devsdk_handle_put puthandler;
  devsdk_stop stop;
  devsdk_create_address create_addr;
  devsdk_free_address free_addr;
  devsdk_create_resource_attr create_res;
  devsdk_free_resource_attr free_res;
  devsdk_discover discover;
  devsdk_discovery_delete discovery_delete;
  devsdk_describe describe;
  devsdk_add_device_callback device_added;
  devsdk_update_device_callback device_updated;
  devsdk_remove_device_callback device_removed;
  devsdk_update_profile_callback profile_updated;
  devsdk_autoevent_start_handler ae_starter;
  devsdk_autoevent_stop_handler ae_stopper;
  devsdk_validate_address validate_addr;
};

struct devsdk_service_t
{
  char *name;
  const char *version;
  const char *regURL;
  const char *profile;
  const char *confdir;
  const char *conffile;
  const char *commonconffile;
  char *confpath;
  void *userdata;
  devsdk_callbacks userfns;
  iot_logger_t *logger;
  edgex_device_config config;
  atomic_bool *stopconfig;
  edgex_rest_server *daemon;
  edgex_device_periodic_discovery_t *discovery;
  edgex_bus_t *msgbus;
  edgex_secret_provider_t *secretstore;
  devsdk_registry_t *registry;
  edgex_device_adminstate adminstate;
  uint64_t starttime;
  devsdk_metrics_t metrics;
  iot_schedule_t *metricschedule;
  bool overwriteconfig;
  bool secureMode;
  bool devmode;
  char *local_ip;
  bool remote_mode;
  char *local_host;
  char *remote_host;
  char *bind_host;

  edgex_devmap_t *devices;
  edgex_watchlist_t *watchlist;
  iot_threadpool_t *thpool;
  iot_threadpool_t *eventq;
  iot_scheduler_t *scheduler;

  auth_wrapper_t callback_profile_wrapper;
  auth_wrapper_t callback_watcher_wrapper;
  auth_wrapper_t callback_watcher_name_wrapper;
  auth_wrapper_t device_name_wrapper;
  auth_wrapper_t discovery_wrapper;
  auth_wrapper_t discovery_delete_wrapper;
  auth_wrapper_t metrics_wrapper;
  auth_wrapper_t config_wrapper;
  auth_wrapper_t secret_wrapper;
  auth_wrapper_t version_wrapper;
  // Note: no ping_wrapper (intentionally)!

};

extern void devsdk_schedule_metrics (devsdk_service_t *svc);

#endif
