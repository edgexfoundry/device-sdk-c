/*
 * Copyright (c) 2019-2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_WATCHERS_H_
#define _EDGEX_DEVICE_WATCHERS_H_ 1

/* Manages provision watchers for the SDK. */

#include "edgex/edgex.h"
#include "devsdk/devsdk-base.h"
#include "iot/logger.h"

struct edgex_watchlist_t;
typedef struct edgex_watchlist_t edgex_watchlist_t;

extern edgex_watchlist_t *edgex_watchlist_alloc (void);
extern void edgex_watchlist_free (edgex_watchlist_t *list);

struct edgex_watcher_regexes_t;
typedef struct edgex_watcher_regexes_t edgex_watcher_regexes_t;

extern void edgex_watcher_regexes_free (edgex_watcher_regexes_t *r);

extern unsigned edgex_watchlist_populate (edgex_watchlist_t *list, const edgex_watcher *entry);
extern bool edgex_watchlist_remove_watcher (edgex_watchlist_t *list, const char *id);
extern void edgex_watchlist_update_watcher (edgex_watchlist_t *list, const edgex_watcher *updated);

extern edgex_watcher *edgex_watchlist_match (const edgex_watchlist_t *list, const iot_data_t *ids);
extern void edgex_device_watchers_upload (devsdk_service_t *svc, devsdk_error *err);

#endif
