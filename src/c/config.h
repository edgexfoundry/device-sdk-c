/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_CONFIG_H_
#define _EDGEX_DEVICE_CONFIG_H_ 1

#include "edgex/devsdk.h"
#include "map.h"

typedef struct edgex_device_serviceinfo
{
  char *host;
  int64_t port;
  int64_t connectretries;
  char **labels;
  char *openmsg;
  int64_t readmaxlimit;
  int64_t timeout;
} edgex_device_serviceinfo;

typedef struct edgex_device_service_endpoint
{
  char *host;
  int64_t port;
} edgex_device_service_endpoint;

typedef struct edgex_service_endpoints
{
  edgex_device_service_endpoint data;
  edgex_device_service_endpoint metadata;
  edgex_device_service_endpoint command;
  edgex_device_service_endpoint consul;
} edgex_service_endpoints;

typedef struct edgex_device_deviceinfo
{
  bool datatransform;
  bool discovery;
  char *initcmd;
  char *initcmdargs;
  int64_t maxcmdops;
  int64_t maxcmdresultlen;
  char *removecmd;
  char *removecmdargs;
  char *profilesdir;
  bool sendreadingsonchanged;
} edgex_device_deviceinfo;

typedef struct edgex_device_logginginfo
{
  char *file;
  char *remoteurl;
} edgex_device_logginginfo;

typedef struct edgex_device_scheduleeventinfo
{
  char *schedule;
  char *path;
} edgex_device_scheduleeventinfo;

typedef struct edgex_device_watcherinfo
{
  char *profile;
  char *key;
  char **ids;
  char *matchstring;
} edgex_device_watcherinfo;

typedef edgex_map(edgex_device_scheduleeventinfo) edgex_map_device_scheduleeventinfo;
typedef edgex_map(edgex_device_watcherinfo) edgex_map_device_watcherinfo;

typedef struct edgex_device_config
{
  edgex_device_serviceinfo service;
  edgex_service_endpoints endpoints;
  edgex_device_deviceinfo device;
  edgex_device_logginginfo logging;
  edgex_map_string schedules;
  edgex_map_device_scheduleeventinfo scheduleevents;
  edgex_map_device_watcherinfo watchers;
} edgex_device_config;

toml_table_t *edgex_device_loadConfig
(
  iot_logging_client *lc,
  const char *dir,
  const char *profile,
  edgex_error *err
);

const char *edgex_device_config_parse8601 (const char *str, int *result);

void edgex_device_populateConfig
  (edgex_device_service *svc, toml_table_t *config, edgex_error *err);

void edgex_device_validateConfig (edgex_device_service *svc, edgex_error *err);

void edgex_device_dumpConfig (edgex_device_service *svc);

void edgex_device_freeConfig (edgex_device_service *svc);

void edgex_device_process_configured_devices
  (edgex_device_service *svc, toml_array_t *devs, edgex_error *err);

#endif
