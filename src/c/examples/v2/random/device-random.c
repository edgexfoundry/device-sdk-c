/*
 * Copyright (c) 2018-2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "devsdk/devsdk.h"

#include <unistd.h>
#include <signal.h>

#define ERR_CHECK(x) if (x.code) { fprintf (stderr, "Error: %d: %s\n", x.code, x.reason); devsdk_service_free (service); free (impl); return x.code; }

typedef struct random_driver
{
  iot_logger_t * lc;
  bool state_flag;
  pthread_mutex_t mutex;
} random_driver;

static bool random_init
(
  void *impl,
  struct iot_logger_t *lc,
  const iot_data_t *config
)
{
  random_driver *driver = (random_driver *) impl;
  driver->lc = lc;
  driver->state_flag=false;
  pthread_mutex_init (&driver->mutex, NULL);
  iot_log_debug(driver->lc,"Init");
  return true;
}

static void random_discover (void *impl) {}

static bool random_get_handler
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
  random_driver *driver = (random_driver *) impl;

  for (uint32_t i = 0; i < nreadings; i++)
  {
    /* Use the attributes to differentiate between requests */
    unsigned long stype = 0;
    const char *swid = NULL;
    if (devsdk_nvpairs_ulong_value (requests[i].attributes, "SensorType", &stype))
    {
      switch (stype)
      {
        case 1:
          /* Set the reading as a random value between 0 and 100 */
          readings[i].value = iot_data_alloc_ui64 (rand() % 100);
          break;
        case 2:
          /* Set the reading as a random value between 0 and 1000 */
          readings[i].value = iot_data_alloc_ui64 (rand() % 1000);
          break;
        default:
          iot_log_error (driver->lc, "%lu is not a valid SensorType", stype);
          return false;
      }
    }
    else if ((swid = devsdk_nvpairs_value (requests[i].attributes, "SwitchID")))
    {
      pthread_mutex_lock (&driver->mutex);
      readings[i].value = iot_data_alloc_bool (driver->state_flag);
      pthread_mutex_unlock (&driver->mutex);
    }
    else
    {
      iot_log_error (driver->lc, "%s: Neither SensorType nor SwitchID were given", requests[i].resname);
      return false;
    }
  }
  return true;
}

static bool random_put_handler
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
  random_driver *driver = (random_driver *) impl;
  bool result = true;

  for (uint32_t i = 0; i < nvalues; i++)
  {
    /* A Device Service again makes use of the data provided to perform a PUT */

    /* In this case we set a boolean flag */
    if (devsdk_nvpairs_value (requests[i].attributes, "SwitchID"))
    {
      pthread_mutex_lock (&driver->mutex);
      driver->state_flag = iot_data_bool (values[i]);
      pthread_mutex_unlock (&driver->mutex);
    }
    else
    {
      iot_log_error (driver->lc, "PUT not valid for resource %s", requests[i].resname);
      result = false;
    }
  }
  return result;
}

static void random_stop (void *impl, bool force) {}

int main (int argc, char *argv[])
{
  sigset_t set;
  int sigret;

  random_driver * impl = malloc (sizeof (random_driver));
  memset (impl, 0, sizeof (random_driver));

  devsdk_error e;
  e.code = 0;

  /* Device Callbacks */
  devsdk_callbacks randomImpls =
  {
    random_init,         /* Initialize */
    random_discover,     /* Discovery */
    random_get_handler,  /* Get */
    random_put_handler,  /* Put */
    random_stop          /* Stop */
  };

  /* Initalise a new device service */
  devsdk_service_t *service = devsdk_service_new
    ("device-random", "1.0", impl, randomImpls, &argc, argv, &e);
  ERR_CHECK (e);

  int n = 1;
  while (n < argc)
  {
    if (strcmp (argv[n], "-h") == 0 || strcmp (argv[n], "--help") == 0)
    {
      printf ("Options:\n");
      printf ("  -h, --help\t\t: Show this text\n");
      return 0;
    }
    else
    {
      printf ("%s: Unrecognized option %s\n", argv[0], argv[n]);
      return 0;
    }
  }

  /* Start the device service*/
  devsdk_service_start (service, &e);
  ERR_CHECK (e);

  /* Wait for interrupt */
  sigemptyset (&set);
  sigaddset (&set, SIGINT);
  sigwait (&set, &sigret);

  /* Stop the device service */
  devsdk_service_stop (service, true, &e);
  ERR_CHECK (e);

  devsdk_service_free (service);
  free (impl);
  return 0;
}
