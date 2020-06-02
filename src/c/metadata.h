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
#include "parson.h"

typedef struct edgex_service_endpoints edgex_service_endpoints;

edgex_deviceprofile * edgex_metadata_client_get_deviceprofile
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const char * name,
  devsdk_error * err
);
JSON_Value * edgex_metadata_client_get_config
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  devsdk_error * err
);
void edgex_metadata_client_set_device_opstate
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const char * deviceid,
  edgex_device_operatingstate opstate,
  devsdk_error * err
);
void edgex_metadata_client_set_device_opstate_byname
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const char * devicename,
  edgex_device_operatingstate opstate,
  devsdk_error * err
);
void edgex_metadata_client_set_device_adminstate
(
  iot_logger_t *lc,
  edgex_service_endpoints * endpoints,
  const char * deviceid,
  edgex_device_adminstate adminstate,
  devsdk_error *err
);
char * edgex_metadata_client_create_deviceprofile
(
  iot_logger_t *lc,
  edgex_service_endpoints * endpoints,
  const edgex_deviceprofile * newdp,
  devsdk_error *err
);
char * edgex_metadata_client_create_deviceprofile_file
(
  iot_logger_t *lc,
  edgex_service_endpoints * endpoints,
  const char * filename,
  devsdk_error *err
);
edgex_deviceservice * edgex_metadata_client_get_deviceservice
(
  iot_logger_t *lc,
  edgex_service_endpoints * endpoints,
  const char * name,
  devsdk_error *err
);
char * edgex_metadata_client_create_deviceservice
(
  iot_logger_t *lc,
  edgex_service_endpoints * endpoints,
  const edgex_deviceservice * newds,
  devsdk_error *err
);
edgex_device * edgex_metadata_client_get_devices
(
  iot_logger_t *lc,
  edgex_service_endpoints * endpoints,
  const char * servicename,
  devsdk_error *err
);
char * edgex_metadata_client_add_device
(
  iot_logger_t *lc,
  edgex_service_endpoints * endpoints,
  const char * name,
  const char * description,
  const devsdk_strings * labels,
  edgex_device_adminstate adminstate,
  devsdk_protocols * protocols,
  edgex_device_autoevents * autos,
  const char * service_name,
  const char * profile_name,
  devsdk_error *err
);
edgex_device * edgex_metadata_client_get_device
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const char * deviceid,
  devsdk_error * err
);
void edgex_metadata_client_update_device
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const char * name,
  const char * id,
  const char * description,
  const devsdk_strings * labels,
  const char * profile_name,
  devsdk_error * err
);
void edgex_metadata_client_delete_device
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const char * deviceid,
  devsdk_error * err
);
void edgex_metadata_client_delete_device_byname
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const char * devicename,
  devsdk_error * err
);
void edgex_metadata_client_update_lastconnected
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const char * devicename,
  devsdk_error * err
);
edgex_watcher *edgex_metadata_client_get_watchers
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const char * servicename,
  devsdk_error * err
);
edgex_watcher *edgex_metadata_client_get_watcher
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const char * watcherId,
  devsdk_error * err
);
edgex_addressable * edgex_metadata_client_get_addressable
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const char * name,
  devsdk_error * err
);
char * edgex_metadata_client_create_addressable
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const edgex_addressable * newadd,
  devsdk_error * err
);
void edgex_metadata_client_update_addressable
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const edgex_addressable * addressable,
  devsdk_error * err
);

#endif
