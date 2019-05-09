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
#include "metrics.h"
#include "errorlist.h"
#include "rest_server.h"
#include "profiles.h"
#include "metadata.h"
#include "data.h"
#include "rest.h"
#include "edgex_rest.h"
#include "edgex_time.h"
#include "edgex/csdk-defs.h"
#include "edgex/registry.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/utsname.h>

#include <microhttpd.h>

#define EDGEX_DEV_API_PING "/api/v1/ping"
#define EDGEX_DEV_API_DISCOVERY "/api/v1/discovery"
#define EDGEX_DEV_API_DEVICE "/api/v1/device/"
#define EDGEX_DEV_API_CALLBACK "/api/v1/callback"
#define EDGEX_DEV_API_CONFIG "/api/v1/config"
#define EDGEX_DEV_API_METRICS "/api/v1/metrics"

#define POOL_THREADS 8

typedef struct postparams
{
  edgex_device_service *svc;
  JSON_Value *jevent;
} postparams;

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
      (iot_log_default (), "edgex_device_service_new: no implementation object");
    *err = EDGEX_NO_DEVICE_IMPL;
    return NULL;
  }
  if (name == NULL || strlen (name) == 0)
  {
    iot_log_error
      (iot_log_default (), "edgex_device_service_new: no name specified");
    *err = EDGEX_NO_DEVICE_NAME;
    return NULL;
  }
  if (version == NULL || strlen (version) == 0)
  {
    iot_log_error
      (iot_log_default (), "edgex_device_service_new: no version specified");
    *err = EDGEX_NO_DEVICE_VERSION;
    return NULL;
  }

  *err = EDGEX_OK;
  edgex_device_service *result = malloc (sizeof (edgex_device_service));
  memset (result, 0, sizeof (edgex_device_service));
  result->name = name;
  result->version = version;
  result->userdata = impldata;
  result->userfns = implfns;
  result->devices = edgex_devmap_alloc (result);
  result->thpool = iot_threadpool_alloc (POOL_THREADS, 0, NULL);
  iot_threadpool_start (result->thpool);
  result->scheduler = iot_scheduler_alloc (result->thpool);
  pthread_mutex_init (&result->discolock, NULL);
  return result;
}

static int ping_handler
(
  void *ctx,
  char *url,
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

static void startConfigured
(
  edgex_device_service *svc,
  edgex_registry *registry,
  toml_table_t *config,
  const char *profile,
  edgex_error *err
)
{
  char *myhost;
  struct utsname buffer;

  if (svc->config.service.host)
  {
    myhost = svc->config.service.host;
  }
  else
  {
    uname (&buffer);
    myhost = buffer.nodename;
  }

  edgex_device_validateConfig (svc, err);
  if (err->code)
  {
    return;
  }

  if (svc->config.logging.file)
  {
    iot_logger_add
      (svc->logger, edgex_log_tofile, svc->config.logging.file);
  }
  if (svc->config.logging.remoteurl)
  {
    iot_logger_add
      (svc->logger, edgex_log_torest, svc->config.logging.remoteurl);
  }

  if (profile)
  {
    iot_log_info (svc->logger, "Uploading configuration to registry.");
    edgex_nvpairs *c = edgex_device_getConfig (svc);
    edgex_registry_put_config (registry, svc->name, profile, c, err);
    edgex_nvpairs_free (c);
    if (err->code)
    {
      iot_log_error (svc->logger, "Unable to upload config: %s", err->reason);
      return;
    }
  }

  iot_log_debug
  (
    svc->logger,
    "Starting %s device service, version %s",
    svc->name, svc->version
  );
  iot_log_debug
    (svc->logger, "EdgeX device SDK for C, version " CSDK_VERSION_STR);
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
  if (retries)
  {
    iot_log_debug (svc->logger, "Found core-data service at %s:%d", svc->config.endpoints.data.host, svc->config.endpoints.data.port);
  }
  else
  {
    iot_log_error (svc->logger, "core-data service not running");
    *err = EDGEX_REMOTE_SERVER_DOWN;
    return;
  }

  retries = svc->config.service.connectretries;
  while (
    !edgex_metadata_client_ping (svc->logger, &svc->config.endpoints, err) &&
    --retries)
  {
    nanosleep (&delay, NULL);
  }
  if (retries)
  {
    iot_log_debug (svc->logger, "Found core-metadata service at %s:%d", svc->config.endpoints.metadata.host, svc->config.endpoints.metadata.port);
  }
  else
  {
    iot_log_error (svc->logger, "core-metadata service not running");
    *err = EDGEX_REMOTE_SERVER_DOWN;
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
    return;
  }

  if (ds == NULL)
  {
    uint64_t millis = edgex_device_millitime ();
    edgex_addressable *addr = edgex_metadata_client_get_addressable
      (svc->logger, &svc->config.endpoints, svc->name, err);
    if (err->code)
    {
      iot_log_error (svc->logger, "get_addressable failed");
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
      addr->address = strdup (myhost);
      addr->port = svc->config.service.port;
      addr->path = strdup (EDGEX_DEV_API_CALLBACK);

      addr->id = edgex_metadata_client_create_addressable
        (svc->logger, &svc->config.endpoints, addr, err);
      if (err->code)
      {
        iot_log_error (svc->logger, "create_addressable failed");
        return;
      }
    }

    ds = malloc (sizeof (edgex_deviceservice));
    memset (ds, 0, sizeof (edgex_deviceservice));
    ds->addressable = addr;
    ds->name = strdup (svc->name);
    ds->operatingState = ENABLED;
    ds->adminState = UNLOCKED;
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
      return;
    }
  }
  edgex_deviceservice_free (ds);

  /* Load DeviceProfiles from files and register in metadata */

  edgex_device_profiles_upload (svc, err);
  if (err->code)
  {
    return;
  }

  /* Obtain Devices from metadata */

  edgex_device *devs = edgex_metadata_client_get_devices
    (svc->logger, &svc->config.endpoints, svc->name, err);

  if (err->code)
  {
    iot_log_error (svc->logger, "Unable to retrieve device list from metadata");
    return;
  }

  edgex_devmap_populate_devices (svc->devices, devs);
  edgex_device_free (devs);

  /* Start REST server now so that we get the callbacks on device addition */

  svc->daemon = edgex_rest_server_create
    (svc->logger, svc->config.service.port, err);
  if (err->code)
  {
    return;
  }

  edgex_rest_server_register_handler
  (
    svc->daemon, EDGEX_DEV_API_CALLBACK, PUT | POST | DELETE, svc,
    edgex_device_handler_callback
  );

  /* Add Devices from configuration */

  if (config)
  {
    edgex_device_process_configured_devices
      (svc, toml_array_in (config, "DeviceList"), err);
    if (err->code)
    {
      return;
    }
  }

  /* Driver configuration */

  if (!svc->userfns.init (svc->userdata, svc->logger, svc->config.driverconf))
  {
    *err = EDGEX_DRIVER_UNSTART;
    iot_log_error (svc->logger, "Protocol driver initialization failed");
    return;
  }

  /* Start scheduled events */

  iot_scheduler_start (svc->scheduler);

  /* Register REST handlers */

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

  edgex_rest_server_register_handler
  (
    svc->daemon, EDGEX_DEV_API_METRICS, GET, svc, edgex_device_handler_metrics
  );

  edgex_rest_server_register_handler
  (
    svc->daemon, EDGEX_DEV_API_CONFIG, GET, svc, edgex_device_handler_config
  );

  edgex_rest_server_register_handler
  (
    svc->daemon, EDGEX_DEV_API_PING, GET, svc, ping_handler
  );

  /* Ready. Register ourselves and log that we have started. */

  if (registry && svc->config.service.checkinterval)
  {
    edgex_registry_register_service
    (
      registry,
      svc->name,
      myhost,
      svc->config.service.port,
      svc->config.service.checkinterval,
      err
    );
    if (err->code)
    {
      iot_log_error (svc->logger, "Unable to register service in registry");
      return;
    }
  }

  if (svc->config.service.startupmsg)
  {
    iot_log_debug (svc->logger, svc->config.service.startupmsg);
  }
}

void edgex_device_service_start
(
  edgex_device_service *svc,
  const char *registryURL,
  const char *profile,
  const char *confDir,
  edgex_error *err
)
{
  toml_table_t *config = NULL;
  edgex_registry *registry = NULL;
  bool uploadConfig = false;
  svc->starttime = edgex_device_millitime();

  svc->logger = iot_logger_alloc (svc->name);
  if (confDir == NULL || *confDir == '\0')
  {
    confDir = "res";
  }
  *err = EDGEX_OK;

  if (registryURL)
  {
    registry = edgex_registry_get_registry (svc->logger, svc->thpool, registryURL);
    if (registry == NULL)
    {
      *err = EDGEX_INVALID_ARG;
      return;
    }
  }

  if (registry)
  {
    // Wait for registry to be ready

    int retries = 5;
    struct timespec delay = { .tv_sec = 1, .tv_nsec = 0 };
    while (!edgex_registry_ping (registry, err) && --retries)
    {
      nanosleep (&delay, NULL);
    }
    if (retries == 0)
    {
      iot_log_error (svc->logger, "registry service not running");
      *err = EDGEX_REMOTE_SERVER_DOWN;
      edgex_registry_free (registry);
      return;
    }

    iot_log_info (svc->logger, "Found registry service at %s", registryURL);
    svc->stopconfig = malloc (sizeof (atomic_bool));
    atomic_init (svc->stopconfig, false);
    edgex_nvpairs *confpairs = edgex_registry_get_config
      (registry, svc->name, profile, edgex_device_updateConf, svc, svc->stopconfig, err);

    if (confpairs)
    {
      edgex_device_populateConfigNV (svc, confpairs, err);
      edgex_nvpairs_free (confpairs);
      if (err->code)
      {
        edgex_registry_free (registry);
        return;
      }
    }
    else
    {
      iot_log_info (svc->logger, "Unable to get configuration from registry.");
      iot_log_info (svc->logger, "Will load from file.");
      uploadConfig = true;
      *err = EDGEX_OK;
    }
  }

  if (uploadConfig || (registry == NULL))
  {
    config = edgex_device_loadConfig (svc->logger, confDir, profile, err);
    if (err->code)
    {
      edgex_registry_free (registry);
      return;
    }

    edgex_device_populateConfig (svc, config, err);
  }

  if (registry)
  {
    edgex_error e; // errors will be picked up in validateConfig
    edgex_registry_query_service (registry, "edgex-core-metadata", &svc->config.endpoints.metadata.host, &svc->config.endpoints.metadata.port, &e);
    edgex_registry_query_service (registry, "edgex-core-data", &svc->config.endpoints.data.host, &svc->config.endpoints.data.port, &e);
  }

  if (svc->config.device.profilesdir == NULL)
  {
    svc->config.device.profilesdir = strdup (confDir);
  }

  startConfigured (svc, registry, config, uploadConfig ? profile : NULL, err);

  edgex_registry_free (registry);
  toml_free (config);

  if (err->code == 0)
  {
    iot_log_info (svc->logger, "Service started in: %dms", edgex_device_millitime() - svc->starttime);
    iot_log_info (svc->logger, "Listening on port: %d", svc->config.service.port);
  }
}

static void doPost (void *p)
{
  postparams *pp = (postparams *) p;
  edgex_error err = EDGEX_OK;
  edgex_data_client_add_event
  (
    pp->svc->logger,
    &pp->svc->config.endpoints,
    pp->jevent,
    &err
  );

  json_value_free (pp->jevent);
  free (pp);
}

void edgex_device_post_readings
(
  edgex_device_service *svc,
  const char *devname,
  const char *resname,
  const edgex_device_commandresult *values
)
{
  edgex_device *dev = edgex_devmap_device_byname (svc->devices, devname);
  if (dev == NULL)
  {
    iot_log_error (svc->logger, "Post readings: no such device %s", devname);
    return;
  }

  const edgex_cmdinfo *command = edgex_deviceprofile_findcommand
    (resname, dev->profile, true);
  edgex_device_release (dev);

  if (command)
  {
    JSON_Value *jevent = edgex_data_generate_event
      (devname, command, values, svc->config.device.datatransform);

    if (jevent)
    {
      postparams *pp = malloc (sizeof (postparams));
      pp->svc = svc;
      pp->jevent = jevent;
      iot_threadpool_add_work (svc->thpool, doPost, pp, NULL);
    }
  }
  else
  {
    iot_log_error (svc->logger, "Post readings: no such resource %s", resname);
  }
}

void edgex_device_service_stop
  (edgex_device_service *svc, bool force, edgex_error *err)
{
  *err = EDGEX_OK;
  iot_log_debug (svc->logger, "Stop device service");
  if (svc->stopconfig)
  {
    *svc->stopconfig = true;
  }
  if (svc->scheduler)
  {
    iot_scheduler_stop (svc->scheduler);
  }
  if (svc->daemon)
  {
    edgex_rest_server_destroy (svc->daemon);
  }
  svc->userfns.stop (svc->userdata, force);
  edgex_devmap_free (svc->devices);
  iot_threadpool_wait (svc->thpool);
  if (svc->scheduler)
  {
    iot_scheduler_free (svc->scheduler);
  }
  iot_threadpool_free (svc->thpool);
  edgex_registry_fini ();
  pthread_mutex_destroy (&svc->discolock);
  iot_log_debug (svc->logger, "Stopped device service");
  iot_logger_free (svc->logger);
  edgex_device_freeConfig (svc);
  free (svc->stopconfig);
  free (svc);
}
