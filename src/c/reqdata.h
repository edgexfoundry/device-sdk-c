/*
 * Copyright (c) 2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_REQDATA_H_
#define _EDGEX_DEVICE_REQDATA_H_ 1

#include "edgex/rest-server.h"

struct edgex_reqdata_t;
typedef struct edgex_reqdata_t edgex_reqdata_t;

extern edgex_reqdata_t *edgex_reqdata_parse (iot_logger_t *lc, const devsdk_http_request *req);

extern const char *edgex_reqdata_get (const edgex_reqdata_t *data, const char *name, const char *dfl);

extern iot_data_t *edgex_reqdata_get_binary (const edgex_reqdata_t *data, const char *name);

extern void edgex_reqdata_free (edgex_reqdata_t *data);

#endif
