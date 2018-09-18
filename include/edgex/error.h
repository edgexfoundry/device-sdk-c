/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_ERROR_H_
#define _EDGEX_ERROR_H_ 1

#include "edgex/os.h"

typedef struct edgex_error
{
  uint32_t code;
  const char *reason;
} edgex_error;

#endif
