/* Pseudo-device service emulating counters using C SDK */

/*
 * Copyright (c) 2018-2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <unistd.h>
#include <signal.h>

#include "edgex/devsdk.h"

#define NCOUNTERS 256

#define ERR_CHECK(x) if (x.code) { fprintf (stderr, "Error: %d: %s\n", x.code, x.reason); edgex_device_service_free (service); free (impl); return x.code; }

typedef struct counter_driver
{
  iot_logger_t * lc;
  atomic_uint_fast32_t counters[NCOUNTERS];
} counter_driver;

static bool counter_init
  (void *impl, struct iot_logger_t *lc, const edgex_nvpairs *config)
{
  counter_driver *driver = (counter_driver *) impl;
  driver->lc = lc;
  for (unsigned i = 0; i < NCOUNTERS; i++)
  {
    atomic_store (&driver->counters[i], 0);
  }
  return true;
}

static bool getDeviceAddress
  (iot_logger_t *lc, unsigned long *index, const edgex_protocols *protocols)
{
  const edgex_nvpairs *addr = edgex_protocols_properties (protocols, "Counter");
  if (addr == NULL)
  {
    iot_log_error (lc, "No Counter protocol in device address");
    return false;
  }

  unsigned long i = 0;
  if (!edgex_nvpairs_ulong_value (addr, "Index", &i))
  {
    iot_log_error (lc, "No Index property in Counter protocol");
    return false;
  }
  if (i >= NCOUNTERS)
  {
    iot_log_error (lc, "Index %ul out of range", i);
    return false;
  }

  *index = i;
  return true;
}

static bool counter_get_handler
(
  void *impl,
  const char *devname,
  const edgex_protocols *protocols,
  uint32_t nreadings,
  const edgex_device_commandrequest *requests,
  edgex_device_commandresult *readings,
  const edgex_nvpairs *qparams
)
{
  counter_driver *driver = (counter_driver *)impl;
  unsigned long index;

  if (!getDeviceAddress (driver->lc, &index, protocols))
  {
    return false;
  }

  for (uint32_t i = 0; i < nreadings; i++)
  {
    const char *reg = edgex_nvpairs_value (requests[i].attributes, "register");
    if (reg == NULL)
    {
      iot_log_error (driver->lc, "No register attribute in GET request");
      return false;
    }
    if (strcmp (reg, "count01") == 0)
    {
      readings[i].type = Uint32;
      readings[i].value.ui32_result = atomic_fetch_add (&driver->counters[index], 1);
    }
    /* else ifs for other registers... */
    else
    {
      iot_log_error (driver->lc, "Request for nonexistent register %s", reg);
      return false;
    }
  }
  return true;
}

static bool counter_put_handler
(
  void *impl,
  const char *devname,
  const edgex_protocols *protocols,
  uint32_t nvalues,
  const edgex_device_commandrequest *requests,
  const edgex_device_commandresult *values
)
{
  counter_driver *driver = (counter_driver *)impl;
  unsigned long index;

  if (!getDeviceAddress (driver->lc, &index, protocols))
  {
    return false;
  }

  for (uint32_t i = 0; i < nvalues; i++)
  {
    const char *reg = edgex_nvpairs_value (requests[i].attributes, "register");
    if (reg == NULL)
    {
      iot_log_error (driver->lc, "No register attribute in PUT request");
      return false;
    }
    if (strcmp (reg, "count01") == 0)
    {
      atomic_store (&driver->counters[index], values[i].value.ui32_result);
    }
    else
    {
      iot_log_error (driver->lc, "Request for nonexistent register %s", reg);
      return false;
    }
  }
  return true;
}

/* ---- Disconnect ---- */
/* Disconnect handles protocol-specific cleanup when a device is removed. */
static bool counter_disconnect (void *impl, edgex_protocols *device)
{
  return true;
}

/* ---- Stop ---- */
/* Stop performs any final actions before the device service is terminated */
static void counter_stop (void *impl, bool force) {}


int main (int argc, char *argv[])
{
  edgex_device_svcparams params = { "device-counter", NULL, NULL, NULL };
  sigset_t set;
  int sigret;

  counter_driver * impl = malloc (sizeof (counter_driver));
  impl->lc = NULL;

  if (!edgex_device_service_processparams (&argc, argv, &params))
  {
    return  0;
  }

  int n = 1;
  while (n < argc)
  {
    if (strcmp (argv[n], "-h") == 0 || strcmp (argv[n], "--help") == 0)
    {
      printf ("Options:\n");
      printf ("  -h, --help\t\t: Show this text\n");
      edgex_device_service_usage ();
      return 0;
    }
    else
    {
      printf ("%s: Unrecognized option %s\n", argv[0], argv[n]);
      return 0;
    }
  }

  edgex_error e;
  e.code = 0;

  edgex_device_callbacks counterImpls =
  {
    counter_init,         /* Initialize */
    NULL,                 /* Discovery */
    counter_get_handler,  /* Get */
    counter_put_handler,  /* Put */
    counter_disconnect,   /* Disconnect */
    counter_stop          /* Stop */
  };

  edgex_device_service *service = edgex_device_service_new
    (params.svcname, "1.0", impl, counterImpls, &e);
  ERR_CHECK (e);

  edgex_device_service_start (service, params.regURL, params.profile, params.confdir, &e);
  ERR_CHECK (e);

  sigemptyset (&set);
  sigaddset (&set, SIGINT);
  sigwait (&set, &sigret);

  edgex_device_service_stop (service, true, &e);
  ERR_CHECK (e);

  edgex_device_service_free (service);
  free (impl);
  return 0;
}
