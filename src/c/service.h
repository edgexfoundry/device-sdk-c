/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_SERVICE_H_
#define _EDGEX_DEVICE_SERVICE_H_ 1

#include "edgex/devsdk.h"
#include "edgex/eventgen.h"
#include "edgex/edgex-logging.h"
#include "edgex/registry.h"
#include "config.h"
#include "devmap.h"
#include "rest-server.h"
#include "iot/threadpool.h"
#include "iot/scheduler.h"

struct edgex_device_service_job;

struct edgex_device_service
{
  const char *name;
  const char *version;
  void *userdata;
  edgex_device_callbacks userfns;
  edgex_device_autoevent_start_handler autoevstart;
  edgex_device_autoevent_stop_handler autoevstop;
  iot_logger_t *logger;
  edgex_device_config config;
  atomic_bool *stopconfig;
  edgex_rest_server *daemon;
  edgex_registry *registry;
  edgex_device_operatingstate opstate;
  edgex_device_adminstate adminstate;
  uint64_t starttime;

  edgex_devmap_t *devices;
  iot_threadpool_t *thpool;
  iot_scheduler_t *scheduler;
  pthread_mutex_t discolock;
};

#endif
