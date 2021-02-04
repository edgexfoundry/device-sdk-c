/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "registry.h"
#include "consul.h"
#include "map.h"
#include "errorlist.h"

typedef edgex_map(devsdk_registry_impl) devsdk_map_registry;

typedef struct devsdk_registry
{
  void *location;
  iot_logger_t *logger;
  iot_threadpool_t *thpool;
  devsdk_registry_impl impl;
} devsdk_registry;

static pthread_mutex_t reglock = PTHREAD_MUTEX_INITIALIZER;
static devsdk_map_registry *regmap = NULL;

static void reginit (void)
{
  pthread_mutex_lock (&reglock);
  if (regmap == NULL)
  {
    devsdk_registry_impl consulimpl;
    consulimpl.ping = edgex_consul_client_ping;
    consulimpl.get_config = edgex_consul_client_get_config;
    consulimpl.put_config = edgex_consul_client_write_config;
    consulimpl.register_service = edgex_consul_client_register_service;
    consulimpl.deregister_service = edgex_consul_client_deregister_service;
    consulimpl.query_service = edgex_consul_client_query_service;
    consulimpl.parser = devsdk_registry_parse_simple_url;
    consulimpl.free_location = devsdk_registry_free_simple_url;
    regmap = malloc (sizeof (devsdk_map_registry));
    edgex_map_init (regmap);
    edgex_map_set (regmap, "consul", consulimpl);
    edgex_map_set (regmap, "consul.http", consulimpl);
  }
  pthread_mutex_unlock (&reglock);
}

devsdk_registry *devsdk_registry_get_registry
  (iot_logger_t *lc, iot_threadpool_t *tp, const char *url)
{
  devsdk_registry *res = NULL;
  devsdk_registry_impl *impl;
  char *delim = strstr (url, "://");
  if (delim)
  {
    char *name = strndup (url, delim - url);
    reginit ();
    pthread_mutex_lock (&reglock);
    impl = edgex_map_get (regmap, name);
    pthread_mutex_unlock (&reglock);
    if (impl)
    {
      void *loc = impl->parser (lc, delim + 3);
      if (loc)
      {
        res = malloc (sizeof (devsdk_registry));
        res->location = loc;
        res->impl = *impl;
        res->logger = lc;
        res->thpool = tp;
      }
    }
    else
    {
      iot_log_error (lc, "No registry implementation found for \"%s\"", name);
    }
    free (name);
  }
  else
  {
    iot_log_error (lc, "Unable to parse \"%s\" as url for registry", url);
  }
  return res;
}

bool devsdk_registry_add_impl (const char *name, devsdk_registry_impl impl)
{
  bool res = false;
  reginit ();
  pthread_mutex_lock (&reglock);
  if (edgex_map_get (regmap, name) == NULL)
  {
    res = true;
    edgex_map_set (regmap, name, impl);
  }
  pthread_mutex_unlock (&reglock);
  return res;
}

void devsdk_registry_free (devsdk_registry *reg)
{
  if (reg)
  {
    reg->impl.free_location (reg->location);
    free (reg);
  }
}

void devsdk_registry_fini ()
{
  reginit ();
  pthread_mutex_lock (&reglock);
  edgex_map_deinit (regmap);
  free (regmap);
  regmap = NULL;
  pthread_mutex_unlock (&reglock);
}

bool devsdk_registry_ping (devsdk_registry *registry, devsdk_error *err)
{
  return registry->impl.ping (registry->logger, registry->location, err);
}

bool devsdk_registry_waitfor (devsdk_registry *registry)
{
  devsdk_error err = EDGEX_OK;
  struct timespec delay;
  char *val;
  int retries = 5;
  time_t secs = 1;

  val = getenv ("EDGEX_STARTUP_INTERVAL");
  if (val == NULL)
  {
    val = getenv ("startup_interval");
  }
  if (val == NULL)
  {
    val = getenv ("edgex_registry_retry_wait");
  }
  if (val)
  {
    int rw = atoi (val);
    if (rw > 0)
    {
      secs = rw;
    }
  }

  val = getenv ("EDGEX_STARTUP_DURATION");
  if (val == NULL)
  {
    val = getenv ("startup_duration");
  }
  if (val)
  {
    int dur = atoi (val);
    if (dur > 0)
    {
      retries = dur / secs;
    }
  }
  else
  {
    val = getenv ("edgex_registry_retry_count");
    if (val)
    {
      int rc = atoi (val);
      if (rc > 0)
      {
        retries = rc;
      }
    }
  }

  delay.tv_sec = secs;
  delay.tv_nsec = 0;

  while (!devsdk_registry_ping (registry, &err) && --retries)
  {
    nanosleep (&delay, NULL);
    err = EDGEX_OK;
  }

  return (retries > 0);
}

devsdk_nvpairs *devsdk_registry_get_config
(
  devsdk_registry *registry,
  const char *servicename,
  devsdk_registry_updatefn updater,
  void *updatectx,
  atomic_bool *updatedone,
  devsdk_error *err
)
{
  return registry->impl.get_config (registry->logger, registry->thpool, registry->location, servicename, updater, updatectx, updatedone, err);
}

void devsdk_registry_put_config
(
  devsdk_registry *registry,
  const char *servicename,
  const iot_data_t *config,
  devsdk_error *err
)
{
  registry->impl.put_config (registry->logger, registry->location, servicename, config, err);
}

void devsdk_registry_register_service
(
  devsdk_registry *registry,
  const char *servicename,
  const char *hostname,
  uint16_t port,
  const char *checkInterval,
  devsdk_error *err
)
{
  registry->impl.register_service
  (
    registry->logger,
    registry->location,
    servicename,
    hostname,
    port,
    checkInterval,
    err
  );
}

void devsdk_registry_deregister_service (devsdk_registry *registry, const char *servicename, devsdk_error *err)
{
  registry->impl.deregister_service (registry->logger, registry->location, servicename, err);
}

void devsdk_registry_query_service
(
  devsdk_registry *registry,
  const char *servicename,
  char **hostname,
  uint16_t *port,
  devsdk_error *err
)
{
  registry->impl.query_service
  (
    registry->logger,
    registry->location,
    servicename,
    hostname,
    port,
    err
  );
}

void *devsdk_registry_parse_simple_url
(
  iot_logger_t *lc,
  const char *location
)
{
  devsdk_registry_hostport *res = NULL;
  char *colon = strchr (location, ':');
  if (colon && strlen (colon + 1))
  {
    char *end;
    uint16_t port = strtoul (colon + 1, &end, 10);
    if (*end == '\0')
    {
      res = malloc (sizeof (devsdk_registry_hostport));
      res->port = port;
      res->host = strndup (location, colon - location);
    }
    else
    {
      iot_log_error
        (lc, "Unable to parse \"%s\" for port number for registry", colon + 1);
    }
  }
  else
  {
    iot_log_error (lc, "No port number found in \"%s\".", location);
  }
  return res;
}

void devsdk_registry_free_simple_url
(
  void *location
)
{
  devsdk_registry_hostport *endpoint = (devsdk_registry_hostport *)location;
  free (endpoint->host);
  free (endpoint);
}

