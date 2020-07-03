/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_EVENTGEN_H_
#define _EDGEX_EVENTGEN_H_ 1

/**
 * @file
 * @brief This file defines the functions and callbacks relating to event
 *        generation other than from incoming REST requests.
 */

#include "edgex/edgex.h"
#include "edgex/error.h"

/**
 * @brief Post readings to the core-data service. This method allows readings
 *        to be generated other than in response to a device GET invocation.
 * @param svc The device service.
 * @param device_name The name of the device that the readings have come from.
 * @param resource_name Name of the resource or command which defines the Event.
 * @param values An array of readings. These will be combined into an Event
 *        and submitted to core-data.
 */

void edgex_device_post_readings
(
  edgex_device_service *svc,
  const char *device_name,
  const char *resource_name,
  edgex_device_commandresult *values
);

/**
 * @brief Callback function requesting that automatic events should begin. 
 *        These should be generated according to the schedule given, and posted
 *        into EdgeX using edgex_device_post_readings().
 * @param impl The context data passed in when the service was created.
 * @param devname The name of the device to be queried.
 * @param protocols The location of the device to be queried.
 * @param resource_name The resource on which autoevents have been configured.
 * @param nreadings The number of readings requested.
 * @param requests An array specifying the readings that have been requested.
 * @param interval The time between events, in milliseconds.
 * @param onChange If true, events should only be generated if one or more
 *        readings have changed.
 * @return A pointer to a data structure that will be provided in a subsequent
 *         call to the stop handler.
 */

typedef void * (*edgex_device_autoevent_start_handler)
(
  void *impl,
  const char *devname,
  const devsdk_protocols *protocols,
  const char *resource_name,
  uint32_t nreadings,
  const edgex_device_commandrequest *requests,
  uint64_t interval,
  bool onChange
);

/**
 * @brief Callback function requesting that automatic events should cease.
 * @param impl The context data passed in when the service was created.
 * @param handle The data structure returned by a previous call to the start
 *        handler.
 */

typedef void (*edgex_device_autoevent_stop_handler)
(
  void *impl,
  void *handle
);

/**
 * @brief This function allows a device service implementation to provide its
 *        own mechanism for generating automatic events. This may be useful
 *        in scenarios where the hardware can be configured to push readings on
 *        a schedule. Handlers must be registerd before starting the service.
 * @param svc The device service.
 * @param starter Function to handle requests to start generating events.
 * @param stopper Function to handle requests to stop generating events.
 */

void edgex_device_register_autoevent_handlers
(
  edgex_device_service *svc,
  edgex_device_autoevent_start_handler starter,
  edgex_device_autoevent_stop_handler stopper
);

#endif
