/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_EDGEX_H_
#define _EDGEX_EDGEX_H_

#include "edgex/edgex-base.h"

typedef enum { LOCKED, UNLOCKED } edgex_device_adminstate;

typedef enum { ENABLED, DISABLED } edgex_device_operatingstate;

typedef struct
{
  bool enabled;
  union
  {
    int64_t ival;
    double dval;
  } value;
} edgex_transformArg;

typedef struct
{
  char *address;
  uint64_t created;
  char *id;
  char *method;
  uint64_t modified;
  char *name;
  uint64_t origin;
  char *password;
  char *path;
  uint64_t port;
  char *protocol;
  char *publisher;
  char *topic;
  char *user;
} edgex_addressable;

typedef struct
{
  edgex_addressable *addressable;
  edgex_device_adminstate adminState;
  uint64_t created;
  char *description;
  char *id;
  edgex_strings *labels;
  uint64_t lastConnected;
  uint64_t lastReported;
  uint64_t modified;
  char *name;
  edgex_device_operatingstate operatingState;
  uint64_t origin;
} edgex_deviceservice;

typedef struct edgex_resourceoperation
{
  char *index;
  char *operation;
  char *object;
  char *property;
  char *parameter;
  char *resource;
  edgex_strings *secondary;
  edgex_nvpairs *mappings;
  struct edgex_resourceoperation *next;
} edgex_resourceoperation;

typedef struct
{
  edgex_propertytype type;
  bool readable;
  bool writable;
  char *minimum;
  char *maximum;
  char *defaultvalue;
  char *lsb;
  edgex_transformArg mask;
  edgex_transformArg shift;
  edgex_transformArg scale;
  edgex_transformArg offset;
  edgex_transformArg base;
  char *assertion;
  char *precision;
  char *mediaType;
  bool floatAsBinary;
} edgex_propertyvalue;

typedef struct
{
  char *type;
  char *readwrite;
  char *defaultvalue;
} edgex_units;

typedef struct
{
  edgex_propertyvalue *value;
  edgex_units *units;
} edgex_profileproperty;

typedef struct edgex_deviceresource
{
  char *description;
  char *name;
  char *tag;
  edgex_profileproperty *properties;
  edgex_nvpairs *attributes;
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
  char *id;
  char *name;
  char *description;
  uint64_t created;
  uint64_t modified;
  uint64_t origin;
  char *manufacturer;
  char *model;
  edgex_strings *labels;
  edgex_deviceresource *device_resources;
  edgex_devicecommand *device_commands;
  struct edgex_cmdinfo *cmdinfo;
  struct edgex_deviceprofile *next;
} edgex_deviceprofile;

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
  edgex_protocols *protocols;
  edgex_device_adminstate adminState;
  uint64_t created;
  char *description;
  char *id;
  edgex_strings *labels;
  uint64_t lastConnected;
  uint64_t lastReported;
  uint64_t modified;
  char *name;
  edgex_device_operatingstate operatingState;
  uint64_t origin;
  edgex_device_autoevents *autos;
  edgex_deviceservice *service;
  edgex_deviceprofile *profile;
  struct edgex_device *next;
  atomic_uint_fast32_t refs;
} edgex_device;

#endif
