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
#include "edgex_time.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

#include <microhttpd.h>

#define EDGEX_DEV_API_PING "/api/v1/ping"
#define EDGEX_DEV_API_DISCOVERY "/api/v1/discovery"
#define EDGEX_DEV_API_DEVICE "/api/v1/device/"
#define EDGEX_DEV_API_CALLBACK "/api/v1/callback"
#define ADDR_EXT "_addr"

#define POOL_THREADS 8

typedef struct postparams
{
  edgex_device_service *svc;
  char *name;
  uint64_t origin;
  uint32_t nreadings;
  edgex_reading *readings;
} postparams;

typedef struct edgex_device_service_job
{
  edgex_device_service *svc;
  char *url;
  struct edgex_device_service_job *next;
} edgex_device_service_job;

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
  result->sjobs = NULL;
  result->thpool = thpool_init (POOL_THREADS);
  result->scheduler = iot_scheduler_init (&result->thpool);
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

static void dev_invoker (void *p)
{
  int rc;
  char *reply = NULL;
  const char *reply_type;
  edgex_device_service_job *job = (edgex_device_service_job *) p;

  rc = edgex_device_handler_device
    (job->svc, job->url, GET, NULL, 0, &reply, &reply_type);

  if (rc != MHD_HTTP_OK)
  {
    iot_log_error
    (
      job->svc->logger,
      "Scheduled request to " EDGEX_DEV_API_DEVICE "%s: HTTP %d",
      job->url, rc
    );
  }
  free (reply);
}

void edgex_device_service_start
(
  edgex_device_service *svc,
  bool useRegistry,
  const char *regHost,
  uint16_t regPort,
  const char *profile,
  const char *confDir,
  edgex_error *err
)
{
  toml_table_t *config = NULL;
  bool uploadConfig = false;

  svc->logger = iot_logging_client_create (svc->name);
  if (confDir == NULL || *confDir == '\0')
  {
    confDir = "res";
  }
  *err = EDGEX_OK;

  if (useRegistry)
  {
    svc->config.endpoints.consul.host =
      strdup (regHost ? regHost : "localhost");
    svc->config.endpoints.consul.port = regPort ? regPort : 8500;

    // Wait for consul to be ready

    int retries = 5;
    struct timespec delay = { .tv_sec = 1, .tv_nsec = 0 };
    while
    (
      !edgex_consul_client_ping (svc->logger, &svc->config.endpoints, err) &&
      --retries
    )
    {
      nanosleep (&delay, NULL);
    }
    if (retries == 0)
    {
      iot_log_error (svc->logger, "consul registry service not running");
      *err = EDGEX_REMOTE_SERVER_DOWN;
      return;
    }

    edgex_nvpairs *consulConf = edgex_consul_client_get_config
    (
      svc->logger,
      &svc->config.endpoints,
      svc->name,
      profile,
      err
    );

    if (consulConf)
    {
      edgex_device_populateConfigNV (svc, consulConf, err);
      edgex_nvpairs_free (consulConf);
      if (err->code)
      {
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

  if (uploadConfig || !useRegistry)
  {
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
  }

  if (svc->config.device.profilesdir == NULL)
  {
    svc->config.device.profilesdir = strdup (confDir);
  }

  edgex_device_validateConfig (svc, err);
  if (err->code)
  {
    toml_free (config);
    return;
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

  if (uploadConfig)
  {
    iot_log_info (svc->logger, "Uploading configuration to registry.");
    edgex_nvpairs *c = edgex_device_getConfig (svc);
    edgex_consul_client_write_config
      (svc->logger, &svc->config.endpoints, svc->name, profile, c, err);
    edgex_nvpairs_free (c);
    if (err->code)
    {
      iot_log_error (svc->logger, "Unable to upload config: %s", err->reason);
      toml_free (config);
      return;
    }
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
    uint64_t millis = edgex_device_millitime ();
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

  edgex_device_profiles_upload (svc, err);
  if (err->code)
  {
    toml_free (config);
    return;
  }

  /* Obtain Devices from metadata */

  edgex_device_free (edgex_device_devices (svc, err));
  if (err->code)
  {
    toml_free (config);
    return;
  }

  /* Obtain Devices from configuration */

  if (config)
  {
    edgex_device_process_configured_devices
      (svc, toml_array_in (config, "DeviceList"), err);
    toml_free (config);
    if (err->code)
    {
      return;
    }
  }

  /* Start REST server */

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

  if (!svc->userfns.init (svc->userdata, svc->logger, svc->config.driverconf))
  {
    *err = EDGEX_DRIVER_UNSTART;
    iot_log_error (svc->logger, "Protocol driver initialization failed");
    return;
  }

  /* Upload Schedules and ScheduleEvents */

  const char *key;
  edgex_map_iter i = edgex_map_iter (svc->config.schedules);

  while ((key = edgex_map_next (&svc->config.schedules, &i)))
  {
    *err = EDGEX_OK;
    edgex_schedule_free (edgex_metadata_client_create_schedule
    (
      svc->logger,
      &svc->config.endpoints,
      key,
      0,
      *edgex_map_get (&svc->config.schedules, key),
      "",
      "",
      false,
      err
    ));
    if (err->code == 0)
    {
      iot_log_info (svc->logger, "Created schedule %s", key);
    }
    else if (err->code == EDGEX_HTTP_CONFLICT.code)
    {
      iot_log_info (svc->logger, "Skipping already existing schedule %s", key);
    }
    else
    {
      iot_log_error (svc->logger, "Unable to create schedule %s", key);
      return;
    }
  }

  i = edgex_map_iter (svc->config.scheduleevents);
  while ((key = edgex_map_next (&svc->config.scheduleevents, &i)))
  {
    edgex_device_scheduleeventinfo *schedevt =
      edgex_map_get (&svc->config.scheduleevents, key);
    if
    (
      strcmp (schedevt->path, EDGEX_DEV_API_DISCOVERY) &&
      strncmp (schedevt->path, EDGEX_DEV_API_DEVICE,
        strlen (EDGEX_DEV_API_DEVICE))
    )
    {
      iot_log_error
        (svc->logger, "Scheduled Event %s not valid, only discovery and device commands are allowed", key);
      *err = EDGEX_BAD_CONFIG;
      return;
    }

    *err = EDGEX_OK;
    edgex_addressable add;
    char *addr_name = malloc (strlen (key) + strlen (ADDR_EXT) + 1);
    strcpy (addr_name, key);
    strcat (addr_name, ADDR_EXT);
    memset (&add, 0, sizeof (edgex_addressable));
    add.name = addr_name;
    add.address = svc->config.service.host;
    add.method = "GET";
    add.path = schedevt->path;
    add.port = svc->config.service.port;
    add.protocol = "HTTP";
    free
      (edgex_metadata_client_create_addressable
        (svc->logger, &svc->config.endpoints, &add, err));
    if (err->code == 0)
    {
      iot_log_info (svc->logger, "Created addressable %s", addr_name);
    }
    else if (err->code == EDGEX_HTTP_CONFLICT.code)
    {
      iot_log_info
        (svc->logger, "Skipping already existing addressable %s", addr_name);
    }
    else
    {
      iot_log_error (svc->logger, "Unable to create addressable %s", addr_name);
      free (addr_name);
      return;
    }

    *err = EDGEX_OK;
    edgex_scheduleevent_free (edgex_metadata_client_create_scheduleevent 
    (
      svc->logger,
      &svc->config.endpoints,
      key,
      0,
      schedevt->schedule,
      addr_name,
      "",
      svc->name,
      err
    ));
    free (addr_name);
    if (err->code == 0)
    {
      iot_log_info (svc->logger, "Created ScheduleEvent %s", key);
    }
    else if (err->code == EDGEX_HTTP_CONFLICT.code)
    {
      iot_log_info
        (svc->logger, "Skipping already existing ScheduleEvent %s", key);
    }
    else
    {
      iot_log_error (svc->logger, "Unable to create ScheduleEvent %s", key);
      return;
    }
  }

  /* Retrieve */

  int interval;
  iot_schedule sched = NULL;
  edgex_device_service_job *job;
  *err = EDGEX_OK;
  edgex_scheduleevent *events = edgex_metadata_client_get_scheduleevents
    (svc->logger, &svc->config.endpoints, svc->name, err);
  if (err->code)
  {
    iot_log_error
      (svc->logger, "Unable to obtain ScheduleEvents from metadata");
    return;
  }

  while (events)
  {
    sched = NULL;
    edgex_schedule *schedule = edgex_metadata_client_get_schedule
    (
      svc->logger,
      &svc->config.endpoints,
      events->schedule,
      err
    );
    if (err->code)
    {
      iot_log_error
      (
        svc->logger,
        "Unable to obtain Schedule %s from metadata",
        events->schedule
      );
      return;
    }
    const char *estr =
      edgex_device_config_parse8601 (schedule->frequency, &interval);
    edgex_schedule_free (schedule);
    if (estr)
    {
      iot_log_error (svc->logger, "Unable to parse frequency for schedule %s, %s", events->schedule, estr);
      *err = EDGEX_BAD_CONFIG;
      return;
    }

    if (strcmp (events->addressable->path, EDGEX_DEV_API_DISCOVERY) == 0)
    {
      sched = iot_schedule_create
      (
        svc->scheduler,
        edgex_device_handler_do_discovery,
        svc,
        IOT_SEC_TO_NS (interval),
        0,
        0
      );
    }
    else if (strncmp (events->addressable->path, EDGEX_DEV_API_DEVICE,
                      strlen (EDGEX_DEV_API_DEVICE)) == 0)
    {
      job = malloc (sizeof (edgex_device_service_job));
      job->svc = svc;
      job->url =
        strdup (events->addressable->path + strlen (EDGEX_DEV_API_DEVICE));
      job->next = svc->sjobs;
      svc->sjobs = job;
      sched = iot_schedule_create
        (svc->scheduler, dev_invoker, job, IOT_SEC_TO_NS (interval), 0, 0);
    }
    else
    {
      iot_log_error (svc->logger, "Scheduled Event %s is invalid, only discovery and device commands are allowed", key);
      *err = EDGEX_BAD_CONFIG;
      return;
    }

    iot_schedule_add (svc->scheduler, sched);

    edgex_scheduleevent *tmp = events->next;
    edgex_scheduleevent_free (events);
    events = tmp;
  }

  /* Start scheduled events */

  iot_scheduler_start (svc->scheduler);

  /* Ready. Enable ping and log that we have started */

  edgex_rest_server_register_handler
  (
    svc->daemon, EDGEX_DEV_API_PING, GET, svc, ping_handler
  );

  if (useRegistry && svc->config.service.checkinterval)
  {
    edgex_consul_client_register_service
    (
      svc->logger,
      &svc->config.endpoints,
      svc->name,
      svc->config.service.host,
      svc->config.service.port,
      svc->config.service.checkinterval,
      err
    );
    if (err->code)
    {
      iot_log_error (svc->logger, "Unable to register service in consul");
      return;
    }
  }

  if (svc->config.service.startupmsg)
  {
    iot_log_debug (svc->logger, svc->config.service.startupmsg);
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
    pp->name,
    pp->origin,
    pp->readings,
    &err
  );

  for (uint32_t i = 0; i < pp->nreadings; i++)
  {
    free (pp->readings[i].value);
  }
  free (pp->readings);
  free (pp->name);
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
  uint64_t timenow = edgex_device_millitime ();
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
      svc->config.device.datatransform,
      sources[i].devobj->properties->value,
      sources[i].ro->mappings
    );
    rdgs[i].origin = values[i].origin;
    rdgs[i].next = (i == nreadings - 1) ? NULL : rdgs + i + 1;
  }
  postparams *pp = malloc (sizeof (postparams));
  pp->svc = svc;
  pp->name = strdup (device_name);
  pp->origin = timenow;
  pp->readings = rdgs;
  pp->nreadings = nreadings;
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
  edgex_device_service_job *j;
  while (svc->sjobs)
  {
    j = svc->sjobs->next;
    free (svc->sjobs->url);
    free (svc->sjobs);
    svc->sjobs = j;
  }
  edgex_device_freeConfig (svc);
  iot_logging_client_destroy (svc->logger);
  edgex_map_deinit (&svc->name_to_id);
  edgex_map_iter i = edgex_map_iter (svc->devices);
  const char *key;
  while ((key = edgex_map_next (&svc->devices, &i)))
  {
    edgex_device **e = edgex_map_get (&svc->devices, key);
    edgex_device_free (*e);
  }
  edgex_map_deinit (&svc->devices);
  i = edgex_map_iter (svc->profiles);
  while ((key = edgex_map_next (&svc->profiles, &i)))
  {
    edgex_deviceprofile **p = edgex_map_get (&svc->profiles, key);
    edgex_deviceprofile_free (*p);
  }
  edgex_map_deinit (&svc->profiles);
  free (svc);
}
