/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_BASE64_H
#define _EDGEX_DEVICE_BASE64_H 1

#include <stddef.h>
#include <stdbool.h>

extern size_t edgex_b64_encodesize (size_t binsize);

extern size_t edgex_b64_maxdecodesize (const char *in);

extern bool edgex_b64_decode (const char *in, void *out, size_t *outLen);

extern bool edgex_b64_encode
  (const void *in, size_t inLen, char *out, size_t outLen);

#endif

