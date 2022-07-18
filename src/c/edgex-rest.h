/*
 * Copyright (c) 2018-2022
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_REST_H_
#define _EDGEX_REST_H_ 1

#include "edgex/edgex.h"
#include "edgex2.h"
#include "rest-server.h"

devsdk_strings *devsdk_strings_dup (const devsdk_strings *strs);
void devsdk_strings_free (devsdk_strings *strs);
char *devsdk_nvpairs_write (const devsdk_nvpairs *e);
devsdk_nvpairs *devsdk_nvpairs_read (const JSON_Object *obj);
devsdk_nvpairs *devsdk_nvpairs_dup (const devsdk_nvpairs *p);
void devsdk_nvpairs_free (devsdk_nvpairs *p);
JSON_Value *edgex_wrap_request (const char *objName, JSON_Value *payload);
JSON_Value *edgex_wrap_request_single (const char *objName, JSON_Value *payload);
const char *edgex_propertytype_tostring (edgex_propertytype pt);
bool edgex_propertytype_fromstring (edgex_propertytype *res, const char *str);
iot_typecode_t *edgex_propertytype_totypecode (edgex_propertytype pt);
devsdk_protocols *devsdk_protocols_dup (const devsdk_protocols *e);
void devsdk_protocols_free (devsdk_protocols *e);
edgex_deviceprofile *edgex_deviceprofile_read (iot_logger_t *lc, const char *json);
devsdk_device_resources *edgex_profile_toresources (const edgex_deviceprofile *p);
edgex_deviceprofile *edgex_deviceprofile_dup (const edgex_deviceprofile *e);
void edgex_deviceprofile_free (devsdk_service_t *svc, edgex_deviceprofile *e);
edgex_deviceservice *edgex_deviceservice_read (const char *json);
void edgex_deviceservice_free (edgex_deviceservice *e);
void edgex_device_autoevents_free (edgex_device_autoevents *e);
char *edgex_device_write (const edgex_device *e);
char *edgex_device_write_sparse (const char *name, const char *description, const devsdk_strings *labels, const char *profile_name);
edgex_device *edgex_device_dup (const edgex_device *e);
devsdk_devices *edgex_device_todevsdk (devsdk_service_t *svc, const edgex_device *e);
void edgex_device_free (devsdk_service_t *svc, edgex_device *e);
edgex_device *edgex_devices_read (iot_logger_t *lc, const char *json);
edgex_watcher *edgex_watchers_read (const char *json);
edgex_watcher *edgex_watcher_dup (const edgex_watcher *e);
void edgex_watcher_free (edgex_watcher *e);

// V2 DTOs

edgex_baserequest *edgex_baserequest_read (devsdk_http_data d);
void edgex_baserequest_free (edgex_baserequest *e);

void edgex_baseresponse_populate (edgex_baseresponse *e, const char *version, int code, const char *msg);

edgex_errorresponse *edgex_errorresponse_create (uint64_t code, char *msg);
void edgex_errorresponse_write (const edgex_errorresponse *er, devsdk_http_reply *reply);
void edgex_errorresponse_free (edgex_errorresponse *e);

void edgex_baseresponse_write (const edgex_baseresponse *br, devsdk_http_reply *reply);
void edgex_pingresponse_write (const edgex_pingresponse *pr, devsdk_http_reply *reply);
void edgex_configresponse_write (const edgex_configresponse *cr, devsdk_http_reply *reply);
void edgex_configresponse_free (edgex_configresponse *cr);
void edgex_metricsresponse_write (const edgex_metricsresponse *mr, devsdk_http_reply *reply);

char *edgex_createDSreq_write (const edgex_deviceservice *ds);
char *edgex_updateDSreq_write (const char *name, const char *baseaddr);
edgex_deviceservice *edgex_getDSresponse_read (const char *json);

edgex_deviceprofile *edgex_getprofileresponse_read (iot_logger_t *lc, const char *json);

char *edgex_createdevicereq_write (const edgex_device *dev);
edgex_device *edgex_createdevicereq_read (const char *json);
char *edgex_updateDevOpreq_write (const char *name, edgex_device_operatingstate opstate);
char *edgex_updateDevLCreq_write (const char *name, uint64_t lastconnected);

edgex_watcher *edgex_createPWreq_read (const char *json);

#endif
