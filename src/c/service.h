/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_SERVICE_H_
#define _EDGEX_DEVICE_SERVICE_H_ 1

#include "devsdk/devsdk.h"
#include "edgex/edgex-logging.h"
#include "registry.h"
#include "config.h"
#include "devmap.h"
#include "watchers.h"
#include "rest-server.h"
#include "iot/threadpool.h"
#include "iot/scheduler.h"

struct devsdk_service_t
{
  char *name;
  const char *version;
  const char *regURL;
  const char *profile;
  const char *confdir;
  const char *conffile;
  void *userdata;
  devsdk_callbacks userfns;
  iot_logger_t *logger;
  edgex_device_config config;
  atomic_bool *stopconfig;
  edgex_rest_server *daemon;
  devsdk_registry *registry;
  edgex_device_operatingstate opstate;
  edgex_device_adminstate adminstate;
  uint64_t starttime;
  bool overwriteconfig;

  edgex_devmap_t *devices;
  edgex_watchlist_t *watchlist;
  iot_threadpool_t *thpool;
  iot_scheduler_t *scheduler;
  iot_schedule_t *discosched;
  pthread_mutex_t discolock;
};

#endif
