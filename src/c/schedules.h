/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EGDEX_SCHEDULES_H_
#define _EGDEX_SCHEDULES_H_ 1

#include "edgex/edgex.h"

typedef struct edgex_scheduleevent
{
  char *name;
  char *id;
  uint64_t origin;
  uint64_t created;
  uint64_t modified;
  char *schedule;
  edgex_addressable *addressable;
  char *parameters;
  char *service;
  struct edgex_scheduleevent *next;
} edgex_scheduleevent;

typedef struct
{
  char *name;
  char *id;
  uint64_t origin;
  uint64_t created;
  uint64_t modified;
  char *start;
  char *end;
  char *frequency;
  char *cron;
  bool runOnce;
} edgex_schedule;

#endif
