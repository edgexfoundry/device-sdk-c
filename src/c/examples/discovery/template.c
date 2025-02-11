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
  bool disc_run;
} template_driver;

static void dump_protocols (iot_logger_t *lc, const devsdk_protocols *prots)
{
  iot_data_map_iter_t iter;
  iot_log_debug (lc, " [Other] protocol:");
  const iot_data_t *others = devsdk_protocols_properties(prots, "Other");
  if ((!others) || (iot_data_type(others) != IOT_DATA_MAP))
  {
    return;
  }
  iot_data_map_iter (others, &iter);
  while (iot_data_map_iter_next (&iter))
  {
    iot_log_debug (lc, "    %s = %s", iot_data_map_iter_string_key (&iter), iot_data_map_iter_string_value (&iter));
  }
}

static void dump_attributes (iot_logger_t *lc, devsdk_resource_attr_t attrs)
{
  iot_data_map_iter_t iter;
  if ((!attrs) || (iot_data_type((iot_data_t *)attrs) != IOT_DATA_MAP))
  {
    return;
  }
  iot_data_map_iter ((iot_data_t *)attrs, &iter);
  while (iot_data_map_iter_next (&iter))
  {
    iot_log_debug (lc, "    %s = %s", iot_data_map_iter_string_key (&iter), iot_data_map_iter_string_value (&iter));
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
  if (config && (iot_data_type(config) == IOT_DATA_MAP))
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

/* --- Reconfigure ---- */
/* Reconfigure is called if the driver configuration is updated.
 */
static void template_reconfigure
(
  void *impl,
  const iot_data_t *config
)
{
  iot_data_map_iter_t iter;
  template_driver *driver = (template_driver *) impl;
  iot_log_debug (driver->lc, "Template Reconfiguration. New Config follows:");
  if (config && (iot_data_type(config) == IOT_DATA_MAP))
  {
    iot_data_map_iter (config, &iter);
    while (iot_data_map_iter_next (&iter))
    {
      iot_log_debug (driver->lc, "    %s = %s", iot_data_map_iter_string_key (&iter), iot_data_map_iter_string_value (&iter));
    }
  }
}

/* ---- Discovery ---- */
/* Device services which are capable of device discovery should implement it
 * in this callback. It is called in response to a request on the
 * device service's discovery REST endpoint. New devices should be added using
 * the devsdk_add_discovered_devices() method
 */
static void template_discover (void *impl, const char *request_id)
{
  template_driver *driver = (template_driver *) impl;

  iot_data_t *map1 = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_t *map2 = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_t *map3 = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_t *map4 = iot_data_alloc_map (IOT_DATA_STRING);

  iot_data_string_map_add (map1, "MAC", iot_data_alloc_string ("00-05-1B-A1-99-00", IOT_DATA_REF));
  iot_data_string_map_add (map2, "MAC", iot_data_alloc_string ("00-05-1B-A1-99-99", IOT_DATA_REF));
  iot_data_string_map_add (map3, "HTTP", iot_data_alloc_string ("10.0.0.254", IOT_DATA_REF));
  iot_data_string_map_add (map4, "HTTP", iot_data_alloc_string ("10.0.0.255", IOT_DATA_REF));

  devsdk_protocols *p1 = devsdk_protocols_new ("MAC Address", map1, NULL);
  devsdk_protocols *p2 = devsdk_protocols_new ("MAC Address", map2, NULL);
  devsdk_protocols *p3 = devsdk_protocols_new ("IP Address", map3, NULL);
  devsdk_protocols *p4 = devsdk_protocols_new ("IP Address", map4, NULL);

  devsdk_discovered_device devs[] =
  {
    { "DiscoveredOne", NULL, p1, "First discovered device", NULL },
    { "DiscoveredTwo", NULL, p2, "Second discovered device", NULL },
    { "DiscoveredThree", NULL, p3, "Third discovered device", NULL },
    { "DiscoveredFour", NULL, p4, "Fourth discovered device", NULL }
  };

  // Publish event
  devsdk_publish_discovery_event (driver->svc, request_id, 100, 4);

  devsdk_add_discovered_devices (driver->svc, 4, devs);

  for (int i = 0; i < 10; i++)
  {
    iot_log_debug(driver->lc, "Waiting for discovery delete");
    if (!driver->disc_run)
    {
      iot_log_warn(driver->lc, "Discovery Delete request received");
      break;
    }
    sleep (1);
  }

  driver->disc_run = true;
  iot_data_free (map1);
  iot_data_free (map2);
  iot_data_free (map3);
  iot_data_free (map4);
  devsdk_protocols_free (p1);
  devsdk_protocols_free (p2);
  devsdk_protocols_free (p3);
  devsdk_protocols_free (p4);
}

static bool template_discovery_delete (void *impl, const char *request_id)
{
  //Implement functionality to cancel a Discovery Request here
  template_driver *driver = (template_driver *) impl;
  driver->disc_run = false;

  for (int i = 0; i < 10; i++)
  {
    if (driver->disc_run)
    {
      iot_log_warn(driver->lc, "Discovery Delete request successful");
      return true;
    }
    sleep (1);
  }

  return false;
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
  const devsdk_device_t *device,
  uint32_t nreadings,
  const devsdk_commandrequest *requests,
  devsdk_commandresult *readings,
  const iot_data_t *options,
  iot_data_t **exception
)
{
  template_driver *driver = (template_driver *) impl;

  /* Access the location of the device to be accessed and log it */
  iot_log_debug(driver->lc, "GET on device:");
  dump_protocols (driver->lc, (devsdk_protocols *)device->address);

  for (uint32_t i = 0; i < nreadings; i++)
  {
    /* Log the attributes for each requested resource */
    iot_log_debug (driver->lc, "  Requested reading %u:", i);
    dump_attributes (driver->lc, requests[i].resource->attrs);
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
  const devsdk_device_t *device,
  uint32_t nvalues,
  const devsdk_commandrequest *requests,
  const iot_data_t *values[],
  const iot_data_t *options,
  iot_data_t **exception
)
{
  template_driver *driver = (template_driver *) impl;
  /* Access the location of the device to be accessed and log it */
  iot_log_debug (driver->lc, "PUT on device:");
  dump_protocols (driver->lc, (devsdk_protocols *)device->address);

  for (uint32_t i = 0; i < nvalues; i++)
  {
    /* A Device Service again makes use of the data provided to perform a PUT */
    /* Log the attributes */
    iot_log_debug (driver->lc, "  Requested device write %u:", i);
    dump_attributes (driver->lc, requests[i].resource->attrs);
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
        char * json = iot_data_to_json (values[i]);
        iot_log_debug (driver->lc, "  Value has unexpected type %s: %s", iot_data_type_name (values[i]), json);
        free (json);
    }
  }
  return true;
}

/* ---- Stop ---- */
/* Stop performs any final actions before the device service is terminated */
static void template_stop (void *impl, bool force) {}

/* ---- Attribute and Protocols --- */

static devsdk_address_t template_create_addr (void *impl, const devsdk_protocols *protocols, iot_data_t **exception)
{
  return (devsdk_address_t)protocols;
}

static void template_free_addr (void *impl, devsdk_address_t address)
{
}

static devsdk_resource_attr_t template_create_resource_attr (void *impl, const iot_data_t *attributes, iot_data_t **exception)
{
  return iot_data_copy (attributes);
}

static void template_free_resource_attr (void *impl, devsdk_resource_attr_t resource)
{
  iot_data_free ((iot_data_t *)resource);
}

int main (int argc, char *argv[])
{
  sigset_t set;
  int sigret;

  template_driver * impl = malloc (sizeof (template_driver));
  memset (impl, 0, sizeof (template_driver));

  impl->disc_run = true;
  devsdk_error e;
  e.code = 0;

  /* Device Callbacks */
  devsdk_callbacks *templateImpls = devsdk_callbacks_init
  (
    template_init,
    template_get_handler,
    template_put_handler,
    template_stop,
    template_create_addr,
    template_free_addr,
    template_create_resource_attr,
    template_free_resource_attr
  );
  devsdk_callbacks_set_discovery (templateImpls, template_discover, NULL);
  devsdk_callbacks_set_reconfiguration (templateImpls, template_reconfigure);

  devsdk_callbacks_set_discovery_delete (templateImpls, template_discovery_delete);
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
  iot_data_t *confparams = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_string_map_add (confparams, "TestParam1", iot_data_alloc_string ("X", IOT_DATA_REF));
  iot_data_string_map_add (confparams, "TestParam2", iot_data_alloc_string ("Y", IOT_DATA_REF));

  /* Start the device service*/
  devsdk_service_start (service, confparams, &e);
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
  free (templateImpls);
  iot_data_free (confparams);
  return 0;
}

