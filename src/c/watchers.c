/*
 * Copyright (c) 2019-2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "watchers.h"
#include "edgex-rest.h"
#include "service.h"
#include "errorlist.h"
#include "metadata.h"
#include "filesys.h"
#include <regex.h>

typedef struct edgex_watchlist_t
{
  pthread_rwlock_t lock;
  edgex_watcher *list;
} edgex_watchlist_t;

typedef struct edgex_watcher_regexes_t
{
  const char *name;
  regex_t preg;
  edgex_watcher_regexes_t *next;
} edgex_watcher_regexes_t;

edgex_watchlist_t *edgex_watchlist_alloc ()
{
  edgex_watchlist_t *res = calloc (1, sizeof (edgex_watchlist_t));
  pthread_rwlock_init (&res->lock, NULL);
  return res;
}

void edgex_watchlist_free (edgex_watchlist_t *wl)
{
  if (wl)
  {
    pthread_rwlock_destroy (&wl->lock);
    edgex_watcher_free (wl->list);
    free (wl);
  }
}

void edgex_watcher_regexes_free (edgex_watcher_regexes_t *regs)
{
  if (regs)
  {
    edgex_watcher_regexes_free (regs->next);
    regfree (&regs->preg);
    free (regs);
  }
}

static edgex_watcher **find_locked (edgex_watcher **list, const char *name)
{
  while (*list && strcmp ((*list)->name, name))
  {
    list = &((*list)->next);
  }
  return list;
}

static void add_locked (edgex_watchlist_t *wl, const edgex_watcher *w)
{
  if ((!w) || (!w->identifiers) || (iot_data_type(w->identifiers) != IOT_DATA_MAP))
  {
    return;
  }
  edgex_watcher *newelem = edgex_watcher_dup (w);
  iot_data_map_iter_t iter;
  iot_data_map_iter (newelem->identifiers, &iter);
  while (iot_data_map_iter_next (&iter))
  {
    edgex_watcher_regexes_t *r = malloc (sizeof (edgex_watcher_regexes_t));
    if (regcomp (&r->preg, iot_data_map_iter_string_value (&iter), REG_NOSUB) == 0)
    {
      r->name = iot_data_map_iter_string_key (&iter);
      r->next = newelem->regs;
      newelem->regs = r;
    }
    else
    {
      free (r);
    }
  }
  newelem->next = wl->list;
  wl->list = newelem;
}

bool edgex_watchlist_remove_watcher (edgex_watchlist_t *wl, const char *name)
{
  bool result = false;
  pthread_rwlock_wrlock (&wl->lock);

  edgex_watcher **ptr = find_locked (&wl->list, name);
  edgex_watcher *found = *ptr;
  if (found)
  {
    *ptr = found->next;
    found->next = NULL;
    edgex_watcher_free (found);
    result = true;
  }

  pthread_rwlock_unlock (&wl->lock);
  return result;
}

void edgex_watchlist_update_watcher (edgex_watchlist_t *wl, const edgex_watcher *updated)
{
  pthread_rwlock_wrlock (&wl->lock);

  edgex_watcher **ptr = find_locked (&wl->list, updated->name);
  edgex_watcher *found = *ptr;
  if (found)
  {
    *ptr = edgex_watcher_dup (updated);
    (*ptr)->next = found->next;
    found->next = NULL;
    edgex_watcher_free (found);
  }
  else
  {
    add_locked (wl, updated);
  }

  pthread_rwlock_unlock (&wl->lock);
}

unsigned edgex_watchlist_populate (edgex_watchlist_t *wl, const edgex_watcher *newlist)
{
  unsigned count = 0;
  pthread_rwlock_wrlock (&wl->lock);

  for (const edgex_watcher *w = newlist; w; w = w->next)
  {
    if (*find_locked (&wl->list, w->name) == NULL)
    {
      count++;
      add_locked (wl, w);
    }
  }

  pthread_rwlock_unlock (&wl->lock);
  return count;
}

static bool matchpw (const edgex_watcher *pw, const iot_data_t *ids)
{
  edgex_watcher_regexes_t *match = NULL;

  for (match = pw->regs; match; match = match->next)
  {
    const char *matchval = iot_data_string_map_get_string (ids, match->name);
    if ((matchval == NULL) || (regexec (&match->preg, matchval, 0, NULL, 0) != 0))
    {
      return false;
    }
  }


  if ((pw->blocking_identifiers) && (iot_data_type(pw->blocking_identifiers) == IOT_DATA_MAP))
  {
    iot_data_map_iter_t blocked;
    iot_data_map_iter (pw->blocking_identifiers, &blocked);
    while (iot_data_map_iter_next (&blocked))
    {
      const iot_data_t *checkval = iot_data_map_get (ids, iot_data_map_iter_key (&blocked));
      if (checkval)
      {
	if (iot_data_vector_find (iot_data_map_iter_value (&blocked), (iot_data_cmp_fn)iot_data_equal, checkval))
	{
	  return false;
	}
      }
    }
  }

  return true;
}

static bool edgex_watcher_exists (const edgex_watchlist_t *wl, const char *name)
{
  bool exists = false;
  pthread_rwlock_rdlock ((pthread_rwlock_t *)&wl->lock);
  
  for (const edgex_watcher *w = wl->list; w; w = w->next)
  {
    if (strcmp (w->name, name) == 0)
    {
      exists = true;
      break;
    }
  }
  
  pthread_rwlock_unlock ((pthread_rwlock_t *)&wl->lock);
  return exists;
}

edgex_watcher *edgex_watchlist_match (const edgex_watchlist_t *wl, const iot_data_t *ids)
{
  pthread_rwlock_rdlock ((pthread_rwlock_t *)&wl->lock);

  edgex_watcher *result = NULL;
  const edgex_watcher *w = wl->list;

  while (w && !result)
  {
    if (matchpw (w, ids))
    {
      result = edgex_watcher_dup (w);
      break;
    }
    w = w->next;
  }

  pthread_rwlock_unlock ((pthread_rwlock_t *)&wl->lock);
  return result;
}

static void edgex_add_watcher_json (devsdk_service_t *svc, const char *fname, devsdk_error *err)
{

  JSON_Value *jval = json_parse_file (fname);
  if (jval)
  {
    JSON_Object *jobj = json_value_get_object (jval);
    const char *name = json_object_get_string (jobj, "name");
    if (name)
    {
      iot_log_debug(svc->logger, "Checking existence of ProvisionWatcher %s", name);
      if (edgex_watcher_exists (svc->watchlist, name))
      {
        iot_log_info(svc->logger, "ProvisionWatcher %s already exists: skipped", name);
      } 
      else 
      {
        JSON_Value *copy = json_value_deep_copy(jval);
        JSON_Object *watchobj = json_value_get_object(copy);
        edgex_metadata_client_add_watcher_jobj(svc->logger, &svc->config.endpoints, svc->secretstore, svc->name, watchobj, err);
      }
    }
    else
    {
      iot_log_warn (svc->logger, "Provision watcher upload: Missing provisionwatcher name in %s", fname);
    }
    
    json_value_free (jval);
  }
  else
  {
    iot_log_error (svc->logger, "File %s does not parse as JSON", fname);
    *err = EDGEX_CONF_PARSE_ERROR;
  }
}

void edgex_device_watchers_upload (devsdk_service_t *svc, devsdk_error *err)
{
  *err = EDGEX_OK;
  
  if (!svc->config.device.provisionwatchersdir || !strlen(svc->config.device.provisionwatchersdir))
  {
    return;  // No directory configured
  }
  
  iot_log_info (svc->logger, "Processing Provision Watchers from %s", svc->config.device.provisionwatchersdir);

  devsdk_strings *filenames = devsdk_scandir (svc->logger, svc->config.device.provisionwatchersdir, "json");
  
  for (devsdk_strings *f = filenames; f; f = f->next)
  {
    edgex_add_watcher_json (svc, f->str, err);
  }
  
  devsdk_strings_free (filenames);
}