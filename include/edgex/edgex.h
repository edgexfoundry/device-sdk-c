/*
 * Copyright (c) 2018-2020
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
  char *parameter;
  devsdk_nvpairs *mappings;
  struct edgex_resourceoperation *next;
} edgex_resourceoperation;

typedef struct
{
  edgex_propertytype type;
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
  char *tag;
  edgex_propertyvalue *properties;
  devsdk_nvpairs *attributes;
  struct edgex_deviceresource *next;
} edgex_deviceresource;

typedef struct edgex_devicecommand
{
  char *name;
  edgex_resourceoperation *set;
  edgex_resourceoperation *get;
  struct edgex_devicecommand *next;
} edgex_devicecommand;

struct edgex_cmdinfo;
struct edgex_autoimpl;

typedef struct edgex_deviceprofile
{
  char *name;
  char *description;
  uint64_t created;
  uint64_t modified;
  uint64_t origin;
  char *manufacturer;
  char *model;
  devsdk_strings *labels;
  edgex_deviceresource *device_resources;
  edgex_devicecommand *device_commands;
  struct edgex_cmdinfo *cmdinfo;
  struct edgex_deviceprofile *next;
} edgex_deviceprofile;

typedef struct edgex_blocklist
{
  char *name;
  devsdk_strings *values;
  struct edgex_blocklist *next;
} edgex_blocklist;

typedef struct edgex_watcher
{
  char *name;
  devsdk_nvpairs *identifiers;
  struct edgex_watcher_regexes_t *regs;
  edgex_blocklist *blocking_identifiers;
  char *profile;
  edgex_device_adminstate adminstate;
  struct edgex_watcher *next;
} edgex_watcher;

typedef struct edgex_device_autoevents
{
  char *resource;
  char *frequency;
  bool onChange;
  struct edgex_autoimpl *impl;
  struct edgex_device_autoevents *next;
} edgex_device_autoevents;

typedef struct edgex_device
{
  devsdk_protocols *protocols;
  edgex_device_adminstate adminState;
  uint64_t created;
  char *description;
  devsdk_strings *labels;
  char *name;
  edgex_device_operatingstate operatingState;
  uint64_t origin;
  edgex_device_autoevents *autos;
  char *servicename;
  edgex_deviceprofile *profile;
  struct edgex_device *next;
  atomic_uint_fast32_t refs;
  bool ownprofile;
} edgex_device;

#endif
