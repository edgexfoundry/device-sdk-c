/* Pseudo-device service emulating counters using C SDK */

/*
 * Copyright (c) 2018-2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <unistd.h>
#include <signal.h>
#include <stdarg.h>

#include "devsdk/devsdk.h"

#define NCOUNTERS 256
#define ERR_BUFSZ 1024

#define ERR_CHECK(x) if (x.code) { fprintf (stderr, "Error: %d: %s\n", x.code, x.reason); devsdk_service_free (service); free (impl); return x.code; }

typedef enum { COUNTER_R0 } counter_register;

typedef struct counter_driver
{
  iot_logger_t * lc;
  atomic_uint_fast32_t counters[NCOUNTERS];
} counter_driver;

static bool counter_init
  (void *impl, struct iot_logger_t *lc, const iot_data_t *config)
{
  counter_driver *driver = (counter_driver *) impl;
  driver->lc = lc;
  for (unsigned i = 0; i < NCOUNTERS; i++)
  {
    atomic_store (&driver->counters[i], 0);
  }
  return true;
}

static devsdk_address_t counter_create_addr (void *impl, const devsdk_protocols *protocols, iot_data_t **exception)
{
  const iot_data_t *props = devsdk_protocols_properties (protocols, "Counter");
  if (props == NULL)
  {
    *exception = iot_data_alloc_string ("No Counter protocol in device address", IOT_DATA_REF);
    return NULL;
  }
  const char *index = iot_data_string_map_get_string (props, "Index");
  if (index == NULL)
  {
    *exception = iot_data_alloc_string ("Index in device address missing", IOT_DATA_REF);
    return NULL;
  }
  char *end = NULL;
  errno = 0;
  unsigned long val = strtoul (index, &end, 0);
  if (errno == 0 && *end == 0 && val >= 0 && val < NCOUNTERS)
  {
    uint64_t *result = malloc (sizeof (uint64_t));
    *result = val;
    return result;
  }
  else
  {
    *exception = iot_data_alloc_string ("Index in device address out of range", IOT_DATA_REF);
    return NULL;
  }
}

static void counter_free_addr (void *impl, devsdk_address_t address)
{
  free (address);
}

static devsdk_resource_attr_t counter_create_resource_attr (void *impl, const iot_data_t *attributes, iot_data_t **exception)
{
  counter_register *result = NULL;
  const char *reg = iot_data_string_map_get_string (attributes, "register");

  if (reg)
  {
    if (strcmp (reg, "count01") == 0)
    {
      result = malloc (sizeof (counter_register));
      *result = COUNTER_R0;
    }
    /* else ifs for other registers... */
    else
    {
      *exception = iot_data_alloc_string ("device resource specifies nonexistent register", IOT_DATA_REF);
    }
  }
  else
  {
    *exception = iot_data_alloc_string ("No register attribute in device resource", IOT_DATA_REF);
  }
  return result;
}

static void counter_free_resource_attr (void *impl, devsdk_resource_attr_t resource)
{
  free (resource);
}

static bool counter_get_handler
(
 void *impl,
 const devsdk_device_t *device,
 uint32_t nreadings,
 const devsdk_commandrequest *requests,
 devsdk_commandresult *readings,
 const iot_data_t *options,
 iot_data_t **exception
)
{
  counter_driver *driver = (counter_driver *)impl;
  uint64_t index = *(uint64_t *)device->address;

  for (uint32_t i = 0; i < nreadings; i++)
  {
    switch (*(counter_register *)requests[i].resource->attrs)
    {
      case COUNTER_R0:
        readings[i].value = iot_data_alloc_ui32 (atomic_fetch_add (&driver->counters[index], 1));
        break;
    }
  }
  return true;
}

static bool counter_put_handler
(
  void *impl,
  const devsdk_device_t *device,
  uint32_t nvalues,
  const devsdk_commandrequest *requests,
  const iot_data_t *values[],
  const iot_data_t *options,
  iot_data_t **exception
)
{
  counter_driver *driver = (counter_driver *)impl;
  uint64_t index = *(uint64_t *)device->address;

  for (uint32_t i = 0; i < nvalues; i++)
  {
    switch (*(counter_register *)requests[i].resource->attrs)
    {
      case COUNTER_R0:
        atomic_store (&driver->counters[index], iot_data_ui32 (values[i]));
        break;
    }
  }
  return true;
}

/* ---- Stop ---- */
/* Stop performs any final actions before the device service is terminated */
static void counter_stop (void *impl, bool force) {}

int main (int argc, char *argv[])
{
  sigset_t set;
  int sigret;

  counter_driver * impl = malloc (sizeof (counter_driver));
  impl->lc = NULL;

  devsdk_error e;
  e.code = 0;

  devsdk_callbacks *counterImpls = devsdk_callbacks_init
  (
    counter_init,
    counter_get_handler,
    counter_put_handler,
    counter_stop,
    counter_create_addr,
    counter_free_addr,
    counter_create_resource_attr,
    counter_free_resource_attr
  );

  devsdk_service_t *service = devsdk_service_new
    ("device-counter", "1.0", impl, counterImpls, &argc, argv, &e);
  ERR_CHECK (e);

  int n = 1;
  while (n < argc)
  {
    if (strcmp (argv[n], "-h") == 0 || strcmp (argv[n], "--help") == 0)
    {
      printf ("Options:\n");
      printf ("  -h, --help\t\t\tShow this text\n");
      return 0;
    }
    else
    {
      printf ("%s: Unrecognized option %s\n", argv[0], argv[n]);
      return 0;
    }
  }

  devsdk_service_start (service, NULL, &e);
  ERR_CHECK (e);

  sigemptyset (&set);
  sigaddset (&set, SIGINT);
  sigprocmask (SIG_BLOCK, &set, NULL);
  sigwait (&set, &sigret);
  sigprocmask (SIG_UNBLOCK, &set, NULL);

  devsdk_service_stop (service, true, &e);
  ERR_CHECK (e);

  devsdk_service_free (service);
  free (impl);
  free (counterImpls);
  return 0;
}
