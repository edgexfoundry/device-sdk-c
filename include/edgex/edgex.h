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

typedef enum
{
  GET = 1,
  POST = 2,
  PUT = 4,
  PATCH = 8,
  DELETE = 16,
  UNKNOWN = 1024
} edgex_http_method;

typedef enum
{
  HTTP = 0,
  TCP = 1,
  MAC = 2,
  ZMQ = 3,
  OTHER = 4,
  SSL = 5
} edgex_protocol;

typedef struct
{
  char *id;
  int64_t created;
  int64_t modified;
  int64_t origin;
} edgex_baseobject;

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
  char *adminState;
  uint64_t created;
  char *description;
  char *id;
  edgex_strings *labels;
  uint64_t lastConnected;
  uint64_t lastReported;
  uint64_t modified;
  char *name;
  char *operatingState;
  uint64_t origin;
} edgex_deviceservice;

typedef struct edgex_valuedescriptor
{
  uint64_t created;
  char *defaultValue;
  char *description;
  char *formatting;
  char *id;
  edgex_strings *labels;
  char *max;
  char *min;
  uint64_t modified;
  char *name;
  uint64_t origin;
  char *type;
  char *uomLabel;
} edgex_valuedescriptor;

typedef struct edgex_response
{
  char *code;
  char *description;
  edgex_strings *expectedvalues;
  struct edgex_response *next;
} edgex_response;

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
  char *type;
  char *readwrite;
  char *minimum;
  char *maximum;
  char *defaultvalue;
  char *size;
  char *word;
  char *lsb;
  char *mask;
  char *shift;
  char *scale;
  char *offset;
  char *base;
  char *assertion;
  bool issigned;
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

typedef struct edgex_deviceobject
{
  char *description;
  char *name;
  char *tag;
  edgex_profileproperty *properties;
  edgex_nvpairs *attributes;
  struct edgex_deviceobject *next;
} edgex_deviceobject;

typedef struct
{
  char *path;
  edgex_response *responses;
} edgex_get;

typedef struct
{
  char *path;
  edgex_response *responses;
  edgex_strings *parameter_names;
} edgex_put;

typedef struct edgex_command
{
  char *id;
  uint64_t created;
  uint64_t modified;
  uint64_t origin;
  char *name;
  edgex_put *put;
  edgex_get *get;
  struct edgex_command *next;
} edgex_command;

typedef struct edgex_profileresource
{
  char *name;
  edgex_resourceoperation *set;
  edgex_resourceoperation *get;
  struct edgex_profileresource *next;
} edgex_profileresource;

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
  edgex_command *commands;
  edgex_deviceobject *device_resources;
  edgex_profileresource *resources;
} edgex_deviceprofile;

typedef struct edgex_device
{
  edgex_addressable *addressable;
  char *adminState;
  uint64_t created;
  char *description;
  char *id;
  edgex_strings *labels;
  uint64_t lastConnected;
  uint64_t lastReported;
  uint64_t modified;
  char *name;
  char *operatingState;
  uint64_t origin;
  edgex_deviceservice *service;
  edgex_deviceprofile *profile;
  struct edgex_device *next;
} edgex_device;

typedef struct edgex_reading
{
  uint64_t created;
  char *id;
  uint64_t modified;
  char *name;
  uint64_t origin;
  uint64_t pushed;
  char *value;
  struct edgex_reading *next;
} edgex_reading;

typedef struct edgex_event
{
  uint64_t created;
  char *device;
  char *id;
  uint64_t modified;
  uint64_t origin;
  uint64_t pushed;
  edgex_reading *readings;
  struct edgex_event *next;
} edgex_event;

#define EDGEX_LIST(T)    \
struct edgex_##T##_list; \
typedef struct edgex_##T##_list edgex_##T##_list; \
typedef struct edgex_##T##_list \
{ \
   edgex_##T *entry; \
   edgex_##T##_list *next; \
} edgex_##T##_list;

EDGEX_LIST(device)

#endif
