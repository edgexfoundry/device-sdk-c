/*
 * Copyright (c) 2018-2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_EDGEX_H_
#define _EDGEX_EDGEX_H_

#include "devsdk/devsdk-base.h"
#include "edgex/edgex-base.h"

typedef struct
{
  bool enabled;
  union
  {
    int64_t ival;
    double dval;
  } value;
} edgex_transformArg;

typedef struct edgex_deviceservice
{
  char *name;
  char *description;
  char *baseaddress;
  devsdk_strings *labels;
  uint64_t lastConnected;
  uint64_t lastReported;
  uint64_t origin;
  edgex_device_adminstate adminState;
} edgex_deviceservice;

typedef struct edgex_resourceoperation
{
  char *deviceResource;
  char *defaultValue;
  iot_data_t *mappings;
  struct edgex_resourceoperation *next;
} edgex_resourceoperation;

typedef struct
{
  iot_typecode_t type;
  char *units;
  bool readable;
  bool writable;
  edgex_transformArg minimum;
  edgex_transformArg maximum;
  char *defaultvalue;
  edgex_transformArg mask;
  edgex_transformArg shift;
  edgex_transformArg scale;
  edgex_transformArg offset;
  edgex_transformArg base;
  char *assertion;
  char *mediaType;
} edgex_propertyvalue;

typedef struct edgex_deviceresource
{
  char *description;
  char *name;
  edgex_propertyvalue *properties;
  iot_data_t *attributes;
  iot_data_t *tags;
  devsdk_resource_attr_t parsed_attrs;
  struct edgex_deviceresource *next;
} edgex_deviceresource;

typedef struct edgex_devicecommand
{
  char *name;
  edgex_resourceoperation *resourceOperations;
  bool readable;
  bool writable;
  iot_data_t *tags;
  struct edgex_devicecommand *next;
} edgex_devicecommand;

struct edgex_cmdinfo;
struct edgex_autoimpl;

typedef struct edgex_deviceprofile
{
  char *name;
  char *description;
  char *manufacturer;
  char *model;
  devsdk_strings *labels;
  edgex_deviceresource *device_resources;
  edgex_devicecommand *device_commands;
  struct edgex_cmdinfo *cmdinfo;
  struct edgex_deviceprofile *next;
} edgex_deviceprofile;

typedef struct edgex_watcher
{
  char *name;
  iot_data_t *identifiers;
  struct edgex_watcher_regexes_t *regs;
  iot_data_t *blocking_identifiers;
  char *profile;
  edgex_device_adminstate adminstate;
  struct edgex_device_autoevents *autoevents;
  bool enabled;
  struct edgex_watcher *next;
} edgex_watcher;

typedef struct edgex_device_autoevents
{
  char *resource;
  char *interval;
  bool onChange;
  double onChangeThreshold;
  struct edgex_autoimpl *impl;
  struct edgex_device_autoevents *next;
} edgex_device_autoevents;

typedef struct edgex_device
{
  devsdk_protocols *protocols;
  devsdk_device_t *devimpl;
  edgex_device_adminstate adminState;
  uint64_t created;
  char *description;
  devsdk_strings *labels;
  iot_data_t *tags;
  char *name;
  char *parent;
  edgex_device_operatingstate operatingState;
  uint64_t origin;
  edgex_device_autoevents *autos;
  char *servicename;
  edgex_deviceprofile *profile;
  struct edgex_device *next;
  atomic_uint_fast32_t refs;
  atomic_int_fast32_t retries;
  bool ownprofile;
} edgex_device;

#endif
