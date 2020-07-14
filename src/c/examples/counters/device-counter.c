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

static bool exceptionAndError0 (iot_data_t **exception, iot_logger_t *lc, const char *msg)
{
  iot_log_error (lc, msg);
  *exception = iot_data_alloc_string (msg, IOT_DATA_REF);
  return false;
}

static bool exceptionAndErrorN (iot_data_t **exception, iot_logger_t *lc, const char *msg, ...)
{
  char *buf = malloc (ERR_BUFSZ);
  va_list args;
  va_start (args, msg);
  vsnprintf (buf, ERR_BUFSZ, msg, args);
  va_end (args);

  iot_log_error (lc, buf);
  *exception = iot_data_alloc_string (buf, IOT_DATA_TAKE);

  return false;
}

static bool getDeviceAddress
  (iot_logger_t *lc, unsigned long *index, const devsdk_protocols *protocols, iot_data_t **exception)
{
  const devsdk_nvpairs *addr = devsdk_protocols_properties (protocols, "Counter");
  if (addr == NULL)
  {
    return exceptionAndError0 (exception, lc, "No Counter protocol in device address");
  }

  unsigned long i = 0;
  if (!devsdk_nvpairs_ulong_value (addr, "Index", &i))
  {
    return exceptionAndError0 (exception, lc, "No Index property in Counter protocol");
  }
  if (i >= NCOUNTERS)
  {
    return exceptionAndErrorN (exception, lc, "Index %ul out of range", i);
  }

  *index = i;
  return true;
}

static bool counter_get_handler
(
 void *impl,
 const char *devname,
 const devsdk_protocols *protocols,
 uint32_t nreadings,
 const devsdk_commandrequest *requests,
 devsdk_commandresult *readings,
 const devsdk_nvpairs *qparms,
 iot_data_t **exception
)
{
  counter_driver *driver = (counter_driver *)impl;
  unsigned long index;

  if (!getDeviceAddress (driver->lc, &index, protocols, exception))
  {
    return false;
  }

  for (uint32_t i = 0; i < nreadings; i++)
  {
    const char *reg = devsdk_nvpairs_value (requests[i].attributes, "register");
    if (reg == NULL)
    {
      return exceptionAndError0 (exception, driver->lc, "No register attribute in GET request");
    }
    if (strcmp (reg, "count01") == 0)
    {
      readings[i].value = iot_data_alloc_ui32 (atomic_fetch_add (&driver->counters[index], 1));
    }
    /* else ifs for other registers... */
    else
    {
      return exceptionAndErrorN (exception, driver->lc, "Request for nonexistent register %s", reg);
    }
  }
  return true;
}

static bool counter_put_handler
(
  void *impl,
  const char *devname,
  const devsdk_protocols *protocols,
  uint32_t nvalues,
  const devsdk_commandrequest *requests,
  const iot_data_t *values[],
  iot_data_t **exception
)
{
  counter_driver *driver = (counter_driver *)impl;
  unsigned long index;

  if (!getDeviceAddress (driver->lc, &index, protocols, exception))
  {
    return false;
  }

  for (uint32_t i = 0; i < nvalues; i++)
  {
    const char *reg = devsdk_nvpairs_value (requests[i].attributes, "register");
    if (reg == NULL)
    {
      return exceptionAndError0 (exception, driver->lc, "No register attribute in PUT request");
    }
    if (strcmp (reg, "count01") == 0)
    {
      atomic_store (&driver->counters[index], iot_data_ui32 (values[i]));
    }
    else
    {
      return exceptionAndErrorN (exception, driver->lc, "Request for nonexistent register %s", reg);
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

  devsdk_callbacks counterImpls =
  {
    counter_init,         /* Initialize */
    NULL,                 /* Reconfigure */
    NULL,                 /* Discovery */
    counter_get_handler,  /* Get */
    counter_put_handler,  /* Put */
    counter_stop          /* Stop */
  };

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
  return 0;
}
