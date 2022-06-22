/*
 * Copyright (c) 2019-2022
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "registry-impl.h"
#include "consul.h"
#include "errorlist.h"
#include "iot/time.h"

typedef struct devsdk_registry_t
{
  void *state;
  devsdk_registry_impls fns;
} devsdk_registry_t;

devsdk_registry_t *devsdk_registry_get_consul ()
{
  devsdk_registry_t *result = malloc (sizeof (devsdk_registry_t));
  result->state = devsdk_registry_consul_alloc ();
  result->fns = devsdk_registry_consul_fns;
  return result;
}

bool devsdk_registry_init (devsdk_registry_t *reg, iot_logger_t *lc, iot_threadpool_t *thpool, const char *url)
{
  return reg->fns.init (reg->state, lc, thpool, url);
}

void devsdk_registry_free (devsdk_registry_t *reg)
{
  if (reg)
  {
    reg->fns.free (reg->state);
    free (reg);
  }
}

bool devsdk_registry_waitfor (devsdk_registry_t *registry, const devsdk_timeout *timeout)
{
  while (true)
  {
    uint64_t t1, t2;

    t1 = iot_time_msecs ();
    if (registry->fns.ping (registry->state))
    {
      return true;
    }
    t2 = iot_time_msecs ();
    if (t2 > timeout->deadline - timeout->interval)
    {
      return false;
    }
    if (timeout->interval > t2 - t1)
    {
      iot_wait_msecs (timeout->interval - (t2 - t1));
    }
  }
}

devsdk_nvpairs *devsdk_registry_get_config
(
  devsdk_registry_t *registry,
  const char *servicename,
  devsdk_registry_updatefn updater,
  void *updatectx,
  atomic_bool *updatedone,
  devsdk_error *err
)
{
  return registry->fns.get_config (registry->state, servicename, updater, updatectx, updatedone, err);
}

void devsdk_registry_put_config (devsdk_registry_t *registry, const char *servicename, const iot_data_t *config, devsdk_error *err)
{
  registry->fns.put_config (registry->state, servicename, config, err);
}

void devsdk_registry_register_service
(
  devsdk_registry_t *registry,
  const char *servicename,
  const char *hostname,
  uint16_t port,
  const char *checkInterval,
  devsdk_error *err
)
{
  registry->fns.register_service (registry->state, servicename, hostname, port, checkInterval, err);
}

void devsdk_registry_deregister_service (devsdk_registry_t *registry, const char *servicename, devsdk_error *err)
{
  registry->fns.deregister_service (registry->state, servicename, err);
}

void devsdk_registry_query_service
(
  devsdk_registry_t *registry,
  const char *servicename,
  char **hostname,
  uint16_t *port,
  const devsdk_timeout *timeout,
  devsdk_error *err
)
{
  uint64_t t1, t2;
  while (true)
  {
    t1 = iot_time_msecs ();
    *err = EDGEX_OK;
    registry->fns.query_service (registry->state, servicename, hostname, port, err);
    if (err->code == 0)
    {
      break;
    }
    t2 = iot_time_msecs ();
    if (t2 > timeout->deadline - timeout->interval)
    {
      *err = EDGEX_REMOTE_SERVER_DOWN;
      break;
    }
    if (timeout->interval > t2 - t1)
    {
      iot_wait_msecs (timeout->interval - (t2 - t1));
    }
  }
}
