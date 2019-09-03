/*
 * Copyright (c) 2018-2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "edgex/devsdk.h"

#include <unistd.h>
#include <signal.h>

#define ERR_CHECK(x) if (x.code) { fprintf (stderr, "Error: %d: %s\n", x.code, x.reason); edgex_device_service_free (service); free (impl); return x.code; }

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
  const edgex_nvpairs *config
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
  const edgex_protocols *protocols,
  uint32_t nreadings,
  const edgex_device_commandrequest *requests,
  edgex_device_commandresult *readings
)
{
  random_driver *driver = (random_driver *) impl;
  bool result = true;

  for (uint32_t i = 0; i < nreadings; i++)
  {
    result = false;
    /* Drill into the attributes to differentiate between resources via the SensorType NVP */
    const edgex_nvpairs * current = requests[i].attributes;
    while (current!=NULL)
    {
      if (strcmp (current->name, "SensorType") ==0 )
      {
        /* Set the resulting reading type as Uint64 */
        readings[i].type = Uint64;

        if (strcmp (current->value, "1") ==0 )
        {
          /* Set the reading as a random value between 0 and 100 */
          readings[i].value.ui64_result = rand() % 100;
          result = true;
        }
        else if (strcmp (current->value, "2") ==0 )
        {
          /* Set the reading as a random value between 0 and 1000 */
          readings[i].value.ui64_result = rand() % 1000;
          result = true;
        }
        else
        {
          iot_log_error (driver->lc, "%s is not a valid SensorType", current->value);
        }
      }

      if (strcmp (current->name, "SwitchID") ==0 )
      {
        readings[i].type = Bool;
        pthread_mutex_lock (&driver->mutex);
        readings[i].value.bool_result=driver->state_flag;
        pthread_mutex_unlock (&driver->mutex);
        result = true;
      }
      current = current->next;
    }
    if (!result)
    {
      break;
    }
  }
  return result;
}

static bool random_put_handler
(
  void *impl,
  const char *devname,
  const edgex_protocols *protocols,
  uint32_t nvalues,
  const edgex_device_commandrequest *requests,
  const edgex_device_commandresult *values
)
{
  random_driver *driver = (random_driver *) impl;
  bool result = true;

  for (uint32_t i = 0; i < nvalues; i++)
  {
    /* A Device Service again makes use of the data provided to perform a PUT */

    /* In this case we set a boolean flag */
    if (strcmp (requests[i].resname,"Switch") ==0 )
    {
      pthread_mutex_lock (&driver->mutex);
      driver->state_flag=values[i].value.bool_result;
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

static bool random_disconnect (void *impl, edgex_protocols *device)
{
  return true;
}

static void random_stop (void *impl, bool force) {}

int main (int argc, char *argv[])
{
  edgex_device_svcparams params = { "device-random", "", "", "" };
  sigset_t set;
  int sigret;

  random_driver * impl = malloc (sizeof (random_driver));
  memset (impl, 0, sizeof (random_driver));

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

  /* Device Callbacks */
  edgex_device_callbacks randomImpls =
  {
    random_init,         /* Initialize */
    random_discover,     /* Discovery */
    random_get_handler,  /* Get */
    random_put_handler,  /* Put */
    random_disconnect,   /* Disconnect */
    random_stop          /* Stop */
  };

  /* Initalise a new device service */
  edgex_device_service *service = edgex_device_service_new
    (params.svcname, "1.0", impl, randomImpls, &e);
  ERR_CHECK (e);

  /* Start the device service*/
  edgex_device_service_start (service, params.regURL, params.profile, params.confdir, &e);
  ERR_CHECK (e);

  /* Wait for interrupt */
  sigemptyset (&set);
  sigaddset (&set, SIGINT);
  sigwait (&set, &sigret);

  /* Stop the device service */
  edgex_device_service_stop (service, true, &e);
  ERR_CHECK (e);

  edgex_device_service_free (service);
  free (impl);
  return 0;
}
