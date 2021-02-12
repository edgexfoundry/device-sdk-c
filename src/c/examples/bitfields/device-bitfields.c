/* Pseudo-device service illustrating bitfield access using C SDK and mask/shift transforms */

/*
 * Copyright (c) 2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <unistd.h>
#include <signal.h>
#include <stdatomic.h>

#include "devsdk/devsdk.h"

#define ERR_CHECK(x) if (x.code) { fprintf (stderr, "Error: %d: %s\n", x.code, x.reason); devsdk_service_free (service); free (impl); return x.code; }

typedef struct bitfield_driver
{
  iot_logger_t * lc;
  atomic_uint_fast32_t data;
} bitfield_driver;

static bool bitfield_init
  (void * impl, struct iot_logger_t * lc, const iot_data_t * config)
{
  bitfield_driver * driver = (bitfield_driver *) impl;
  driver->lc = lc;
  driver->data = 0x12345678;
  return true;
}

static bool bitfield_get_handler
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
  bitfield_driver * driver = (bitfield_driver *) impl;

  for (uint32_t i = 0; i < nreadings; i++)
  {
    iot_log_info (driver->lc, "Reading data, value is %.8x", driver->data);
    readings[i].value = iot_data_alloc_i32 (driver->data);
  }
  return true;
}

static bool bitfield_put_handler
(
  void * impl,
  const char * devname,
  const devsdk_protocols * protocols,
  uint32_t nvalues,
  const devsdk_commandrequest * requests,
  const iot_data_t * values[],
  const devsdk_nvpairs *qparams,
  iot_data_t ** exception
)
{
  bitfield_driver * driver = (bitfield_driver *) impl;
  for (uint32_t i = 0; i < nvalues; i++)
  {
    if (iot_data_type (values[i]) != IOT_DATA_UINT32)
    {
      *exception = iot_data_alloc_string ("Wrong datatype for bitfield write; must be uint32", IOT_DATA_REF);
      return false;
    }
  }
  for (uint32_t i = 0; i < nvalues; i++)
  {
    iot_log_info (driver->lc, "Writing data, original value is %.8x", driver->data);
    if (requests[i].mask)
    {
      // driver->data = (driver->data & requests[i].mask) | iot_data_ui32 (values[i]);  but do so atomically
      uint_fast32_t d;
      uint_fast32_t result;
      do
      {
        d = driver->data;
        result = (d & requests[i].mask) | iot_data_ui32 (values[i]);
      }
      while (!atomic_compare_exchange_weak (&driver->data, &d, result));
    }
    else 
    {
      driver->data = iot_data_ui32 (values[i]);
    }
    iot_log_info (driver->lc, "Written data, new value is %.8x", driver->data);
  }
  return true;
}

static void bitfield_stop (void * impl, bool force)
{
}

int main (int argc, char * argv[])
{
  sigset_t set;
  int sigret;

  bitfield_driver * impl = malloc (sizeof (bitfield_driver));
  impl->lc = NULL;

  devsdk_error e;
  e.code = 0;

  devsdk_callbacks bitfieldImpls =
  {
    bitfield_init,         /* Initialize */
    NULL,              /* Reconfigure */
    NULL,              /* Discovery */
    bitfield_get_handler,  /* Get */
    bitfield_put_handler,  /* Put */
    bitfield_stop          /* Stop */
  };

  devsdk_service_t * service = devsdk_service_new
    ("device-bitfield", "1.0", impl, bitfieldImpls, &argc, argv, &e);
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
