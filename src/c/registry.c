/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "edgex/registry.h"
#include "consul.h"
#include "map.h"

typedef edgex_map(edgex_registry_impl) edgex_map_registry;

typedef struct edgex_registry
{
  void *location;
  struct iot_logging_client *logger;
  edgex_registry_impl impl;
} edgex_registry;

static pthread_mutex_t reglock = PTHREAD_MUTEX_INITIALIZER;
static edgex_map_registry *regmap = NULL;

static void reginit (void)
{
  pthread_mutex_lock (&reglock);
  if (regmap == NULL)
  {
    edgex_registry_impl consulimpl;
    consulimpl.ping = edgex_consul_client_ping;
    consulimpl.get_config = edgex_consul_client_get_config;
    consulimpl.put_config = edgex_consul_client_write_config;
    consulimpl.register_service = edgex_consul_client_register_service;
    consulimpl.query_service = edgex_consul_client_query_service;
    consulimpl.parser = edgex_registry_parse_simple_url;
    consulimpl.free_location = edgex_registry_free_simple_url;
    regmap = malloc (sizeof (edgex_map_registry));
    edgex_map_init (regmap);
    edgex_map_set (regmap, "consul", consulimpl);
  }
  pthread_mutex_unlock (&reglock);
}

edgex_registry *edgex_registry_get_registry
  (struct iot_logging_client *lc, const char *url)
{
  edgex_registry *res = NULL;
  edgex_registry_impl *impl;
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
        res = malloc (sizeof (edgex_registry));
        res->location = loc;
        res->impl = *impl;
        res->logger = lc;
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

bool edgex_registry_add_impl (const char *name, edgex_registry_impl impl)
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

void edgex_registry_free (edgex_registry *reg)
{
  if (reg)
  {
    reg->impl.free_location (reg->location);
    free (reg);
  }
}

void edgex_registry_fini ()
{
  reginit ();
  pthread_mutex_lock (&reglock);
  edgex_map_deinit (regmap);
  free (regmap);
  regmap = NULL;
  pthread_mutex_unlock (&reglock);
}

bool edgex_registry_ping (edgex_registry *registry, edgex_error *err)
{
  return registry->impl.ping (registry->logger, registry->location, err);
}

edgex_nvpairs *edgex_registry_get_config
(
  edgex_registry *registry,
  const char *servicename,
  const char *profile,
  edgex_error *err
)
{
  return registry->impl.get_config
    (registry->logger, registry->location, servicename, profile, err);
}

void edgex_registry_put_config
(
  edgex_registry *registry,
  const char *servicename,
  const char *profile,
  const edgex_nvpairs *config,
  edgex_error *err
)
{
  registry->impl.put_config
    (registry->logger, registry->location, servicename, profile, config, err);
}

void edgex_registry_register_service
(
  edgex_registry *registry,
  const char *servicename,
  const char *hostname,
  uint16_t port,
  const char *checkInterval,
  edgex_error *err
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

void edgex_registry_query_service
(
  edgex_registry *registry,
  const char *servicename,
  char **hostname,
  uint16_t *port,
  edgex_error *err
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

void *edgex_registry_parse_simple_url
(
  iot_logging_client *lc,
  const char *location
)
{
  edgex_registry_hostport *res = NULL;
  char *colon = strchr (location, ':');
  if (colon && strlen (colon + 1))
  {
    char *end;
    uint16_t port = strtoul (colon + 1, &end, 10);
    if (*end == '\0')
    {
      res = malloc (sizeof (edgex_registry_hostport));
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

void edgex_registry_free_simple_url
(
  void *location
)
{
  edgex_registry_hostport *endpoint = (edgex_registry_hostport *)location;
  free (endpoint->host);
  free (endpoint);
}

