/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

/* Device / profile map implementation. We maintain 3 maps: device by id,
 * device id by name and profile by name. The devices in the device map
 * reference profiles in the profile map by pointer.
 */

#include "devmap.h"
#include "devutil.h"
#include "map.h"
#include "edgex-rest.h"
#include "device.h"
#include "autoevent.h"

typedef edgex_map(edgex_device *) edgex_map_device;
typedef edgex_map(edgex_deviceprofile *) edgex_map_profile;

struct edgex_devmap_t
{
  pthread_rwlock_t lock;
  edgex_map_device devices;
  edgex_map_string name_to_id;
  edgex_map_profile profiles;
  devsdk_service_t *svc;
};

edgex_devmap_t *edgex_devmap_alloc (devsdk_service_t *svc)
{
  pthread_rwlockattr_t rwatt;
  pthread_rwlockattr_init (&rwatt);
#ifdef  __GLIBC_PREREQ
#if __GLIBC_PREREQ(2, 1)
  /* Avoid heavy readlock use (eg spammed "all" commands) blocking updates */
  pthread_rwlockattr_setkind_np
    (&rwatt, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
#endif
#endif

  edgex_devmap_t *res = malloc (sizeof (edgex_devmap_t));
  pthread_rwlock_init (&res->lock, &rwatt);
  pthread_rwlockattr_destroy (&rwatt);
  edgex_map_init (&res->devices);
  edgex_map_init (&res->name_to_id);
  edgex_map_init (&res->profiles);
  res->svc = svc;
  return res;
}

void edgex_devmap_clear (edgex_devmap_t *map)
{
  const char *key;
  const char *next;

  pthread_rwlock_wrlock (&map->lock);
  edgex_map_iter i = edgex_map_iter (map->devices);
  key = edgex_map_next (&map->devices, &i);
  while (key)
  {
    edgex_device **e = edgex_map_get (&map->devices, key);
    edgex_map_remove (&map->name_to_id, (*e)->name);
    edgex_device_release (*e);
    next = edgex_map_next (&map->devices, &i);
    edgex_map_remove (&map->devices, key);
    key = next;
  }
  pthread_rwlock_unlock (&map->lock);
}

void edgex_devmap_free (edgex_devmap_t *map)
{
  const char *key;
  edgex_map_deinit (&map->name_to_id);
  edgex_map_deinit (&map->devices);
  edgex_map_iter i = edgex_map_iter (map->profiles);
  while ((key = edgex_map_next (&map->profiles, &i)))
  {
    edgex_deviceprofile **p = edgex_map_get (&map->profiles, key);
    edgex_deviceprofile_free (*p);
  }
  edgex_map_deinit (&map->profiles);
  pthread_rwlock_destroy (&map->lock);
  free (map);
}

static void add_locked (edgex_devmap_t *map, const edgex_device *newdev)
{
  edgex_device *dup = edgex_device_dup (newdev);
  atomic_store (&dup->refs, 1);
  dup->ownprofile = false;
  edgex_deviceprofile **pp = edgex_map_get (&map->profiles, dup->profile->name);
  if (pp)
  {
    edgex_deviceprofile_free (dup->profile);
    dup->profile = *pp;
  }
  else
  {
    edgex_map_set (&map->profiles, dup->profile->name, dup->profile);
  }
  edgex_map_set (&map->devices, dup->id, dup);
  edgex_map_set (&map->name_to_id, dup->name, dup->id);
  edgex_device_autoevent_start (map->svc, dup);
}

void edgex_devmap_populate_devices
  (edgex_devmap_t *map, const edgex_device *devs)
{
  pthread_rwlock_wrlock (&map->lock);
  for (const edgex_device *d = devs; d; d = d->next)
  {
    if (edgex_map_get (&map->name_to_id, d->name) == NULL)
    {
      add_locked (map, d);
    }
  }
  pthread_rwlock_unlock (&map->lock);
}

devsdk_devices *edgex_devmap_copydevices_generic (edgex_devmap_t *map)
{
  devsdk_devices *result = NULL;
  devsdk_devices *entry = NULL;
  const char *key;

  pthread_rwlock_rdlock (&map->lock);
  edgex_map_iter iter = edgex_map_iter (map->devices);
  while ((key = edgex_map_next (&map->devices, &iter)))
  {
    entry = edgex_device_todevsdk (*edgex_map_get (&map->devices, key));
    entry->next = result;
    result = entry;
  }
  pthread_rwlock_unlock (&map->lock);
  return result;
}

edgex_device *edgex_devmap_copydevices (edgex_devmap_t *map)
{
  edgex_device *result = NULL;
  edgex_device *dup;
  const char *key;

  pthread_rwlock_rdlock (&map->lock);
  edgex_map_iter iter = edgex_map_iter (map->devices);
  while ((key = edgex_map_next (&map->devices, &iter)))
  {
    dup = edgex_device_dup (*edgex_map_get (&map->devices, key));
    dup->next = result;
    result = dup;
  }
  pthread_rwlock_unlock (&map->lock);
  return result;
}

edgex_deviceprofile *edgex_devmap_copyprofiles (edgex_devmap_t *map)
{
  edgex_deviceprofile *result = NULL;
  edgex_deviceprofile *dup;
  const char *key;

  pthread_rwlock_rdlock (&map->lock);
  edgex_map_iter iter = edgex_map_iter (map->profiles);
  while ((key = edgex_map_next (&map->profiles, &iter)))
  {
    dup = edgex_deviceprofile_dup (*edgex_map_get (&map->profiles, key));
    dup->next = result;
    result = dup;
  }
  pthread_rwlock_unlock (&map->lock);
  return result;
}

bool edgex_devmap_remove_profile (edgex_devmap_t *map, const char *id)
{
  const char *key;
  bool result = true;

  pthread_rwlock_wrlock (&map->lock);
  edgex_map_iter iter = edgex_map_iter (map->devices);
  while (result && (key = edgex_map_next (&map->devices, &iter)))
  {
    result = (strcmp ((*edgex_map_get (&map->devices, key))->profile->id, id) != 0);
  }
  if (result)
  {
    edgex_map_iter iter2 = edgex_map_iter (map->profiles);
    while ((key = edgex_map_next (&map->profiles, &iter2)))
    {
      edgex_deviceprofile **dpp = edgex_map_get (&map->profiles, key);
      if (strcmp ((*dpp)->id, id) == 0)
      {
        edgex_deviceprofile_free (*dpp);
        edgex_map_remove (&map->profiles, key);
        break;
      }
    }
  }
  pthread_rwlock_unlock (&map->lock);
  return result;
}

const edgex_deviceprofile *edgex_devmap_profile
  (edgex_devmap_t *map, const char *name)
{
  edgex_deviceprofile **dpp;
  pthread_rwlock_rdlock (&map->lock);
  dpp = edgex_map_get (&map->profiles, name);
  pthread_rwlock_unlock (&map->lock);
  return dpp ? *dpp : NULL;
}

static void remove_locked (edgex_devmap_t *map, edgex_device *olddev)
{
  const char *key;
  bool last = true;
  edgex_map_remove (&map->name_to_id, olddev->name);
  edgex_map_remove (&map->devices, olddev->id);
  edgex_map_iter iter = edgex_map_iter (map->devices);
  while ((key = edgex_map_next (&map->devices, &iter)))
  {
    if ((*edgex_map_get (&map->devices, key))->profile == olddev->profile)
    {
      last = false;
      break;
    }
  }
  if (last)
  {
    edgex_map_remove (&map->profiles, olddev->profile->name);
    olddev->ownprofile = true;
  }
  edgex_device_release (olddev);
}

/* Update a device, but fail if there could be effects on autoevents or
 * operations in progress. For such attempts we will remove the device and
 * add a new one.
 */

static bool update_in_place (edgex_device *dest, const edgex_device *src, edgex_devmap_outcome_t *outcome)
{
  if (!devsdk_protocols_equal ((devsdk_protocols *)dest->protocols, (devsdk_protocols *)src->protocols))
  {
    *outcome = UPDATED_DRIVER;
    return false;
  }
  if (strcmp (dest->name, src->name))
  {
    *outcome = UPDATED_DRIVER;
    return false;
  }
  if (dest->adminState != src->adminState)
  {
    *outcome = UPDATED_DRIVER;
    dest->adminState = src->adminState;
  }
  if (strcmp (dest->profile->name, src->profile->name))
  {
    return false;
  }
  if (!edgex_device_autoevents_equal (dest->autos, src->autos))
  {
    return false;
  }
  dest->operatingState = src->operatingState;
  dest->created = src->created;
  dest->lastConnected = src->lastConnected;
  dest->lastReported = src->lastReported;
  dest->modified = src->modified;
  dest->origin = src->origin;
  free (dest->description);
  dest->description = strdup (src->description);
  devsdk_strings_free (dest->labels);
  dest->labels = devsdk_strings_dup (src->labels);

  return true;
}

edgex_devmap_outcome_t edgex_devmap_replace_device (edgex_devmap_t *map, const edgex_device *dev)
{
  edgex_device **olddev;
  edgex_devmap_outcome_t result = UPDATED_SDK;

  pthread_rwlock_wrlock (&map->lock);
  olddev = edgex_map_get (&map->devices, dev->id);
  if (olddev == NULL)
  {
    add_locked (map, dev);
    result = CREATED;
  }
  else
  {
    if (!update_in_place (*olddev, dev, &result))
    {
      remove_locked (map, *olddev);
      add_locked (map, dev);
    }
  }
  pthread_rwlock_unlock (&map->lock);
  return result;
}

edgex_device *edgex_devmap_device_byid (edgex_devmap_t *map, const char *id)
{
  edgex_device **dev;
  edgex_device *result = NULL;

  pthread_rwlock_rdlock (&map->lock);
  dev = edgex_map_get (&map->devices, id);
  if (dev)
  {
    result = *dev;
    atomic_fetch_add (&result->refs, 1);
  }
  pthread_rwlock_unlock (&map->lock);
  return result;
}

edgex_device *edgex_devmap_device_byname (edgex_devmap_t *map, const char *name)
{
  char **id;
  edgex_device *result = NULL;

  pthread_rwlock_rdlock (&map->lock);
  id = edgex_map_get (&map->name_to_id, name);
  if (id)
  {
    result = *edgex_map_get (&map->devices, *id);
    atomic_fetch_add (&result->refs, 1);
  }
  pthread_rwlock_unlock (&map->lock);
  return result;
}

void edgex_devmap_removedevice_byname (edgex_devmap_t *map, const char *name)
{
  edgex_device **olddev;
  char **id;

  pthread_rwlock_wrlock (&map->lock);
  id = edgex_map_get (&map->name_to_id, name);
  if (id)
  {
    olddev = edgex_map_get (&map->devices, *id);
    remove_locked (map, *olddev);
  }
  pthread_rwlock_unlock (&map->lock);
}

bool edgex_devmap_removedevice_byid (edgex_devmap_t *map, const char *id)
{
  edgex_device **olddev;
  pthread_rwlock_wrlock (&map->lock);
  olddev = edgex_map_get (&map->devices, id);
  if (olddev)
  {
    remove_locked (map, *olddev);
  }
  pthread_rwlock_unlock (&map->lock);
  return olddev;
}

void edgex_devmap_add_profile (edgex_devmap_t *map, edgex_deviceprofile *dp)
{
  pthread_rwlock_wrlock (&map->lock);
  edgex_map_set (&map->profiles, dp->name, dp);
  pthread_rwlock_unlock (&map->lock);
}

void edgex_devmap_update_profile (devsdk_service_t *svc, edgex_deviceprofile *dp)
{
  pthread_rwlock_wrlock (&svc->devices->lock);
  edgex_deviceprofile **oldp = edgex_map_get (&svc->devices->profiles, dp->name);
  if (oldp)
  {
    edgex_deviceprofile *old = *oldp;
    const char *key;
    edgex_map_iter iter = edgex_map_iter (svc->devices->devices);
    while ((key = edgex_map_next (&svc->devices->devices, &iter)))
    {
      edgex_device **dev = edgex_map_get (&svc->devices->devices, key);
      if ((*dev)->profile == old)
      {
        edgex_device_autoevent_stop_now (*dev);
        (*dev)->profile = dp;
        edgex_device_autoevent_start (svc, *dev);
      }
    }

    edgex_map_remove (&svc->devices->profiles, dp->name);
    edgex_deviceprofile_free (old);
  }
  edgex_map_set (&svc->devices->profiles, dp->name, dp);
  pthread_rwlock_unlock (&svc->devices->lock);
}

edgex_cmdqueue_t *edgex_devmap_device_forcmd
  (edgex_devmap_t *map, const char *cmd, bool forGet)
{
  edgex_device *dev;
  const char *key;
  const struct edgex_cmdinfo *command;
  edgex_cmdqueue_t *result = NULL;
  edgex_cmdqueue_t *q;

  pthread_rwlock_rdlock (&map->lock);
  edgex_map_iter iter = edgex_map_iter (map->devices);
  while ((key = edgex_map_next (&map->devices, &iter)))
  {
    dev = *edgex_map_get (&map->devices, key);
    if (dev->operatingState == ENABLED && dev->adminState == UNLOCKED)
    {
      command = edgex_deviceprofile_findcommand (cmd, dev->profile, forGet);
      if (command)
      {
        q = malloc (sizeof (edgex_cmdqueue_t));
        q->dev = dev;
        q->cmd = command;
        q->next = result;
        result = q;
        atomic_fetch_add (&dev->refs, 1);
      }
    }
  }
  pthread_rwlock_unlock (&map->lock);
  return result;
}

void edgex_device_release (edgex_device *dev)
{
  if (atomic_fetch_add (&dev->refs, -1) == 1)
  {
    edgex_device_autoevent_stop (dev);
    if (!dev->ownprofile)
    {
      dev->profile = NULL;
    }
    edgex_device_free (dev);
  }
}
