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
#include "edgex/edgex_logging.h"

/**
 * @brief Post readings to the core-data service. This method allows readings
 *        to be generated other than in response to a device GET invocation.
 * @param svc The device service.
 * @param device_name The name of the device that the readings have come from.
 * @param resource_name Name of the resource or command which defines the Event.
 * @param values An array of readings. These will be combined into an Event
 *        and submitted to core-data. For readings of String or Binary type,
 *        the SDK takes ownership of the memory containing the string or
 *        byte array.
 */

void edgex_device_post_readings
(
  edgex_device_service *svc,
  const char *device_name,
  const char *resource_name,
  const edgex_device_commandresult *values
);

typedef void * (*edgex_device_autoevent_start_handler)
(
  void *impl,
  const char *devname,
  const edgex_protocols *protocols,
  const char *resource_name,
  uint32_t nreadings,
  const edgex_device_commandrequest *requests,
  uint64_t interval,
  bool onChange
);

typedef void (*edgex_device_autoevent_stop_handler)
(
  void *impl,
  void *handle
);

void edgex_device_register_autoevent_handlers
(
  edgex_device_service *svc,
  edgex_device_autoevent_start_handler starter,
  edgex_device_autoevent_stop_handler stopper
);

#endif
