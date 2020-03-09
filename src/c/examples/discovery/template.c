/* template implementation of an Edgex device service using C SDK */

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

typedef struct template_driver
{
  iot_logger_t * lc;
  devsdk_service_t *svc;
} template_driver;

static void dump_protocols (iot_logger_t *lc, const devsdk_protocols *prots)
{
  iot_log_debug (lc, " [Other] protocol:");
  for (const devsdk_nvpairs *nv = devsdk_protocols_properties (prots, "Other"); nv; nv = nv->next)
  {
    iot_log_debug (lc, "    %s = %s", nv->name, nv->value);
  }
}

static void dump_attributes (iot_logger_t *lc, const devsdk_nvpairs *attrs)
{
  for (const devsdk_nvpairs *a = attrs; a; a = a->next)
  {
    iot_log_debug (lc, "    %s = %s", a->name, a->value);
  }
}

/* --- Initialize ---- */
/* Initialize performs protocol-specific initialization for the device
 * service.
 */
static bool template_init
(
  void *impl,
  struct iot_logger_t *lc,
  const iot_data_t *config
)
{
  template_driver *driver = (template_driver *) impl;
  iot_log_debug (lc, "Template Init. Driver Config follows:");
  if (config)
  {
    iot_data_map_iter_t iter;
    iot_data_map_iter (config, &iter);
    while (iot_data_map_iter_next (&iter))
    {
      iot_log_debug (lc, "    %s = %s", iot_data_map_iter_string_key (&iter), iot_data_map_iter_string_value (&iter));
    }
  }
  driver->lc = lc;
  iot_log_debug (lc, "Template Init done");
  return true;
}

/* ---- Discovery ---- */
/* Device services which are capable of device discovery should implement it
 * in this callback. It is called in response to a request on the
 * device service's discovery REST endpoint. New devices should be added using
 * the devsdk_add_device() method
 */
static void template_discover (void *impl)
{
  template_driver *driver = (template_driver *) impl;

  devsdk_nvpairs *nv1 = devsdk_nvpairs_new ("MAC", "00-05-1B-A1-99-00", NULL);
  devsdk_nvpairs *nv2 = devsdk_nvpairs_new ("MAC", "00-05-1B-A1-99-99", NULL);
  devsdk_nvpairs *nv3 = devsdk_nvpairs_new ("HTTP", "10.0.0.254", NULL);
  devsdk_nvpairs *nv4 = devsdk_nvpairs_new ("HTTP", "10.0.0.255", NULL);

  devsdk_protocols *p1 = devsdk_protocols_new ("MAC Address", nv1, NULL);
  devsdk_protocols *p2 = devsdk_protocols_new ("MAC Address", nv2, NULL);
  devsdk_protocols *p3 = devsdk_protocols_new ("IP Address", nv3, NULL);
  devsdk_protocols *p4 = devsdk_protocols_new ("IP Address", nv4, NULL);

  devsdk_discovered_device devs[] =
  {
    { "DiscoveredOne", p1, "First discovered device", NULL },
    { "DiscoveredTwo", p2, "Second discovered device", NULL },
    { "DiscoveredThree", p3, "Third discovered device", NULL },
    { "DiscoveredFour", p4, "Fourth discovered device", NULL }
  };

  devsdk_add_discovered_devices (driver->svc, 4, devs);

  devsdk_nvpairs_free (nv1);
  devsdk_nvpairs_free (nv2);
  devsdk_nvpairs_free (nv3);
  devsdk_nvpairs_free (nv4);
  devsdk_protocols_free (p1);
  devsdk_protocols_free (p2);
  devsdk_protocols_free (p3);
  devsdk_protocols_free (p4);
}

/* ---- Get ---- */
/* Get triggers an asynchronous protocol specific GET operation.
 * The device to query is specified by the protocols. nreadings is
 * the number of values being requested and defines the size of the requests
 * and readings arrays. For each value, the commandrequest holds information
 * as to what is being requested. The implementation of this method should
 * query the device accordingly and write the resulting value into the
 * commandresult.
 *
 * Note - In a commandrequest, the DeviceResource represents a deviceResource
 * which is defined in the device profile.
*/
static bool template_get_handler
(
  void *impl,
  const char *devname,
  const devsdk_protocols *protocols,
  uint32_t nreadings,
  const devsdk_commandrequest *requests,
  devsdk_commandresult *readings,
  const devsdk_nvpairs *qparams,
  iot_data_t **exception
)
{
  template_driver *driver = (template_driver *) impl;

  /* Access the location of the device to be accessed and log it */
  iot_log_debug(driver->lc, "GET on device:");
  dump_protocols (driver->lc, protocols);

  for (uint32_t i = 0; i < nreadings; i++)
  {
    /* Log the attributes for each requested resource */
    iot_log_debug (driver->lc, "  Requested reading %u:", i);
    dump_attributes (driver->lc, requests[i].attributes);
    /* Fill in a result regardless */
    readings[i].value = iot_data_alloc_string ("Template result", IOT_DATA_REF);
  }
  return true;
}

/* ---- Put ---- */
/* Put triggers an asynchronous protocol specific SET operation.
 * The device to set values on is specified by the protocols.
 * nvalues is the number of values to be set and defines the size of the
 * requests and values arrays. For each value, the commandresult holds the
 * value, and the commandrequest holds information as to where it is to be
 * written. The implementation of this method should effect the write to the
 * device.
 *
 * Note - In a commandrequest, the DeviceResource represents a deviceResource
 * which is defined in the device profile.
*/
static bool template_put_handler
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
  template_driver *driver = (template_driver *) impl;
  /* Access the location of the device to be accessed and log it */
  iot_log_debug (driver->lc, "PUT on device:");
  dump_protocols (driver->lc, protocols);

  for (uint32_t i = 0; i < nvalues; i++)
  {
    /* A Device Service again makes use of the data provided to perform a PUT */
    /* Log the attributes */
    iot_log_debug (driver->lc, "  Requested device write %u:", i);
    dump_attributes (driver->lc, requests[i].attributes);
    switch (iot_data_type (values[i]))
    {
      case IOT_DATA_STRING:
        iot_log_debug (driver->lc, "  Value: %s", iot_data_string (values[i]));
        break;
      case IOT_DATA_UINT64:
        iot_log_debug (driver->lc, "  Value: %lu", iot_data_ui64 (values[i]));
        break;
      case IOT_DATA_BOOL:
        iot_log_debug (driver->lc, "  Value: %s", iot_data_bool (values[i]) ? "true" : "false");
        break;
      /* etc etc */
      default:
        iot_log_debug (driver->lc, "  Value has unexpected type %s: %s", iot_data_type_name (values[i]), iot_data_to_json (values[i]));
    }
  }
  return true;
}

/* ---- Stop ---- */
/* Stop performs any final actions before the device service is terminated */
static void template_stop (void *impl, bool force) {}

int main (int argc, char *argv[])
{
  sigset_t set;
  int sigret;

  template_driver * impl = malloc (sizeof (template_driver));
  memset (impl, 0, sizeof (template_driver));

  devsdk_error e;
  e.code = 0;

  /* Device Callbacks */
  devsdk_callbacks templateImpls =
  {
    template_init,         /* Initialize */
    template_discover,     /* Discovery */
    template_get_handler,  /* Get */
    template_put_handler,  /* Put */
    template_stop          /* Stop */
  };

  /* Initalise a new device service */
  devsdk_service_t *service = devsdk_service_new
    ("device-template", "1.0", impl, templateImpls, &argc, argv, &e);
  ERR_CHECK (e);

  int n = 1;
  while (n < argc)
  {
    if (strcmp (argv[n], "-h") == 0 || strcmp (argv[n], "--help") == 0)
    {
      printf ("Options:\n");
      printf ("  -h, --help\t\t: Show this text\n");
      devsdk_usage ();
      return 0;
    }
    else
    {
      printf ("%s: Unrecognized option %s\n", argv[0], argv[n]);
      return 0;
    }
  }

  impl->svc = service;

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
