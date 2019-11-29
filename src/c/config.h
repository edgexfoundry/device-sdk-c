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
#include "rest-server.h"
#include "toml.h"
#include "map.h"

typedef struct edgex_device_serviceinfo
{
  char *host;
  uint16_t port;
  uint32_t connectretries;
  char **labels;
  char *startupmsg;
  struct timespec timeout;
  char *checkinterval;
} edgex_device_serviceinfo;

typedef struct edgex_device_service_endpoint
{
  char *host;
  uint16_t port;
} edgex_device_service_endpoint;

typedef struct edgex_service_endpoints
{
  edgex_device_service_endpoint data;
  edgex_device_service_endpoint metadata;
  edgex_device_service_endpoint logging;
} edgex_service_endpoints;

typedef struct edgex_device_deviceinfo
{
  bool datatransform;
  bool discovery;
  char *initcmd;
  char *initcmdargs;
  uint32_t maxcmdops;
  uint32_t maxcmdresultlen;
  char *removecmd;
  char *removecmdargs;
  char *profilesdir;
  bool sendreadingsonchanged;
} edgex_device_deviceinfo;

typedef struct edgex_device_logginginfo
{
  char *file;
  bool useremote;
  iot_loglevel_t level;
} edgex_device_logginginfo;

typedef struct edgex_device_watcherinfo
{
  char *profile;
  char *key;
  char **ids;
  char *matchstring;
} edgex_device_watcherinfo;

typedef edgex_map(edgex_device_watcherinfo) edgex_map_device_watcherinfo;

typedef struct edgex_device_config
{
  edgex_device_serviceinfo service;
  edgex_service_endpoints endpoints;
  edgex_device_deviceinfo device;
  edgex_device_logginginfo logging;
  edgex_nvpairs *driverconf;
  edgex_map_device_watcherinfo watchers;
} edgex_device_config;

toml_table_t *edgex_device_loadConfig
(
  iot_logger_t *lc,
  const char *dir,
  const char *profile,
  edgex_error *err
);

char *edgex_device_getRegURL (toml_table_t *config);

edgex_nvpairs *edgex_device_parseToml (toml_table_t *config);

void edgex_device_parseTomlClients (iot_logger_t *lc, toml_table_t *clients, edgex_service_endpoints *endpoints, edgex_error *err);

void edgex_device_populateConfig
  (edgex_device_service *svc, const edgex_nvpairs *config, edgex_error *err);

void edgex_device_overrideConfig (iot_logger_t *lc, const char *sname, edgex_nvpairs *config);

void edgex_device_updateConf (void *svc, const edgex_nvpairs *config);

void edgex_device_freeConfig (edgex_device_service *svc);

void edgex_device_process_configured_devices
  (edgex_device_service *svc, toml_array_t *devs, edgex_error *err);

int edgex_device_handler_config
(
  void *ctx,
  char *url,
  const edgex_nvpairs *qparams,
  edgex_http_method method,
  const char *upload_data,
  size_t upload_data_size,
  void **reply,
  size_t *reply_size,
  const char **reply_type
);

#endif
