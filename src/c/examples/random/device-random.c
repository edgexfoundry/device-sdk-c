/*
 * Copyright (c) 2018-2021
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "devsdk/devsdk.h"

#include <unistd.h>
#include <signal.h>
#include <stdarg.h>

#define ERR_CHECK(x) if (x.code) { fprintf (stderr, "Error: %d: %s\n", x.code, x.reason); devsdk_service_free (service); free (impl); return x.code; }

typedef enum { RANDOM_R100, RANDOM_R1000, RANDOM_SW } random_resourcetype;

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

static bool random_get_handler
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
  random_driver *driver = (random_driver *) impl;

  for (uint32_t i = 0; i < nreadings; i++)
  {
    switch (*(random_resourcetype *)requests[i].resource->attrs)
    {
      case RANDOM_R100:
        /* Set the reading as a random value between 0 and 100 */
        readings[i].value = iot_data_alloc_ui64 (rand() % 100);
        break;
      case RANDOM_R1000:
        /* Set the reading as a random value between 0 and 1000 */
        readings[i].value = iot_data_alloc_ui64 (rand() % 1000);
        break;
      case RANDOM_SW:
        /* Get the switch value */
        pthread_mutex_lock (&driver->mutex);
        readings[i].value = iot_data_alloc_bool (driver->state_flag);
        pthread_mutex_unlock (&driver->mutex);
        break;
    }
  }
  return true;
}

static bool random_put_handler
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
  random_driver *driver = (random_driver *) impl;

  for (uint32_t i = 0; i < nvalues; i++)
  {
    if (*(random_resourcetype *)requests[i].resource->attrs == RANDOM_SW)
    {
      /* set a boolean flag */
      pthread_mutex_lock (&driver->mutex);
      driver->state_flag = iot_data_bool (values[i]);
      pthread_mutex_unlock (&driver->mutex);
    }
    else
    {
      *exception = iot_data_alloc_string ("PUT not valid for this resource", IOT_DATA_REF);
      return false;
    }
  }
  return true;
}

static void random_stop (void *impl, bool force) {}

static devsdk_address_t random_create_addr (void *impl, const devsdk_protocols *protocols, iot_data_t **exception)
{
  return (devsdk_address_t)protocols;
}

static void random_free_addr (void *impl, devsdk_address_t address)
{
}

static devsdk_resource_attr_t random_create_resource_attr (void *impl, const iot_data_t *attributes, iot_data_t **exception)
{
  random_resourcetype *attr;
  random_resourcetype result;

  switch (iot_data_string_map_get_i64 (attributes, "SensorType", 0))
  {
    case 0:
      if (iot_data_string_map_get (attributes, "SwitchID"))
      {
        result = RANDOM_SW;
      }
      else
      {
        *exception = iot_data_alloc_string ("random: either SensorType or SwitchID attributes are required", IOT_DATA_REF);
        return NULL;
      }
      break;
    case 1:
      result = RANDOM_R100;
      break;
    case 2:
      result = RANDOM_R1000;
      break;
    default:
      *exception = iot_data_alloc_string ("random: out-of-range SensorType specified", IOT_DATA_REF);
      return NULL;
  }
  attr = malloc (sizeof (random_resourcetype));
  *attr = result;
  return attr;
}

static void random_free_resource_attr (void *impl, devsdk_resource_attr_t resource)
{
  free (resource);
}


int main (int argc, char *argv[])
{
  sigset_t set;
  int sigret;

  random_driver * impl = malloc (sizeof (random_driver));
  memset (impl, 0, sizeof (random_driver));

  devsdk_error e;
  e.code = 0;

  /* Device Callbacks */
  devsdk_callbacks *randomImpls = devsdk_callbacks_init
  (
    random_init,
    NULL,
    random_get_handler,
    random_put_handler,
    random_stop,
    random_create_addr,
    random_free_addr,
    random_create_resource_attr,
    random_free_resource_attr
  );

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
      printf ("  -h, --help\t\t\tShow this text\n");
      return 0;
    }
    else
    {
      printf ("%s: Unrecognized option %s\n", argv[0], argv[n]);
      return 0;
    }
  }

  /* Start the device service*/
  devsdk_service_start (service, NULL, &e);
  ERR_CHECK (e);

  /* Wait for interrupt */
  sigemptyset (&set);
  sigaddset (&set, SIGINT);
  sigprocmask (SIG_BLOCK, &set, NULL);
  sigwait (&set, &sigret);
  sigprocmask (SIG_UNBLOCK, &set, NULL);

  /* Stop the device service */
  devsdk_service_stop (service, true, &e);
  ERR_CHECK (e);

  devsdk_service_free (service);
  free (impl);
  free (randomImpls);
  return 0;
}
