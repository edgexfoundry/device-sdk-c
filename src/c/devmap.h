/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_DEVMAP_H_
#define _EDGEX_DEVICE_DEVMAP_H_ 1

/* Maintains the device / profile map for the SDK. */

#include "devsdk/devsdk.h"
#include "edgex/edgex.h"

struct edgex_devmap_t;
typedef struct edgex_devmap_t edgex_devmap_t;

/*
 * Possible outcomes of a 'replace' operation
 */

typedef enum
{
  UPDATED_SDK,     // device fields relevant only to the SDK sere updated
  UPDATED_DRIVER,  // device fields relevant to the SDK and driver were updated
  CREATED          // device was created
} edgex_devmap_outcome_t;

/*
 * A list of devices matching a command.
 */

typedef struct edgex_cmdqueue_t
{
   edgex_device *dev;
   const struct edgex_cmdinfo *cmd;
   struct edgex_cmdqueue_t *next;
} edgex_cmdqueue_t;

/*
 * Device[Profile] map lifecycle.
 */

extern edgex_devmap_t *edgex_devmap_alloc (devsdk_service_t *svc);
extern void edgex_devmap_clear (edgex_devmap_t *map);
extern void edgex_devmap_free (edgex_devmap_t *map);

/*
 * These functions copy devices and profiles in and out.
 */

extern void edgex_devmap_populate_devices
  (edgex_devmap_t *map, const edgex_device *devs);
extern edgex_device *edgex_devmap_copydevices (edgex_devmap_t *map);
extern devsdk_devices *edgex_devmap_copydevices_generic (edgex_devmap_t *map);
extern edgex_deviceprofile *edgex_devmap_copyprofiles (edgex_devmap_t *map);
extern edgex_devmap_outcome_t edgex_devmap_replace_device
  (edgex_devmap_t *map, const edgex_device *dev);

/*
 * These functions return pointers to the devices held in the implementation.
 * They must be released after use by calling edgex_device_release().
 */

extern edgex_device *edgex_devmap_device_byid
  (edgex_devmap_t *map, const char *id);
extern edgex_device *edgex_devmap_device_byname
  (edgex_devmap_t *map, const char *name);
extern edgex_cmdqueue_t *edgex_devmap_device_forcmd
  (edgex_devmap_t *map, const char *cmd, bool forGet);

/*
 * Remove a profile but only if there are no associated devices
 */

extern bool edgex_devmap_remove_profile (edgex_devmap_t *map, const char *id);

/*
 * Release function. The device is freed when its reference count hits zero.
 */

extern void edgex_device_release (edgex_device *dev);

/*
 * Device removal.
 */

extern void edgex_devmap_removedevice_byid
  (edgex_devmap_t *map, const char *id);
extern void edgex_devmap_removedevice_byname
  (edgex_devmap_t *map, const char *name);

/*
 * Add and retrieve profiles. We take ownership on add, and return pointers
 * to the profiles held in the implementation. Unlike devices these are not
 * refcounted and do not need to be released or freed.
 */

extern void edgex_devmap_add_profile
  (edgex_devmap_t *map, edgex_deviceprofile *dp);
extern const edgex_deviceprofile *edgex_devmap_profile
  (edgex_devmap_t *map, const char *name);

#endif
