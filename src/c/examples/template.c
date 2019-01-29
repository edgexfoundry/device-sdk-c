/* template implementation of an Edgex device service using C SDK */

/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "edgex/devsdk.h"

#include <unistd.h>
#include <signal.h>

#define ERR_CHECK(x) if (x.code) { fprintf (stderr, "Error: %d: %s\n", x.code, x.reason); return x.code; }

typedef struct template_driver
{
  iot_logging_client * lc;
  bool state_flag;
  pthread_mutex_t mutex;
} template_driver;


static sig_atomic_t running = true;
static void inthandler (int i)
{
  running = (i != SIGINT);
}


/* --- Initialize ---- */
/* Initialize performs protocol-specific initialization for the device
 * service.
 */
static bool template_init
(
  void *impl,
  struct iot_logging_client *lc,
  const edgex_nvpairs *config
)
{
  template_driver *driver = (template_driver *) impl;
  lc=iot_log_default;
  driver->lc = lc;
  driver->state_flag=false;
  pthread_mutex_init (&driver->mutex, NULL);
  iot_log_debug(driver->lc,"Init");
  return true;
}

/* ---- Discovery ---- */
/* Device services which are capable of device discovery should implement it
 * in this callback. It is called in response to a request on the
 * device service's discovery REST endpoint. New devices should be added using
 * the edgex_device_add_device() method
 */
static void template_discover (void *impl) {}

/* ---- Get ---- */
/* Get triggers an asynchronous protocol specific GET operation.
 * The device to query is specified by the addressable devaddr. nreadings is
 * the number of values being requested and defines the size of the requests
 * and readings arrays. For each value, the commandrequest holds information
 * as to what is being requested. The implementation of this method should
 * query the device accordingly and write the resulting value into the
 * commandresult.
 *
 * Note - In a commandrequest, the DeviceObject represents a deviceResource
 * which is defined in the device profile.
*/
static bool template_get_handler
(
  void *impl,
  const edgex_addressable *devaddr,
  uint32_t nreadings,
  const edgex_device_commandrequest *requests,
  edgex_device_commandresult *readings
)
{
  template_driver *driver = (template_driver *) impl;

  /* Access the address of the device to be accessed and log it */
  iot_log_debug(driver->lc, "GET on address: %s",devaddr->address);

  for (uint32_t i = 0; i < nreadings; i++)
  {
  /* Drill into the attributes to differentiate between resources via the
   * SensorType NVP */
    edgex_nvpairs * current = requests->devobj->attributes;
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
        }
        else if (strcmp (current->value, "2") ==0 )
        {
          /* Set the reading as a random value between 0 and 1000 */
          readings[i].value.ui64_result = rand() % 1000;
        }
      }

      if (strcmp (current->name, "SwitchID") ==0 )
      {
        readings[i].type = Bool;
        pthread_mutex_lock (&driver->mutex);
        readings[i].value.bool_result=driver->state_flag;
        pthread_mutex_unlock (&driver->mutex);
      }
      current = current->next;
    }
  }
  return true;
}

/* ---- Put ---- */
/* Put triggers an asynchronous protocol specific SET operation.
 * The device to set values on is specified by the addressable devaddr.
 * nvalues is the number of values to be set and defines the size of the
 * requests and values arrays. For each value, the commandresult holds the
 * value, and the commandrequest holds information as to where it is to be
 * written. The implementation of this method should effect the write to the
 * device.
 *
 * Note - In a commandrequest, the DeviceObject represents a deviceResource
 * which is defined in the device profile.
*/
static bool template_put_handler
(
  void *impl,
  const edgex_addressable *devaddr,
  uint32_t nvalues,
  const edgex_device_commandrequest *requests,
  const edgex_device_commandresult *values
)
{
  template_driver *driver = (template_driver *) impl;

  /* Access the address of the device to be accessed and log it */
  iot_log_debug(driver->lc, "PUT on address: %s",devaddr->address);

  for (uint32_t i = 0; i < nvalues; i++)
  {
    /* A Device Service again makes use of the data provided to perform a PUT */

    /* In this case we set a boolean flag */
    if (strcmp (requests[i].devobj->name,"Switch") ==0 )
    {
      pthread_mutex_lock (&driver->mutex);
      driver->state_flag=values->value.bool_result;
      pthread_mutex_unlock (&driver->mutex);
    }
  }
  return true;
}

/* ---- Disconnect ---- */
/* Disconnect handles protocol-specific cleanup when a device is removed. */
static bool template_disconnect (void *impl, edgex_addressable *device)
{
  return true;
}

/* ---- Stop ---- */
/* Stop performs any final actions before the device service is terminated */
static void template_stop (void *impl, bool force) {}


static void usage (void)
{
  printf ("Options: \n");
  printf ("   -h, --help           : Show this text\n");
  printf ("   -r, --registry       : Use the registry service\n");
  printf ("   -p, --profile <name> : Set the profile name\n");
  printf ("   -c, --confdir <dir>  : Set the configuration directory\n");
}

int main (int argc, char *argv[])
{
  char *profile = "";
  char *confdir = "";
  char *regURL = NULL;
  template_driver * impl = malloc (sizeof (template_driver));
  memset (impl, 0, sizeof (template_driver));

  int n = 1;
  while (n < argc)
  {
    if (strcmp (argv[n], "-h") == 0 || strcmp (argv[n], "--help") == 0)
    {
      usage ();
      return 0;
    }
    if (strcmp (argv[n], "-r") == 0 || strcmp (argv[n], "--registry") == 0)
    {
      regURL = "consul://localhost:8500";
      n++;
      continue;
    }
    if (strcmp (argv[n], "-p") == 0 || strcmp (argv[n], "--profile") == 0)
    {
      profile = argv[n + 1];
      n += 2;
      continue;
    }
    if (strcmp (argv[n], "-c") == 0 || strcmp (argv[n], "--confdir") == 0)
    {
      confdir = argv[n + 1];
      n += 2;
      continue;
    }
    printf ("Unknown option %s\n", argv[n]);
    usage ();
    return 0;
  }

  edgex_error e;
  e.code = 0;

  /* Device Callbacks */
  edgex_device_callbacks templateImpls =
  {
    template_init,         /* Initialize */
    template_discover,     /* Discovery */
    template_get_handler,  /* Get */
    template_put_handler,  /* Put */
    template_disconnect,   /* Disconnect */
    template_stop          /* Stop */
  };

  /* Initalise a new device service */
  edgex_device_service *service = edgex_device_service_new
  (
    "device-template",
    "1.0",
    impl,
    templateImpls,
    &e
  );
  ERR_CHECK (e);

  /* Start the device service*/
  edgex_device_service_start (service, regURL, profile, confdir, &e);
  ERR_CHECK (e);

  signal (SIGINT, inthandler);
  running = true;
  while (running)
  {
    sleep(1);
  }

  /* Stop the device service */
  edgex_device_service_stop (service, true, &e);
  ERR_CHECK (e);

  free (impl);
  exit (0);
  return 0;
}
