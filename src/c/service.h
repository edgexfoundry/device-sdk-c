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
#include "edgex/edgex_logging.h"
#include "config.h"
#include "state.h"
#include "map.h"
#include "rest_server.h"
#include "thpool.h"
#include "iot/scheduler.h"

typedef edgex_map(edgex_device) edgex_map_device;
typedef edgex_map(edgex_deviceprofile) edgex_map_profile;
typedef edgex_map(edgex_reading) edgex_map_objectreading;
typedef edgex_map(edgex_map_objectreading) edgex_map_reading;

struct edgex_device_service
{
  const char *name;
  const char *version;
  void *userdata;
  edgex_device_callbacks userfns;
  iot_logging_client *logger;
  edgex_device_config config;
  edgex_rest_server *daemon;
  edgex_device_operatingstate opstate;
  edgex_device_adminstate adminstate;

  edgex_map_device devices;
  edgex_map_string name_to_id;
  pthread_rwlock_t deviceslock;

  edgex_map_profile profiles;
  pthread_mutex_t profileslock;

  threadpool thpool;
  iot_scheduler scheduler;
  pthread_mutex_t discolock;
};

#endif
