/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_TRANSFORM_H_
#define _EDGEX_DEVICE_TRANSFORM_H_ 1

#include "edgex/devsdk.h"
#include "edgex/edgex.h"

void edgex_transform_outgoing
  (edgex_device_commandresult *cres, edgex_propertyvalue *props, edgex_nvpairs *mappings);

bool edgex_transform_incoming
  (edgex_device_commandresult *cres, edgex_propertyvalue *props, edgex_nvpairs *mappings);

#endif
