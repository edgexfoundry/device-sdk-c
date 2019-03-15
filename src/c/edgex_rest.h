/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_REST_H_
#define _EDGEX_REST_H_ 1

#include "edgex/edgex.h"
#include "iot/logging.h"
#include "schedules.h"
#include "data.h"

edgex_strings *edgex_strings_dup (const edgex_strings *strs);
void edgex_strings_free (edgex_strings *strs);
edgex_nvpairs *edgex_nvpairs_dup (edgex_nvpairs *p);
void edgex_nvpairs_free (edgex_nvpairs *p);
const char *edgex_propertytype_tostring (edgex_propertytype pt);
bool edgex_propertytype_fromstring (edgex_propertytype *res, const char *str);
edgex_protocols *edgex_protocols_dup (const edgex_protocols *e);
void edgex_protocols_free (edgex_protocols *e);
edgex_deviceprofile *edgex_deviceprofile_read (iot_logging_client *lc, const char *json);
char *edgex_deviceprofile_write (const edgex_deviceprofile *e, bool create);
edgex_deviceprofile *edgex_deviceprofile_dup (edgex_deviceprofile *e);
void edgex_deviceprofile_free (edgex_deviceprofile *e);
edgex_deviceservice *edgex_deviceservice_read (const char *json);
char *edgex_deviceservice_write (const edgex_deviceservice *e, bool create);
edgex_deviceservice *edgex_deviceservice_dup (const edgex_deviceservice *e);
void edgex_deviceservice_free (edgex_deviceservice *e);
edgex_device *edgex_device_read (iot_logging_client *lc, const char *json);
char *edgex_device_write (const edgex_device *e, bool create);
char *edgex_device_write_sparse (const char *name, const char *id, const char *description, const edgex_strings *labels, const char *profile_name);
edgex_device *edgex_device_dup (const edgex_device *e);
void edgex_device_free (edgex_device *e);
edgex_device *edgex_devices_read (iot_logging_client *lc, const char *json);
edgex_scheduleevent *edgex_scheduleevents_read (const char *json);
char *edgex_scheduleevent_write (const edgex_scheduleevent *e, bool create);
void edgex_scheduleevent_free (edgex_scheduleevent *e);
edgex_schedule *edgex_schedule_read (const char *json);
char *edgex_schedule_write (const edgex_schedule *e, bool create);
void edgex_schedule_free (edgex_schedule *e);
edgex_addressable *edgex_addressable_read (const char *json);
char *edgex_addressable_write (const edgex_addressable *e, bool create);
edgex_addressable *edgex_addressable_dup (edgex_addressable *e);
void edgex_addressable_free (edgex_addressable *e);
edgex_valuedescriptor *edgex_valuedescriptor_read (const char *json);
char *edgex_valuedescriptor_write (const edgex_valuedescriptor *e);
void edgex_valuedescriptor_free (edgex_valuedescriptor *e);

#ifdef EDGEX_DEBUG_DUMP
void edgex_deviceprofile_dump (edgex_deviceprofile * e);
void edgex_deviceservice_dump (edgex_deviceservice * e);
void edgex_addressable_dump (edgex_addressable * e);
void edgex_device_dump (edgex_device * e);
void edgex_valuedescriptor_dump (edgex_valuedescriptor * e);
#endif

#endif
