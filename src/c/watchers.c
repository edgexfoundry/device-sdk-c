/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "watchers.h"
#include "edgex-rest.h"

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

static edgex_watcher **find_locked (edgex_watcher **list, const char *id)
{
  while (*list && strcmp ((*list)->id, id))
  {
    list = &((*list)->next);
  }
  return list;
}

static void add_locked (edgex_watchlist_t *wl, const edgex_watcher *w)
{
  edgex_watcher *newelem = edgex_watcher_dup (w);
  for (edgex_nvpairs *ids = newelem->identifiers; ids; ids = ids->next)
  {
    edgex_watcher_regexes_t *r = malloc (sizeof (edgex_watcher_regexes_t));
    if (regcomp (&r->preg, ids->value, REG_NOSUB) == 0)
    {
      r->name = ids->name;
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

bool edgex_watchlist_remove_watcher (edgex_watchlist_t *wl, const char *id)
{
  bool result = false;
  pthread_rwlock_wrlock (&wl->lock);

  edgex_watcher **ptr = find_locked (&wl->list, id);
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

  edgex_watcher **ptr = find_locked (&wl->list, updated->id);
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
    if (*find_locked (&wl->list, w->id) == NULL)
    {
      count++;
      add_locked (wl, w);
    }
  }

  pthread_rwlock_unlock (&wl->lock);
  return count;
}

static bool matchpw (const edgex_watcher *pw, const devsdk_nvpairs *ids)
{
  const edgex_blocklist *blocked = NULL;
  edgex_watcher_regexes_t *match = NULL;

  for (match = pw->regs; match; match = match->next)
  {
    const char *matchval = devsdk_nvpairs_value (ids, match->name);
    if (matchval && regexec (&match->preg, matchval, 0, NULL, 0) == 0)
    {
      break;
    }
  }
  if (match == NULL)
  {
    return false;
  }

  for (blocked = (const edgex_blocklist *)pw->blocking_identifiers; blocked; blocked = blocked->next)
  {
    const char *checkval = devsdk_nvpairs_value (ids, blocked->name);
    if (checkval)
    {
      for (edgex_strings *bv = blocked->values; bv; bv = bv->next)
      {
        if (strcmp (bv->str, checkval) == 0)
        {
          return false;
        }
      }
    }
  }
  return true;
}

edgex_watcher *edgex_watchlist_match (const edgex_watchlist_t *wl, const devsdk_nvpairs *ids)
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

void edgex_watchlist_dump (const edgex_watchlist_t *wl, iot_logger_t *logger)
{
  pthread_rwlock_rdlock ((pthread_rwlock_t *)&wl->lock);

  for (const edgex_watcher *w = wl->list; w; w = w->next)
  {
    iot_log_debug (logger, "PW: Id=%s Name=%s Profile=%s", w->id, w->name, w->profile);
    for (const devsdk_nvpairs *match = (const devsdk_nvpairs *)w->identifiers; match; match = match->next)
    {
      iot_log_debug (logger, "PW: Match %s = %s", match->name, match->value);
    }
    for (const edgex_blocklist *bl = w->blocking_identifiers; bl; bl = bl->next)
    {
      for (edgex_strings *s = bl->values; s; s = s->next)
      {
        iot_log_debug (logger, "PW: Block %s = %s", bl->name, s->str);
      }
    }
  }

  pthread_rwlock_unlock ((pthread_rwlock_t *)&wl->lock);
}
