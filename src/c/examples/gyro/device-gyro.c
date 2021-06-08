/* Pseudo-device service illustrating resource aggregation using C SDK */

/*
 * Copyright (c) 2018-2021
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

typedef enum { GYRO_XROT, GYRO_YROT, GYRO_ZROT, GYRO_INVALID } gyro_resourcetype;

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
static devsdk_address_t gyro_create_addr (void *impl, const devsdk_protocols *protocols, iot_data_t **exception)
{
  return (devsdk_address_t)protocols;
}

static void gyro_free_addr (void *impl, devsdk_address_t address)
{
}

static devsdk_resource_attr_t gyro_create_resource_attr (void *impl, const iot_data_t *attributes, iot_data_t **exception)
{
  gyro_resourcetype *attr = NULL;
  gyro_resourcetype result = GYRO_INVALID;

  const char *param = iot_data_string_map_get_string (attributes, "parameter");
  if (param)
  {
    if (strcmp (param, "xrot") == 0)
    {
      result = GYRO_XROT;
    }
    else if (strcmp (param, "yrot") == 0)
    {
      result = GYRO_YROT;
    }
    else if (strcmp (param, "zrot") == 0)
    {
      result = GYRO_ZROT;
    }
    else
    {
      *exception = iot_data_alloc_string ("gyro: invalid value specified for \"parameter\"", IOT_DATA_REF);
    }
  }
  else
  {
    *exception = iot_data_alloc_string ("gyro: \"parameter\" is required", IOT_DATA_REF);
  }
  if (result != GYRO_INVALID)
  {
    attr = malloc (sizeof (gyro_resourcetype));
    *attr = result;
  }
  return attr;
}

static void gyro_free_resource_attr (void *impl, devsdk_resource_attr_t resource)
{
  free (resource);
}

static bool gyro_get_handler
(
  void * impl,
  const devsdk_device_t *device,
  uint32_t nreadings,
  const devsdk_commandrequest * requests,
  devsdk_commandresult * readings,
  const iot_data_t * options,
  iot_data_t ** exception
)
{
  for (uint32_t i = 0; i < nreadings; i++)
  {
    switch (*(gyro_resourcetype *)requests[i].resource->attrs)
    {
      case GYRO_XROT:
        readings[i].value = iot_data_alloc_i32 ((random () % 501) - 250);
        break;
      case GYRO_YROT:
        readings[i].value = iot_data_alloc_i32 ((random () % 501) - 250);
        break;
      case GYRO_ZROT:
        readings[i].value = iot_data_alloc_i32 ((random () % 501) - 250);
        break;
      default:
        *exception = iot_data_alloc_string ("gyro: internal error (invalid resourcetype)", IOT_DATA_REF);
        return false;
    }
  }
  return true;
}

static bool gyro_put_handler
(
  void * impl,
  const devsdk_device_t *device,
  uint32_t nvalues,
  const devsdk_commandrequest * requests,
  const iot_data_t * values[],
  const iot_data_t *options,
  iot_data_t ** exception
)
{
  *exception = iot_data_alloc_string ("PUT called for gyro device. This is a read-only device.", IOT_DATA_REF);
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

  devsdk_callbacks *gyroImpls = devsdk_callbacks_init
  (
    gyro_init,
    NULL,
    gyro_get_handler,
    gyro_put_handler,
    gyro_stop,
    gyro_create_addr,
    gyro_free_addr,
    gyro_create_resource_attr,
    gyro_free_resource_attr
  );

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
  free (gyroImpls);
  return 0;
}
