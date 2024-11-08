/*
 * Copyright (c) 2018-2022
 * Eaton Corp
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_KEEPER_H_
#define _EDGEX_KEEPER_H_ 1

#include "registry-impl.h"
#include "devsdk/devsdk-base.h"

#define KEEPER_PUBLISH_PREFIX "edgex/configs/"

void *devsdk_registry_keeper_alloc (devsdk_service_t *service);
extern const devsdk_registry_impls devsdk_registry_keeper_fns;

#endif
