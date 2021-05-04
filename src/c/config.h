/*
 * Copyright (c) 2018-2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_CONFIG_H_
#define _EDGEX_DEVICE_CONFIG_H_ 1

#include "devsdk/devsdk.h"
#include "rest-server.h"
#include "toml.h"
#include "map.h"

#include <stdatomic.h>

typedef struct edgex_device_serviceinfo
{
  const char *host;
  uint16_t port;
  uint32_t connectretries;
  char **labels;
  const char *startupmsg;
  struct timespec timeout;
  const char *checkinterval;
  const char *bindaddr;
  uint64_t maxreqsz;
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
} edgex_service_endpoints;

typedef struct edgex_device_deviceinfo
{
  atomic_bool datatransform;
  atomic_bool discovery_enabled;
  _Atomic(uint32_t) discovery_interval;
  _Atomic(uint32_t) maxcmdops;
  _Atomic(uint32_t) maxcmdresultlen;
  const char *profilesdir;
  const char *devicesdir;
  atomic_bool updatelastconnected;
  uint32_t eventqlen;
} edgex_device_deviceinfo;

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
  iot_loglevel_t loglevel;
  iot_data_t *driverconf;
  iot_data_t *sdkconf;
  edgex_map_device_watcherinfo watchers;
} edgex_device_config;

toml_table_t *edgex_device_loadConfig
(
  iot_logger_t *lc,
  const char *dir,
  const char *fname,
  const char *profile,
  devsdk_error *err
);

iot_data_t *edgex_config_defaults (const iot_data_t *driverconf);

char *edgex_device_getRegURL (toml_table_t *config);

void edgex_device_parseTomlClients (iot_logger_t *lc, toml_table_t *clients, edgex_service_endpoints *endpoints, devsdk_error *err);

void edgex_device_populateConfig (devsdk_service_t *svc, iot_data_t *config);

void edgex_device_overrideConfig_toml (iot_data_t *config, toml_table_t *toml);

void edgex_device_overrideConfig_env (iot_logger_t *lc, iot_data_t *config);

void edgex_device_overrideConfig_nvpairs (iot_data_t *config, const devsdk_nvpairs *pairs);

void edgex_device_updateConf (void *svc, const devsdk_nvpairs *config);

void edgex_device_processDriverConfig (iot_data_t *driverconf, const iot_data_t *allconf);

void edgex_device_dumpConfig (iot_logger_t *lc, iot_data_t *config);

void edgex_device_freeConfig (devsdk_service_t *svc);

void edgex_device_process_configured_devices (devsdk_service_t *svc, toml_array_t *devs, devsdk_error *err);

void edgex_device_handler_configv2 (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply);

#endif
