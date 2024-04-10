/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_METADATA_H_
#define _EDGEX_METADATA_H_ 1

#include "edgex/edgex.h"
#include "devsdk/devsdk-base.h"
#include "iot/logger.h"
#include "devmap.h"
#include "parson.h"
#include "secrets.h"

typedef struct edgex_service_endpoints edgex_service_endpoints;

edgex_deviceprofile * edgex_metadata_client_get_deviceprofile
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * name,
  devsdk_error * err
);
void edgex_metadata_client_set_device_opstate
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * devicename,
  edgex_device_operatingstate opstate,
  devsdk_error * err
);
char * edgex_metadata_client_create_deviceprofile_file
(
  iot_logger_t *lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * filename,
  devsdk_error *err
);
edgex_deviceservice * edgex_metadata_client_get_deviceservice
(
  iot_logger_t *lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * name,
  devsdk_error *err
);
void edgex_metadata_client_create_deviceservice
(
  iot_logger_t *lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const edgex_deviceservice * newds,
  devsdk_error *err
);
void edgex_metadata_client_update_deviceservice
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * name,
  const char * baseaddr,
  devsdk_error * err
);
edgex_device * edgex_metadata_client_get_devices
(
  iot_logger_t *lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * servicename,
  devsdk_error *err
);
char * edgex_metadata_client_add_device
(
  iot_logger_t *lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * name,
  const char * parent,
  const char * description,
  const devsdk_strings * labels,
  edgex_device_adminstate adminstate,
  devsdk_protocols * protocols,
  edgex_device_autoevents * autos,
  const char * service_name,
  const char * profile_name,
  devsdk_error *err
);

void edgex_metadata_client_add_device_jobj (iot_logger_t *lc, edgex_service_endpoints *endpoints, edgex_secret_provider_t * secretprovider, JSON_Object *jobj, devsdk_error *err);
void edgex_metadata_client_add_profile_jobj (iot_logger_t *lc, edgex_service_endpoints *endpoints, edgex_secret_provider_t * secretprovider, JSON_Object *jobj, devsdk_error *err);

void edgex_metadata_client_add_or_modify_device
(
  iot_logger_t *lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * name,
  const char * parent, 
  const char * description,
  const devsdk_strings * labels,
  edgex_device_adminstate adminstate,
  devsdk_protocols * protocols,
  edgex_device_autoevents * autos,
  const char * service_name,
  const char * profile_name
);

bool edgex_metadata_client_check_device (iot_logger_t * lc, edgex_service_endpoints * endpoints, edgex_secret_provider_t * secretprovider, const char * devicename);

void edgex_metadata_client_update_device
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * name,
  const char * parent,
  const char * description,
  const devsdk_strings * labels,
  const char * profile_name,
  devsdk_error * err
);
void edgex_metadata_client_delete_device_byname
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * devicename,
  devsdk_error * err
);
void edgex_metadata_client_update_lastconnected
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * devicename,
  devsdk_error * err
);
edgex_watcher *edgex_metadata_client_get_watchers
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * servicename,
  devsdk_error * err
);

#endif
