/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "service.h"
#include "device.h"
#include "discovery.h"
#include "callback.h"
#include "errorlist.h"
#include "rest_server.h"
#include "profiles.h"
#include "consul.h"
#include "metadata.h"
#include "data.h"
#include "rest.h"
#include "edgex_rest.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

#include <microhttpd.h>

#define EDGEX_DEV_API_PING "/api/v1/ping"
#define EDGEX_DEV_API_DISCOVERY "/api/v1/discovery"
#define EDGEX_DEV_API_DEVICE "/api/v1/device/"
#define EDGEX_DEV_API_CALLBACK "/api/v1/callback"

#define STRIFY(X) #X

#define POOL_THREADS 8

typedef struct postparams
{
  edgex_device_service *svc;
  const char *name;
  uint64_t origin;
  edgex_reading *readings;
} postparams;

typedef struct schedparam
{
  edgex_device_service *svc;
  const char *url;
} schedparam;

edgex_device_service *edgex_device_service_new
(
  const char *name,
  const char *version,
  void *impldata,
  edgex_device_callbacks implfns,
  edgex_error *err
)
{
  if (impldata == NULL)
  {
    iot_log_error
      (iot_log_default, "edgex_device_service_new: no implementation object");
    *err = EDGEX_NO_DEVICE_IMPL;
    return NULL;
  }
  if (name == NULL || strlen (name) == 0)
  {
    iot_log_error
      (iot_log_default, "edgex_device_service_new: no name specified");
    *err = EDGEX_NO_DEVICE_NAME;
    return NULL;
  }
  if (version == NULL || strlen (version) == 0)
  {
    iot_log_error
      (iot_log_default, "edgex_device_service_new: no version specified");
    *err = EDGEX_NO_DEVICE_VERSION;
    return NULL;
  }

  edgex_device_service *result = malloc (sizeof (edgex_device_service));
  memset (result, 0, sizeof (edgex_device_service));
  result->name = name;
  result->version = version;
  result->userdata = impldata;
  result->userfns = implfns;
  pthread_rwlockattr_t rwatt;
  pthread_rwlockattr_init (&rwatt);
#ifdef  __GLIBC_PREREQ
#if __GLIBC_PREREQ(2, 1)
  /* Avoid heavy readlock use (eg spammed "all" commands) blocking discovery */
  pthread_rwlockattr_setkind_np
    (&rwatt, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
#endif
#endif
  pthread_rwlock_init (&result->deviceslock, &rwatt);
  pthread_mutex_init (&result->discolock, NULL);
  pthread_mutex_init (&result->profileslock, NULL);
  edgex_map_init (&result->devices);
  edgex_map_init (&result->name_to_id);
  result->thpool = thpool_init (POOL_THREADS);
  result->scheduler = iot_scheduler_init (&result->thpool);
  return result;
}

static int ping_handler
(
  void *ctx,
  const char *url,
  edgex_http_method method,
  const char *upload_data,
  size_t upload_data_size,
  char **reply,
  const char **reply_type
)
{
  *reply = strdup ("{\"value\":\"pong\"}\n");
  *reply_type = "application/json";
  return MHD_HTTP_OK;
}

static void dev_invoker (void *p)
{
  int rc;
  char *reply = NULL;
  const char *reply_type;
  schedparam *param = (schedparam *) p;

  rc = edgex_device_handler_device
    (param->svc, param->url, GET, NULL, 0, &reply, &reply_type);

  if (rc != MHD_HTTP_OK)
  {
    iot_log_error
    (
      param->svc->logger,
      "Scheduled request to " STRIFY(EDGEX_DEV_API_DEVICE) "%s: HTTP %d",
      param->url, rc
    );
  }
  free (reply);
}

void edgex_device_service_start
(
  edgex_device_service *svc,
  bool useRegistry,
  const char *profile,
  const char *confDir,
  edgex_error *err
)
{
  toml_table_t *config;
  int ndevs = 0;

  *err = EDGEX_OK;
  if (confDir == NULL || *confDir == '\0')
  {
    confDir = "res";
  }

  svc->logger = iot_logging_client_create (svc->name);
  config = edgex_device_loadConfig (svc->logger, confDir, profile, err);
  if (err->code)
  {
    return;
  }
  edgex_device_populateConfig (svc, config, err);
  if (err->code)
  {
    toml_free (config);
    return;
  }

  /* TODO: if (useRegistry) get config from consul. Wait for it if need be. */

  edgex_device_validateConfig (svc, err);
  if (err->code)
  {
    toml_free (config);
    return;
  }

  if (svc->config.device.profilesdir == NULL)
  {
    svc->config.device.profilesdir = strdup (confDir);
  }

  if (svc->config.logging.file)
  {
    iot_log_addlogger
      (svc->logger, iot_log_tofile, svc->config.logging.file);
  }
  if (svc->config.logging.remoteurl)
  {
    iot_log_addlogger
      (svc->logger, edgex_log_torest, svc->config.logging.remoteurl);
  }

  iot_log_debug
  (
    svc->logger,
    "Starting %s device service, version %s",
    svc->name, svc->version
  );
  edgex_device_dumpConfig (svc);

  svc->adminstate = UNLOCKED;
  svc->opstate = ENABLED;

  /* Wait for metadata and data to be available */

  int retries = svc->config.service.connectretries;
  struct timespec delay =
  {
    .tv_sec = svc->config.service.timeout / 1000,
    .tv_nsec = 1000000 * (svc->config.service.timeout % 1000)
  };
  while (!edgex_data_client_ping (svc->logger, &svc->config.endpoints, err) &&
         --retries)
  {
    nanosleep (&delay, NULL);
  }
  if (retries == 0)
  {
    iot_log_error (svc->logger, "core-data service not running");
    *err = EDGEX_REMOTE_SERVER_DOWN;
    toml_free (config);
    return;
  }

  retries = svc->config.service.connectretries;
  while (
    !edgex_metadata_client_ping (svc->logger, &svc->config.endpoints, err) &&
    --retries)
  {
    nanosleep (&delay, NULL);
  }
  if (retries == 0)
  {
    iot_log_error (svc->logger, "core-metadata service not running");
    *err = EDGEX_REMOTE_SERVER_DOWN;
    toml_free (config);
    return;
  }

  *err = EDGEX_OK;

  /* Register device service in metadata */

  edgex_deviceservice *ds;
  ds = edgex_metadata_client_get_deviceservice
    (svc->logger, &svc->config.endpoints, svc->name, err);
  if (err->code)
  {
    iot_log_error (svc->logger, "get_deviceservice failed");
    toml_free (config);
    return;
  }

  if (ds == NULL)
  {
    uint64_t millis = time (NULL) * 1000;
    edgex_addressable *addr = edgex_metadata_client_get_addressable
      (svc->logger, &svc->config.endpoints, svc->name, err);
    if (err->code)
    {
      iot_log_error (svc->logger, "get_addressable failed");
      toml_free (config);
      return;
    }
    if (addr == NULL)
    {
      addr = malloc (sizeof (edgex_addressable));
      memset (addr, 0, sizeof (edgex_addressable));
      addr->origin = millis;
      addr->name = strdup (svc->name);
      addr->method = strdup ("POST");
      addr->protocol = strdup ("HTTP");
      addr->address = strdup (svc->config.service.host);
      addr->port = svc->config.service.port;
      addr->path = strdup (EDGEX_DEV_API_CALLBACK);

      addr->id = edgex_metadata_client_create_addressable
        (svc->logger, &svc->config.endpoints, addr, err);
      if (err->code)
      {
        iot_log_error (svc->logger, "create_addressable failed");
        toml_free (config);
        return;
      }
    }

    ds = malloc (sizeof (edgex_deviceservice));
    memset (ds, 0, sizeof (edgex_deviceservice));
    ds->addressable = addr;
    ds->name = strdup (svc->name);
    ds->operatingState = strdup ("ENABLED");
    ds->adminState = strdup ("UNLOCKED");
    ds->created = millis;
    for (int n = 0; svc->config.service.labels[n]; n++)
    {
      edgex_strings *newlabel = malloc (sizeof (edgex_strings));
      newlabel->str = strdup (svc->config.service.labels[n]);
      newlabel->next = ds->labels;
      ds->labels = newlabel;
    }

    ds->id = edgex_metadata_client_create_deviceservice (svc->logger,
                                                         &svc->config.endpoints,
                                                         ds, err);
    if (err->code)
    {
      iot_log_error
        (svc->logger, "Unable to create device service in metadata");
      toml_free (config);
      return;
    }
  }
  edgex_deviceservice_free (ds);

  /* Load DeviceProfiles from files and register in metadata */

  edgex_device_profiles_upload
    (svc->logger, svc->config.device.profilesdir, &svc->config.endpoints, err);
  if (err->code)
  {
    toml_free (config);
    return;
  }

  /* Obtain Devices from metadata */

  edgex_device *devlist =
    edgex_metadata_client_get_devices
      (svc->logger, &svc->config.endpoints, svc->name, err);
  if (err->code)
  {
    iot_log_error (svc->logger, "Unable to retrieve device list from metadata");
    toml_free (config);
    return;
  }
  edgex_device *iter = devlist;
  pthread_rwlock_wrlock (&svc->deviceslock);
  while (iter)
  {
    edgex_map_set (&svc->devices, iter->id, *iter);
    edgex_map_set (&svc->name_to_id, iter->name, iter->id);
    iter = iter->next;
    ndevs++;
  }
  pthread_rwlock_unlock (&svc->deviceslock);
  iot_log_debug
    (svc->logger, "%d devices retrieved from core-metadata", ndevs);

  pthread_mutex_lock (&svc->profileslock);
  for (iter = devlist; iter; iter = iter->next)
  {
    const char *pname = iter->profile->name;
    if (edgex_map_get (&svc->profiles, pname) == NULL)
    {
      edgex_map_set (&svc->profiles, pname, *iter->profile);
    }
  }
  pthread_mutex_unlock (&svc->profileslock);

  if (devlist)
  {
    while (devlist->next)
    {
      iter = devlist->next;
      free (devlist);
      devlist = iter;
    }
    free (devlist);
  }

  edgex_device_process_configured_devices
    (svc, toml_array_in (config, "DeviceList"), err);
  if (err->code)
  {
    toml_free (config);
    return;
  }

  /* Start REST server */

  svc->daemon = edgex_rest_server_create
    (svc->logger, svc->config.service.port, err);
  if (err->code)
  {
    toml_free (config);
    return;
  }

  edgex_rest_server_register_handler
  (
    svc->daemon, EDGEX_DEV_API_CALLBACK, PUT /* | POST | DELETE */, svc,
    edgex_device_handler_callback
  );
  edgex_rest_server_register_handler
  (
    svc->daemon, EDGEX_DEV_API_DEVICE, GET | PUT | POST, svc,
    edgex_device_handler_device
  );
  edgex_rest_server_register_handler
  (
    svc->daemon, EDGEX_DEV_API_DISCOVERY, POST, svc,
    edgex_device_handler_discovery
  );

  /* Driver configuration */

  if
  (
    !svc->userfns.init
      (svc->userdata, svc->logger, toml_table_in (config, "Driver"))
  )
  {
    *err = EDGEX_DRIVER_UNSTART;
    iot_log_error (svc->logger, "Protocol driver initialization failed");
    toml_free (config);
    return;
  }
  toml_free (config);

  /* TODO: Register device service and health check with consul (if enabled) */

  /* Start scheduled events */

  const edgex_device_scheduleeventinfo *schedevt;
  const char *key;
  uint64_t interval;
  iot_schedule sched = NULL;
  schedparam *sparam;
  edgex_map_iter i = edgex_map_iter (svc->config.scheduleevents);
  while ((key = edgex_map_next (&svc->config.scheduleevents, &i)))
  {
    schedevt = edgex_map_get (&svc->config.scheduleevents, key);
    interval = IOT_SEC_TO_NS
      (*edgex_map_get (&svc->config.schedules, schedevt->schedule));

    if (strcmp (schedevt->path, EDGEX_DEV_API_DISCOVERY) == 0)
    {
      sched = iot_schedule_create
        (svc->scheduler, edgex_device_handler_do_discovery, svc, interval, 0,
         0);
    }
    else
    {
      if (strncmp (schedevt->path, EDGEX_DEV_API_DEVICE,
                   strlen (EDGEX_DEV_API_DEVICE)) == 0)
      {
        sparam = malloc (sizeof (schedparam));
        sparam->svc = svc;
        sparam->url = schedevt->path + strlen (EDGEX_DEV_API_DEVICE);

        sched = iot_schedule_create
          (svc->scheduler, dev_invoker, sparam, interval, 0, 0);
      }
      else
      {
        iot_log_error
          (svc->logger, "Scheduled Event %s not enabled, only discovery and device commands are allowed", key);
      }
    }
    if (sched)
    {
      iot_schedule_add (svc->scheduler, sched);
    }
  }
  iot_scheduler_start (svc->scheduler);

  /* Ready. Enable ping and log that we have started */

  edgex_rest_server_register_handler
  (
    svc->daemon, EDGEX_DEV_API_PING, GET, svc, ping_handler
  );

  iot_log_debug (svc->logger, svc->config.service.openmsg);
}

static void doPost (void *p)
{
  postparams *pp = (postparams *) p;
  edgex_error err = EDGEX_OK;
  edgex_data_client_add_event
  (
    pp->svc->logger,
    &pp->svc->config.endpoints,
    pp->name,
    pp->origin,
    pp->readings,
    &err
  );
  free (pp->readings);
  free (pp);
}

void edgex_device_post_readings
(
  edgex_device_service *svc,
  const char *device_name,
  uint32_t nreadings,
  const edgex_device_commandrequest *sources,
  const edgex_device_commandresult *values
)
{
  uint64_t timenow = time (NULL) * 1000UL;
  uint64_t origin =
    (nreadings == 1 && values[0].origin) ? values[0].origin : timenow;
  edgex_reading *rdgs = malloc (nreadings * sizeof (edgex_reading));
  for (uint32_t i = 0; i < nreadings; i++)
  {
    rdgs[i].created = timenow;
    rdgs[i].modified = timenow;
    rdgs[i].pushed = timenow;
    rdgs[i].name = sources[i].devobj->name;
    rdgs[i].id = NULL;
    rdgs[i].value = edgex_value_tostring
    (
      values[i].type,
      values[i].value,
      sources[i].devobj->properties->value,
      sources[i].ro->mappings
    );
    rdgs[i].origin = values[i].origin ? values[i].origin : timenow;
    rdgs[i].next = (i == nreadings - 1) ? NULL : rdgs + i + 1;
  }
  postparams *pp = malloc (sizeof (postparams));
  pp->svc = svc;
  pp->name = device_name;
  pp->origin = origin;
  pp->readings = rdgs;
  thpool_add_work (svc->thpool, doPost, pp);
}

void edgex_device_service_stop
  (edgex_device_service *svc, bool force, edgex_error *err)
{
  iot_log_debug (svc->logger, "Stop device service");
  if (svc->scheduler)
  {
    iot_scheduler_stop (svc->scheduler);
    iot_scheduler_fini (svc->scheduler);
  }
  if (svc->daemon)
  {
    edgex_rest_server_destroy (svc->daemon);
  }
  svc->userfns.stop (svc->userdata, force);
  thpool_destroy (svc->thpool);
  iot_log_debug (svc->logger, "Stopped device service");
  edgex_device_freeConfig (svc);
  iot_logging_client_destroy (svc->logger);
  edgex_map_deinit (&svc->name_to_id);
  edgex_map_iter i = edgex_map_iter (svc->devices);
  const char *key;
  while ((key = edgex_map_next (&svc->devices, &i)))
  {
    edgex_device *e = edgex_map_get (&svc->devices, key);
    edgex_addressable_free (e->addressable);
    free (e->adminState);
    free (e->description);
    free (e->id);
    edgex_strings_free (e->labels);
    free (e->name);
    free (e->operatingState);
    edgex_deviceprofile_free (e->profile);
    edgex_deviceservice_free (e->service);
  }
  edgex_map_deinit (&svc->devices);
  edgex_map_deinit (&svc->profiles);
  free (svc);
}
