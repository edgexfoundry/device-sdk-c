/* Pseudo-device service illustrating resource aggregation using C SDK */

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

#define ERR_CHECK(x) if (x.code) { fprintf (stderr, "Error: %d: %s\n", x.code, x.reason); edgex_device_service_free (service); free (impl); return x.code; }

typedef struct gyro_driver
{
  iot_logger_t * lc;
} gyro_driver;

static const char *findinpairs
  (const edgex_nvpairs *nvps, const char *name)
{
  const edgex_nvpairs *pair = nvps;
  while (pair)
  {
    if (strcmp (pair->name, name) == 0)
    {
      return pair->value;
    }
    pair = pair->next;
  }
  return NULL;
}

static bool gyro_init
  (void *impl, struct iot_logger_t *lc, const edgex_nvpairs *config)
{
  gyro_driver *driver = (gyro_driver *) impl;
  driver->lc = lc;
  return true;
}

static bool gyro_get_handler
(
  void *impl,
  const char *devname,
  const edgex_protocols *protocols,
  uint32_t nreadings,
  const edgex_device_commandrequest *requests,
  edgex_device_commandresult *readings
)
{
  gyro_driver *driver = (gyro_driver *)impl;

  for (uint32_t i = 0; i < nreadings; i++)
  {
    const char *param = findinpairs (requests[i].attributes, "parameter");
    if (param == NULL)
    {
      iot_log_error (driver->lc, "No parameter attribute in GET request");
      return false;
    }
    if (strcmp (param, "xrot") == 0)
    {
      readings[i].value.i32_result = (random () % 501) - 250;
      readings[i].type = Int32;
    }
    else if (strcmp (param, "yrot") == 0)
    {
      readings[i].value.i32_result = (random () % 501) - 250;
      readings[i].type = Int32;
    }
    else if (strcmp (param, "zrot") == 0)
    {
      readings[i].value.i32_result = (random () % 501) - 250;
      readings[i].type = Int32;
    }
    else
    {
      iot_log_error (driver->lc, "Unknown parameter %s requested", param);
      return false;
    }
  }

  return true;
}

static bool gyro_put_handler
(
  void *impl,
  const char *devname,
  const edgex_protocols *protocols,
  uint32_t nvalues,
  const edgex_device_commandrequest *requests,
  const edgex_device_commandresult *values
)
{
  gyro_driver *driver = (gyro_driver *)impl;
  iot_log_error (driver->lc, "PUT called for gyro device. This is a read-only device");
  return false;
}


/* ---- Disconnect ---- */
/* Disconnect handles protocol-specific cleanup when a device is removed. */
static bool gyro_disconnect (void *impl, edgex_protocols *device)
{
  return true;
}

/* ---- Stop ---- */
/* Stop performs any final actions before the device service is terminated */
static void gyro_stop (void *impl, bool force)
{
}


int main (int argc, char *argv[])
{
  edgex_device_svcparams params = { "device-gyro", NULL, NULL, NULL };
  sigset_t set;
  int sigret;

  gyro_driver * impl = malloc (sizeof (gyro_driver));
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

  edgex_device_callbacks gyroImpls =
  {
    gyro_init,         /* Initialize */
    NULL,              /* Discovery */
    gyro_get_handler,  /* Get */
    gyro_put_handler,  /* Put */
    gyro_disconnect,   /* Disconnect */
    gyro_stop          /* Stop */
  };

  edgex_device_service *service = edgex_device_service_new
    (params.svcname, "1.0", impl, gyroImpls, &e);
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
