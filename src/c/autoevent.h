/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <semaphore.h>

#ifndef _EDGEX_DEVICE_AUTOEVENT_H
#define _EDGEX_DEVICE_AUTOEVENT_H 1

#include "service.h"

sem_t mstr_mutex;

sem_t slv_mutex;

void edgex_device_autoevent_start (devsdk_service_t *svc, edgex_device *dev);

void edgex_device_autoevent_stop (edgex_device *dev);

void edgex_device_autoevent_stop_to_update (edgex_device *dev);

void clear_ae_signals ();

#endif
