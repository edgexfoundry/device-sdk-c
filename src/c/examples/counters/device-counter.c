/* Pseudo-device service emulating counters using C SDK */

/*
 * Copyright (c) 2018-2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <unistd.h>
#include <signal.h>

#include "edgex/devsdk.h"

#define NCOUNTERS 256

#define ERR_CHECK(x) if (x.code) { fprintf (stderr, "Error: %d: %s\n", x.code, x.reason); edgex_device_service_free (service); free (impl); return x.code; }

typedef struct counter_driver
{
  iot_logger_t * lc;
  atomic_uint_fast32_t counters[NCOUNTERS];
} counter_driver;

static volatile sig_atomic_t running = true;
static void inthandler (int i)
{
  running = (i != SIGINT);
}

static const edgex_protocols *findprotocol
  (const edgex_protocols *prots, const char *name)
{
  const edgex_protocols *result = prots;
  while (result)
  {
    if (strcmp (result->name, name) == 0)
    {
      break;
    }
    result = result->next;
  }
  return result;
}

static const char *findinpairs
  (const edgex_nvpairs *nvps, const char *name)
{
  const edgex_nvpairs *pair = nvps;
  while (pair)
  {
    if (strcmp (pair->name, name) == 0)
    {
      return pair->value;
    }
    pair = pair->next;
  }
  return NULL;
}

static bool counter_init
  (void *impl, struct iot_logger_t *lc, const edgex_nvpairs *config)
{
  counter_driver *driver = (counter_driver *) impl;
  driver->lc = lc;
  for (unsigned i = 0; i < NCOUNTERS; i++)
  {
    atomic_store (&driver->counters[i], 0);
  }
  return true;
}

static bool getDeviceAddress
  (iot_logger_t *lc, unsigned long *index, const edgex_protocols *protocols)
{
  const edgex_protocols *p = findprotocol (protocols, "Counter");
  if (p == NULL)
  {
    iot_log_error (lc, "No Counter protocol in device address");
    return false;
  }

  const char *index_prop = findinpairs (p->properties, "Index");
  if (index_prop == NULL || strlen (index_prop) == 0)
  {
    iot_log_error (lc, "No Index property in Counter protocol");
    return false;
  }

  char *e; 
  unsigned long i = strtol (index_prop, &e, 0);
  if (*e != '\0' || i >= NCOUNTERS)
  {
    iot_log_error (lc, "Invalid Index: %s", index_prop);
    return false;
  }

  *index = i;
  return true;
}

static bool counter_get_handler
(
  void *impl,
  const char *devname,
  const edgex_protocols *protocols,
  uint32_t nreadings,
  const edgex_device_commandrequest *requests,
  edgex_device_commandresult *readings
)
{
  counter_driver *driver = (counter_driver *)impl;
  unsigned long index;

  if (!getDeviceAddress (driver->lc, &index, protocols))
  {
    return false;
  }

  for (uint32_t i = 0; i < nreadings; i++)
  {
    const char *reg = findinpairs (requests[i].attributes, "register");
    if (reg == NULL)
    {
      iot_log_error (driver->lc, "No register attribute in GET request");
      return false;
    }
    if (strcmp (reg, "count01") == 0)
    {
      readings[i].type = Uint32;
      readings[i].value.ui32_result = atomic_fetch_add (&driver->counters[index], 1);
    }
    /* else ifs for other registers... */
    else
    {
      iot_log_error (driver->lc, "Request for nonexistent register %s", reg);
      return false;
    }
  }
  return true;
}

static bool counter_put_handler
(
  void *impl,
  const char *devname,
  const edgex_protocols *protocols,
  uint32_t nvalues,
  const edgex_device_commandrequest *requests,
  const edgex_device_commandresult *values
)
{
  counter_driver *driver = (counter_driver *)impl;
  unsigned long index;

  if (!getDeviceAddress (driver->lc, &index, protocols))
  {
    return false;
  }

  for (uint32_t i = 0; i < nvalues; i++)
  {
    const char *reg = findinpairs (requests[i].attributes, "register");
    if (reg == NULL)
    {
      iot_log_error (driver->lc, "No register attribute in PUT request");
      return false;
    }
    if (strcmp (reg, "count01") == 0)
    {
      atomic_store (&driver->counters[index], values[i].value.ui32_result);
    }
    else
    {
      iot_log_error (driver->lc, "Request for nonexistent register %s", reg);
      return false;
    }
  }
  return true;
}

/* ---- Disconnect ---- */
/* Disconnect handles protocol-specific cleanup when a device is removed. */
static bool counter_disconnect (void *impl, edgex_protocols *device)
{
  return true;
}

/* ---- Stop ---- */
/* Stop performs any final actions before the device service is terminated */
static void counter_stop (void *impl, bool force) {}


static void usage (void)
{
  printf ("Options: \n");
  printf ("   -h, --help           : Show this text\n");
  printf ("   -n, --name=<name>    : Set the device service name\n");
  printf ("   -r, --registry=<url> : Use the registry service\n");
  printf ("   -p, --profile=<name> : Set the profile name\n");
  printf ("   -c, --confdir=<dir>  : Set the configuration directory\n");
}

static bool testArg (int argc, char *argv[], int *pos, const char *pshort, const char *plong, char **var)
{
  if (strcmp (argv[*pos], pshort) == 0 || strcmp (argv[*pos], plong) == 0)
  {
    if (*pos < argc - 1)
    {
      (*pos)++;
      *var = argv[*pos];
      (*pos)++;
      return true;
    }
    else
    {
      printf ("Option %s requires an argument\n", argv[*pos]);
      exit (0);
    }
  }
  char *eq = strchr (argv[*pos], '=');
  if (eq)
  {
    if (strncmp (argv[*pos], pshort, eq - argv[*pos]) == 0 || strncmp (argv[*pos], plong, eq - argv[*pos]) == 0)
    {
      if (strlen (++eq))
      {
        *var = eq;
        (*pos)++;
        return true;
      }
      else
      {
        printf ("Option %s requires an argument\n", argv[*pos]);
        exit (0);
      }
    }
  }
  return false;
}

int main (int argc, char *argv[])
{
  char *profile = "";
  char *confdir = "";
  char *svcname = "device-counter";
  char *regURL = getenv ("EDGEX_REGISTRY");

  counter_driver * impl = malloc (sizeof (counter_driver));
  impl->lc = NULL;

  int n = 1;
  while (n < argc)
  {
    if (strcmp (argv[n], "-h") == 0 || strcmp (argv[n], "--help") == 0)
    {
      usage ();
      return 0;
    }
    if (testArg (argc, argv, &n, "-r", "--registry", &regURL))
    {
      continue;
    }
    if (testArg (argc, argv, &n, "-n", "--name", &svcname))
    {
      continue;
    }
    if (testArg (argc, argv, &n, "-p", "--profile", &profile))
    {
      continue;
    }
    if (testArg (argc, argv, &n, "-c", "--confdir", &confdir))
    {
      continue;
    }

    printf ("Unknown option %s\n", argv[n]);
    usage ();
    return 0;
  }

  edgex_error e;
  e.code = 0;

  edgex_device_callbacks counterImpls =
  {
    counter_init,         /* Initialize */
    NULL,                 /* Discovery */
    counter_get_handler,  /* Get */
    counter_put_handler,  /* Put */
    counter_disconnect,   /* Disconnect */
    counter_stop          /* Stop */
  };

  edgex_device_service *service = edgex_device_service_new
    (svcname, "1.0", impl, counterImpls, &e);
  ERR_CHECK (e);

  edgex_device_service_start (service, regURL, profile, confdir, &e);
  ERR_CHECK (e);

  signal (SIGINT, inthandler);
  running = true;
  while (running)
  {
    sleep(1);
  }

  edgex_device_service_stop (service, true, &e);
  ERR_CHECK (e);

  edgex_device_service_free (service);
  free (impl);
  return 0;
}
