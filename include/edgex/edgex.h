/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_EDGEX_H_
#define _EDGEX_EDGEX_H_

#include "edgex/os.h"

typedef enum
{
  Bool,
  String,
  Binary,
  Uint8, Uint16, Uint32, Uint64,
  Int8, Int16, Int32, Int64,
  Float32, Float64
} edgex_propertytype;

typedef enum { LOCKED, UNLOCKED } edgex_device_adminstate;

typedef enum { ENABLED, DISABLED } edgex_device_operatingstate;

typedef struct edgex_nvpairs
{
  char *name;
  char *value;
  struct edgex_nvpairs *next;
} edgex_nvpairs;

typedef struct edgex_strings
{
  char *str;
  struct edgex_strings *next;
} edgex_strings;

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

typedef struct edgex_profileresource
{
  char *name;
  edgex_resourceoperation *set;
  edgex_resourceoperation *get;
  struct edgex_profileresource *next;
} edgex_profileresource;

struct edgex_cmdinfo;

typedef struct
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
  edgex_profileresource *resources;
  struct edgex_cmdinfo *cmdinfo;
} edgex_deviceprofile;

typedef struct edgex_device
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
  edgex_deviceservice *service;
  edgex_deviceprofile *profile;
  struct edgex_device *next;
} edgex_device;

#endif
