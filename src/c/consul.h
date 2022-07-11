/*
 * Copyright (c) 2018-2022
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_CONSUL_H_
#define _EDGEX_CONSUL_H_ 1

#include "registry-impl.h"

void *devsdk_registry_consul_alloc (void);
extern const devsdk_registry_impls devsdk_registry_consul_fns;

#endif
