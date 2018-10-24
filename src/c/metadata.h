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
#include "edgex/edgex_logging.h"
#include "edgex/error.h"
#include "schedules.h"

typedef struct edgex_service_endpoints edgex_service_endpoints;

edgex_deviceprofile * edgex_metadata_client_get_deviceprofile
(
  iot_logging_client * lc,
  edgex_service_endpoints * endpoints,
  const char * name,
  edgex_error * err
);
void edgex_metadata_client_set_device_opstate
(
  iot_logging_client * lc,
  edgex_service_endpoints * endpoints,
  const char * deviceid,
  bool enabled,
  edgex_error * err
);
void edgex_metadata_client_set_device_adminstate
(
  iot_logging_client *lc,
  edgex_service_endpoints * endpoints,
  const char * deviceid,
  bool locked,
  edgex_error *err
);
char * edgex_metadata_client_create_deviceprofile
(
  iot_logging_client *lc,
  edgex_service_endpoints * endpoints,
  const edgex_deviceprofile * newdp,
  edgex_error *err
);
char * edgex_metadata_client_create_deviceprofile_file
(
  iot_logging_client *lc,
  edgex_service_endpoints * endpoints,
  const char * filename,
  edgex_error *err
);
edgex_deviceservice * edgex_metadata_client_get_deviceservice
(
  iot_logging_client *lc,
  edgex_service_endpoints * endpoints,
  const char * name,
  edgex_error *err
);
char * edgex_metadata_client_create_deviceservice
(
  iot_logging_client *lc,
  edgex_service_endpoints * endpoints,
  const edgex_deviceservice * newds,
  edgex_error *err
);
edgex_scheduleevent * edgex_metadata_client_get_scheduleevents
(
  iot_logging_client *lc,
  edgex_service_endpoints * endpoints,
  const char * servicename,
  edgex_error *err
);
edgex_scheduleevent * edgex_metadata_client_create_scheduleevent
(
  iot_logging_client *lc,
  edgex_service_endpoints * endpoints,
  const char * name,
  uint64_t origin,
  const char *schedule_name,
  const char *addressable_name,
  const char *parameters,
  const char *service_name,
  edgex_error *err
);
edgex_schedule * edgex_metadata_client_get_schedule
(
  iot_logging_client *lc,
  edgex_service_endpoints * endpoints,
  const char * schedulename,
  edgex_error *err
);
edgex_schedule * edgex_metadata_client_create_schedule
(
  iot_logging_client *lc,
  edgex_service_endpoints * endpoints,
  const char * name,
  uint64_t origin,
  const char *frequency,
  const char *start,
  const char *end,
  bool runOnce,
  edgex_error *err
);
edgex_device * edgex_metadata_client_get_devices
(
  iot_logging_client *lc,
  edgex_service_endpoints * endpoints,
  const char * servicename,
  edgex_error *err
);
edgex_device * edgex_metadata_client_add_device
(
  iot_logging_client *lc,
  edgex_service_endpoints * endpoints,
  const char * name,
  const char * description,
  const edgex_strings * labels,
  uint64_t origin,
  const char * addressable_name,
  const char * service_name,
  const char * profile_name,
  edgex_error *err
);
edgex_device * edgex_metadata_client_get_device
(
  iot_logging_client * lc,
  edgex_service_endpoints * endpoints,
  const char * deviceid,
  edgex_error * err
);
edgex_device * edgex_metadata_client_get_device_byname
(
  iot_logging_client * lc,
  edgex_service_endpoints * endpoints,
  const char * devicename,
  edgex_error * err
);
void edgex_metadata_client_update_device
(
  iot_logging_client * lc,
  edgex_service_endpoints * endpoints,
  const char * name,
  const char * id,
  const char * description,
  const edgex_strings * labels,
  const char * profile_name,
  edgex_error * err
);
void edgex_metadata_client_delete_device
(
  iot_logging_client * lc,
  edgex_service_endpoints * endpoints,
  const char * deviceid,
  edgex_error * err
);
void edgex_metadata_client_delete_device_byname
(
  iot_logging_client * lc,
  edgex_service_endpoints * endpoints,
  const char * devicename,
  edgex_error * err
);
edgex_addressable * edgex_metadata_client_get_addressable
(
  iot_logging_client * lc,
  edgex_service_endpoints * endpoints,
  const char * name,
  edgex_error * err
);
char * edgex_metadata_client_create_addressable
(
  iot_logging_client * lc,
  edgex_service_endpoints * endpoints,
  const edgex_addressable * newadd,
  edgex_error * err
);
void edgex_metadata_client_update_addressable
(
  iot_logging_client * lc,
  edgex_service_endpoints * endpoints,
  const edgex_addressable * addressable,
  edgex_error * err
);
bool edgex_metadata_client_ping
(
  iot_logging_client * lc,
  edgex_service_endpoints * endpoints,
  edgex_error * err
);

#endif
