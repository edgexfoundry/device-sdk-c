/*
 * Copyright (c) 2021
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_SECRETSTORE_H
#define _EDGEX_SECRETSTORE_H 1

/**
 * @file
 * @brief This file defines the functions for secret store access provided by the SDK.
 */

#include "devsdk/devsdk.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Obtain secret credentials.
 * @param svc The device service.
 * @param name The path to search for secrets.
 */

devsdk_nvpairs *edgex_get_secrets (devsdk_service_t *svc, const char *path);

#ifdef __cplusplus
}
#endif

#endif
