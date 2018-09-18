/* Simple example implementation of an Edgex device service using C SDK */

/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "edgex/devsdk.h"

#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

#define ERR_CHECK(x) if (x.code) { fprintf (stderr, "Error: %d: %s\n", x.code, x.reason); return x.code; }

typedef struct simpledriver
{
  iot_logging_client *lc;
  edgex_device_service *svc;
} simpledriver;

static bool myInit
  (void *, iot_logging_client *, const toml_table_t *);

static void myDiscover (void *);

static bool myGetHandler
(
  void *,
  const edgex_addressable *,
  uint32_t,
  const edgex_device_commandrequest *,
  edgex_device_commandresult *
);

static bool myPutHandler
(
  void *,
  const edgex_addressable *,
  uint32_t,
  const edgex_device_commandrequest *,
  const edgex_device_commandresult *
);

static bool myDisconnect (void *, edgex_addressable *);

static void myStop (void *, bool);

static void inthandler (int i)
{
}

/* Initialize performs protocol-specific initialization for the device
 * service.
 */

bool myInit (void *impl, iot_logging_client *lc, const toml_table_t *config)
{
  ((simpledriver *) impl)->lc = lc;
  iot_log_debug (lc, "driver initialized");
  return true;
}

/* Discover triggers protocol specific device discovery, which is
   a synchronous operation which adds any new devices
   which may be added to the device service based on service
   configuration. This function may also optionally trigger sensor
   discovery, which could result in dynamic device profile creation.
*/

void myDiscover (void *impl)
{
  edgex_error err;
  simpledriver *mydata = (simpledriver *) impl;
  iot_log_debug (mydata->lc, "driver:discover called");

  edgex_addressable addr;
  memset (&addr, 0, sizeof (addr));
  addr.address = "modbusgw02";
  addr.port = 502;
  addr.protocol = "OTHER";
  edgex_device_add_device (mydata->svc, "dev02", "My discovered device", NULL, "Proximity Sensor", &addr, &err);
}


/* HandleOperation triggers an asynchronous protocol specific GET or SET
   operation for the specified device. Device profile attributes are passed as
   part of the *models.DeviceObject. The parameter 'value' must be provided for
   a SET operation, otherwise it should be 'nil'.

   Note - DeviceObject represents a deviceResource defined in deviceprofile.
*/

bool myGetHandler
(
  void *impl,
  const edgex_addressable *devaddr,
  uint32_t nresults,
  const edgex_device_commandrequest *requests,
  edgex_device_commandresult *readings
)
{
  simpledriver *sd = (simpledriver *) impl;

  iot_log_debug
    (sd->lc, "Implementation for GET, address is %s", devaddr->address);

  for (uint32_t i = 0; i < nresults; i++)
  {
    iot_log_debug
      (sd->lc, "Implementation for GET, op is %s", requests[i].devobj->name);
    readings[i].origin = time (NULL);
    readings[i].type = Float32;
    readings[i].value.f32_result = random() % 10000 / 100.0;
  }
  return true;
}

bool myPutHandler
(
  void *impl,
  const edgex_addressable *devaddr,
  uint32_t nrequests,
  const edgex_device_commandrequest *requests,
  const edgex_device_commandresult *values
)
{
  simpledriver *sd = (simpledriver *) impl;
  for (uint32_t i = 0; i < nrequests; i++)
  {
    iot_log_debug (sd->lc, "PUT Command handler: path=%s, op=%s", devaddr->path,
                   requests[i].devobj->name);
  }
  return true;
}

/* DisconnectDevice handles protocol-specific cleanup when a device is removed.
*/

bool myDisconnect (void *impl, edgex_addressable *device)
{
  return true;
}

void myStop (void *impl, bool force)
{
}

static void usage (void);

void usage ()
{
  printf ("Options: \n");
  printf ("   -h, --help           : Show this text\n");
  printf ("   -r, --registry       : Use the registry service\n");
  printf ("   -p, --profile <name> : Set the profile name\n");
  printf ("   -c, --confdir <dir>  : Set the configuration directory\n");
}

int main (int argc, char *argv[])
{
  bool useRegistry = false;
  char *profile = "";
  char *confdir = "";
  simpledriver *implObject = malloc (sizeof (simpledriver));
  implObject->lc = iot_log_default;

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
      useRegistry = true;
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

  edgex_device_callbacks myImpls =
    {myInit, myDiscover, myGetHandler, myPutHandler, myDisconnect, myStop};

  edgex_device_service *service =
    edgex_device_service_new ("device-simple", "1.0", implObject, myImpls, &e);
  ERR_CHECK(e);

  implObject->svc = service;

  edgex_device_service_start (service, useRegistry, profile, confdir, &e);
  ERR_CHECK(e);

  printf ("Known device profiles after initialization:\n");
  edgex_deviceprofile *profiles;
  uint32_t nprofiles;
  edgex_device_service_getprofiles (service, &nprofiles, &profiles);
  for (uint32_t i = 0; i < nprofiles; i++)
  {
    printf ("%s\n", profiles[i].name);
  }
  free (profiles);

  printf ("\nRunning - press ctrl-c to exit\n");
  signal (SIGINT, inthandler);
  while (sleep (10) == 0)
  {}

  edgex_device_service_stop (service, true, &e);
  ERR_CHECK(e);

  free (implObject);
  return 0;
}
