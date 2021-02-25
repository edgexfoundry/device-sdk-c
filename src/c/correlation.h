/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_CORRELATION_H
#define _EDGEX_DEVICE_CORRELATION_H 1

#define EDGEX_CRLID_HDR "correlation-id"

char *edgex_device_genuuid (void);

const char *edgex_device_get_crlid (void);
void edgex_device_alloc_crlid (const char *id);
void edgex_device_free_crlid (void);

#endif
