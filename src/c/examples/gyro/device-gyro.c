/* Pseudo-device service illustrating resource aggregation using C SDK */

/*
 * Copyright (c) 2018-2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <unistd.h>
#include <signal.h>

#include "devsdk/devsdk.h"

#define ERR_CHECK(x) if (x.code) { fprintf (stderr, "Error: %d: %s\n", x.code, x.reason); devsdk_service_free (service); free (impl); return x.code; }
#define ERR_BUFSZ 1024
#define ERR_GYRO_WRITE "PUT called for gyro device. This is a read-only device."
#define ERR_GYRO_NO_PARAM "No parameter attribute in GET request."

typedef struct gyro_driver
{
  iot_logger_t * lc;
} gyro_driver;

static bool gyro_init
  (void * impl, struct iot_logger_t * lc, const iot_data_t * config)
{
  gyro_driver * driver = (gyro_driver *) impl;
  driver->lc = lc;
  return true;
}

static bool gyro_get_handler
(
  void * impl,
  const char * devname,
  const devsdk_protocols * protocols,
  uint32_t nreadings,
  const devsdk_commandrequest * requests,
  devsdk_commandresult * readings,
  const devsdk_nvpairs * qparams,
  iot_data_t ** exception
)
{
  gyro_driver * driver = (gyro_driver *) impl;
  char * buff;

  for (uint32_t i = 0; i < nreadings; i++)
  {
    const char * param = devsdk_nvpairs_value (requests[i].attributes, "parameter");
    if (param == NULL)
    {
      iot_log_error (driver->lc, ERR_GYRO_NO_PARAM);
      * exception = iot_data_alloc_string (ERR_GYRO_NO_PARAM, IOT_DATA_REF);
      return false;
    }
    if (strcmp (param, "xrot") == 0)
    {
      readings[i].value = iot_data_alloc_i32 ((random () % 501) - 250);
    }
    else if (strcmp (param, "yrot") == 0)
    {
      readings[i].value = iot_data_alloc_i32 ((random () % 501) - 250);
    }
    else if (strcmp (param, "zrot") == 0)
    {
      readings[i].value = iot_data_alloc_i32 ((random () % 501) - 250);
    }
    else
    {
      buff = malloc (ERR_BUFSZ);
      snprintf (buff, ERR_BUFSZ, "Unknown parameter %s requested", param);
      iot_log_error (driver->lc, buff);
      * exception = iot_data_alloc_string (buff, IOT_DATA_TAKE);
      return false;
    }
  }
  return true;
}

static bool gyro_put_handler
(
  void * impl,
  const char * devname,
  const devsdk_protocols * protocols,
  uint32_t nvalues,
  const devsdk_commandrequest * requests,
  const iot_data_t * values[],
  iot_data_t ** exception
)
{
  gyro_driver * driver = (gyro_driver *) impl;
  iot_log_error (driver->lc, ERR_GYRO_WRITE);
  * exception = iot_data_alloc_string (ERR_GYRO_WRITE, IOT_DATA_REF);
  return false;
}

/* ---- Stop ---- */
/* Stop performs any final actions before the device service is terminated */
static void gyro_stop (void * impl, bool force)
{
}

int main (int argc, char * argv[])
{
  sigset_t set;
  int sigret;

  gyro_driver * impl = malloc (sizeof (gyro_driver));
  impl->lc = NULL;

  devsdk_error e;
  e.code = 0;

  devsdk_callbacks gyroImpls =
  {
    gyro_init,         /* Initialize */
    NULL,              /* Discovery */
    gyro_get_handler,  /* Get */
    gyro_put_handler,  /* Put */
    gyro_stop          /* Stop */
  };

  devsdk_service_t * service = devsdk_service_new
    ("device-gyro", "1.0", impl, gyroImpls, &argc, argv, &e);
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

  devsdk_service_start (service, &e);
  ERR_CHECK (e);

  sigemptyset (&set);
  sigaddset (&set, SIGINT);
  sigwait (&set, &sigret);

  devsdk_service_stop (service, true, &e);
  ERR_CHECK (e);

  devsdk_service_free (service);
  free (impl);
  return 0;
}
