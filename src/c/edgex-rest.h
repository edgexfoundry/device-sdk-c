/*
 * Copyright (c) 2018-2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_REST_H_
#define _EDGEX_REST_H_ 1

#include "edgex/edgex.h"
#include "data.h"
#include "edgex2.h"
#include "rest-server.h"

devsdk_strings *devsdk_strings_dup (const devsdk_strings *strs);
void devsdk_strings_free (devsdk_strings *strs);
devsdk_nvpairs *devsdk_nvpairs_dup (const devsdk_nvpairs *p);
void devsdk_nvpairs_free (devsdk_nvpairs *p);
const char *edgex_propertytype_tostring (edgex_propertytype pt);
bool edgex_propertytype_fromstring (edgex_propertytype *res, const char *str);
iot_typecode_t *edgex_propertytype_totypecode (edgex_propertytype pt);
devsdk_protocols *devsdk_protocols_dup (const devsdk_protocols *e);
void devsdk_protocols_free (devsdk_protocols *e);
edgex_deviceprofile *edgex_deviceprofile_read (iot_logger_t *lc, const char *json);
char *edgex_deviceprofile_write (const edgex_deviceprofile *e, bool create);
devsdk_device_resources *edgex_profile_toresources (const edgex_deviceprofile *p);
edgex_deviceprofile *edgex_deviceprofile_dup (const edgex_deviceprofile *e);
void edgex_deviceprofile_free (edgex_deviceprofile *e);
edgex_deviceservice *edgex_deviceservice_read (const char *json);
char *edgex_deviceservice_write (const edgex_deviceservice *e, bool create);
edgex_deviceservice *edgex_deviceservice_dup (const edgex_deviceservice *e);
void edgex_deviceservice_free (edgex_deviceservice *e);
void edgex_device_autoevents_free (edgex_device_autoevents *e);
edgex_device *edgex_device_read (iot_logger_t *lc, const char *json);
char *edgex_device_write (const edgex_device *e);
char *edgex_device_write_sparse (const char *name, const char *id, const char *description, const devsdk_strings *labels, const char *profile_name);
edgex_device *edgex_device_dup (const edgex_device *e);
devsdk_devices *edgex_device_todevsdk (const edgex_device *e);
void edgex_device_free (edgex_device *e);
edgex_device *edgex_devices_read (iot_logger_t *lc, const char *json);
edgex_addressable *edgex_addressable_read (const char *json);
char *edgex_addressable_write (const edgex_addressable *e, bool create);
edgex_addressable *edgex_addressable_dup (const edgex_addressable *e);
void edgex_addressable_free (edgex_addressable *e);
edgex_valuedescriptor *edgex_valuedescriptor_read (const char *json);
char *edgex_valuedescriptor_write (const edgex_valuedescriptor *e);
void edgex_valuedescriptor_free (edgex_valuedescriptor *e);
edgex_watcher *edgex_watcher_read (const char *json);
edgex_watcher *edgex_watchers_read (const char *json);
edgex_watcher *edgex_watcher_dup (const edgex_watcher *e);
void edgex_watcher_free (edgex_watcher *e);

// V2 DTOs

edgex_baserequest *edgex_baserequest_read (devsdk_http_data d);
void edgex_baserequest_free (edgex_baserequest *e);

void edgex_baseresponse_populate (edgex_baseresponse *e, const char *reqId, int code, const char *msg);

edgex_errorresponse *edgex_errorresponse_create (uint64_t code, char *msg);
void edgex_errorresponse_write (const edgex_errorresponse *er, devsdk_http_reply *reply);
void edgex_errorresponse_free (edgex_errorresponse *e);

void edgex_baseresponse_write (const edgex_baseresponse *br, devsdk_http_reply *reply);
void edgex_pingresponse_write (const edgex_pingresponse *pr, devsdk_http_reply *reply);
void edgex_configresponse_write (const edgex_configresponse *cr, devsdk_http_reply *reply);
void edgex_configresponse_free (edgex_configresponse *cr);


#ifdef EDGEX_DEBUG_DUMP
void edgex_deviceprofile_dump (edgex_deviceprofile * e);
void edgex_deviceservice_dump (edgex_deviceservice * e);
void edgex_addressable_dump (edgex_addressable * e);
void edgex_device_dump (edgex_device * e);
void edgex_valuedescriptor_dump (edgex_valuedescriptor * e);
#endif

#endif
