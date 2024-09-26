/*
 * Copyright (c) 2018-2024
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "service.h"
#include "api.h"
#include "edgex-logging.h"
#include "bus.h"
#include "device.h"
#include "discovery.h"
#include "callback3.h"
#include "validate.h"
#include "errorlist.h"
#include "rest-server.h"
#include "profiles.h"
#include "metadata.h"
#include "data.h"
#include "rest.h"
#include "edgex-rest.h"
#include "iot/time.h"
#include "iot/iot.h"
#include "filesys.h"
#include "devutil.h"
#include "correlation.h"
#include "edgex/csdk-defs.h"
#include "request_auth.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

#include <microhttpd.h>

#define SECUREENV "EDGEX_SECURITY_SECRET_STORE"

#define POOL_THREADS 8
#define PING_RETRIES 10
#define ERRBUFSZ 1024

void devsdk_usage ()
{
  printf ("  -cp, --configProvider=<url>\tIndicates to use Configuration Provider service at specified URL.\n"
          "                             \tURL Format: {type}.{protocol}://{host}:{port} ex: consul.http://localhost:8500\n");
  printf ("  -cc, --commonConfig        \tTakes the location where the common configuration is loaded from when not using the Configuration Provider\n");
  printf ("  -o,  --overwrite            \tOverwrite configuration in provider with local configuration.\n"
          "                             \t*** Use with cation *** Use will clobber existing settings in provider,\n"
          "                             \tproblematic if those settings were edited by hand intentionally\n");
  printf ("  -cf, --configFile          \tIndicates name of the local configuration file. Defaults to configuration.yaml\n");
  printf ("  -p,  --profile=<name>       \tIndicate configuration profile other than default.\n");
  printf ("  -cd, --configDir=<dir>     \tSpecify local configuration directory\n");
  printf ("  -r,  --registry             \tIndicates service should use Registry.\n");
  printf ("  -i,  --instance=<name>      \tSpecify device service instance name (if specified this is appended to the device service name).\n");
}

static bool testArg (const char *arg, const char *val, const char *pshort, const char *plong, const char **var, bool *result)
{
  if (strcmp (arg, pshort) == 0 || strcmp (arg, plong) == 0)
  {
    if (val && *val)
    {
      *var = val;
    }
    else
    {
      printf ("Option \"%s\" requires a parameter\n", arg);
      *result = false;
    }
    return true;
  }
  else
  {
    return false;
  }
}

static bool testBool (const char *arg, const char *val, const char *pshort, const char *plong, bool *var, bool *result)
{
  if (strcmp (arg, pshort) == 0 || strcmp (arg, plong) == 0)
  {
    *var = true;
    return true;
  }
  return false;
}

static void consumeArgs (int *argc_p, char **argv, int start, int nargs)
{
  for (int n = start + nargs; n < *argc_p; n++)
  {
    argv[n - nargs] = argv[n];
  }
  *argc_p -= nargs;
}

static void checkEnv (const char **setting, const char *varname)
{
  const char *val = getenv (varname);
  if (val)
  {
    *setting = val;
  }
}

static void checkEnvBool (bool *setting, const char *varname)
{
  const char *val = getenv (varname);
  if (val)
  {
    if (strcmp (val, "true") == 0)
    {
      *setting = true;
    }
    else if (strcmp (val, "false") == 0)
    {
      *setting = false;
    }
  }
}

static bool processCmdLine (int *argc_p, char **argv, devsdk_service_t *svc)
{
  bool result = true;
  char *eq;
  const char *arg;
  const char *val;
  int argc = *argc_p;
  bool usereg = false;

  int n = 1;
  while (result && n < argc)
  {
    arg = argv[n];
    val = NULL;
    eq = strchr (arg, '=');
    if (eq)
    {
      *eq = '\0';
      val = eq + 1;
    }
    else if (n + 1 < argc)
    {
      val = argv[n + 1];
    }
    if
    (
      testArg (arg, val, "-cp", "--configProvider", &svc->regURL, &result) ||
      testArg (arg, val, "-cc", "--commonConfig", &svc->commonconffile, &result) ||
      testArg (arg, val, "-i", "--instance", (const char **)&svc->name, &result) ||
      testArg (arg, val, "-p", "--profile", &svc->profile, &result) ||
      testArg (arg, val, "-cd", "--configDir", &svc->confdir, &result) ||
      testArg (arg, val, "-cf", "--configFile", &svc->conffile, &result)
    )
    {
      consumeArgs (&argc, argv, n, eq ? 1 : 2);
    }
    else if
    (
      testBool (arg, val, "-o", "--overwrite", &svc->overwriteconfig, &result) ||
      testBool (arg, val, "-r", "--registry", &usereg, &result)
    )
    {
      consumeArgs (&argc, argv, n, 1);
    }
    else
    {
      n++;
    }
    if (eq)
    {
      *eq = '=';
    }
  }
  *argc_p = argc;

  checkEnv (&svc->regURL, "EDGEX_CONFIG_PROVIDER");
  checkEnv (&svc->commonconffile, "EDGEX_COMMON_CONFIG");
  checkEnv (&svc->profile, "EDGEX_PROFILE");
  checkEnv (&svc->confdir, "EDGEX_CONFIG_DIR");
  checkEnv (&svc->conffile, "EDGEX_CONFIG_FILE");
  checkEnv ((const char **)&svc->name, "EDGEX_INSTANCE_NAME");
  checkEnvBool (&usereg, "EDGEX_USE_REGISTRY");

  if (usereg)
  {
    if (svc->regURL == NULL)
    {
      svc->regURL = "";
    }
  }
  else
  {
    if (svc->regURL)
    {
      iot_log_warn (svc->logger, "Configuration provider was specified but registry not enabled");
      svc->regURL = NULL;
    }
  }
  return result;
}

static char *devsdk_service_confpath (const char *dir, const char *fname, const char *profile)
{
  int pathlen;
  char *result;

  if (fname && *fname)
  {
    pathlen = strlen (dir) + 1 + strlen (fname) + 1;
  }
  else
  {
    pathlen = strlen (dir) + 1 + strlen ("configuration.yaml") + 1;
    if (profile && *profile)
    {
      pathlen += (strlen (profile) + 1);
    }
  }
  result = malloc (pathlen);
  strcpy (result, dir);
  strcat (result, "/");
  if (fname && *fname)
  {
    strcat (result, fname);
  }
  else
  {
    strcat (result, "configuration");
    if (profile && *profile)
    {
      strcat (result, "-");
      strcat (result, profile);
    }
    strcat (result, ".yaml");
  }
  return result;
}

devsdk_service_t *devsdk_service_new
  (const char *defaultname, const char *version, void *impldata, devsdk_callbacks *implfns, int *argc, char **argv, devsdk_error *err)
{
  iot_loglevel_t ll = IOT_LOG_INFO;
  const char *llstr = getenv ("WRITABLE_LOGLEVEL");
  if (llstr)
  {
    edgex_logger_nametolevel (llstr, &ll);
  }
  iot_logger_t *logger = iot_logger_alloc_custom (defaultname, ll, true, NULL, edgex_log_tostdout, (void *)defaultname, NULL);
  if (impldata == NULL)
  {
    iot_log_error (logger, "devsdk_service_new: no implementation object");
    *err = EDGEX_NO_DEVICE_IMPL;
    return NULL;
  }
  if (defaultname == NULL || strlen (defaultname) == 0)
  {
    iot_log_error (logger, "devsdk_service_new: no default name specified");
    *err = EDGEX_NO_DEVICE_NAME;
    return NULL;
  }
  if (version == NULL || strlen (version) == 0)
  {
    iot_log_error (logger, "devsdk_service_new: no version specified");
    *err = EDGEX_NO_DEVICE_VERSION;
    return NULL;
  }

  *err = EDGEX_OK;
  devsdk_service_t *result = malloc (sizeof (devsdk_service_t));
  memset (result, 0, sizeof (devsdk_service_t));
  result->logger = logger;
  result->config.loglevel = ll;

  if (!processCmdLine (argc, argv, result))
  {
    *err = EDGEX_INVALID_ARG;
    return NULL;
  }

  if (result->name)
  {
    const char *n = result->name;
    int l = strlen (n) + strlen (defaultname) + 2;
    result->name = malloc (l);
    strcpy (result->name, defaultname);
    strcat (result->name, "_");
    strcat (result->name, n);
    iot_logger_free (result->logger);
    result->logger = iot_logger_alloc_custom (result->name, ll, true, NULL, edgex_log_tostdout, result->name, NULL);
  }
  else
  {
    result->name = strdup (defaultname);
  }
  if (result->confdir == NULL)
  {
    result->confdir = "res";
  }
  result->confpath = devsdk_service_confpath (result->confdir, result->conffile, result->profile);
  result->version = version;
  result->userdata = impldata;
  result->userfns = *implfns;
  result->devices = edgex_devmap_alloc (result);
  result->watchlist = edgex_watchlist_alloc ();
  result->thpool = iot_threadpool_alloc (POOL_THREADS, 0, -1, -1, result->logger);
  result->scheduler = iot_scheduler_alloc (-1, -1, result->logger);
  result->discovery = edgex_device_periodic_discovery_alloc (result->logger, result->scheduler, result->thpool, implfns->discover, implfns->discovery_delete, impldata);
  atomic_store (&result->metrics.esent, 0);
  atomic_store (&result->metrics.rsent, 0);
  atomic_store (&result->metrics.rcexe, 0);
  atomic_store (&result->metrics.secrq, 0);
  atomic_store (&result->metrics.secsto, 0);
  return result;
}

static void ping2_handler (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;
  edgex_pingresponse pr;

  edgex_baseresponse_populate ((edgex_baseresponse *)&pr, EDGEX_API_VERSION, MHD_HTTP_OK, NULL);
  pr.timestamp = iot_time_secs ();
  pr.svcname = svc->name;
  edgex_pingresponse_write (&pr, reply);
}

static void version_handler (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;
  JSON_Value *val = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (val);
  json_object_set_string (obj, "version", svc->version);
  json_object_set_string (obj, "sdk_version", CSDK_VERSION_STR);
  json_object_set_string (obj, "serviceName", svc->name);
  char *json = json_serialize_to_string (val);
  json_value_free (val);
  reply->data.bytes = json;
  reply->data.size = strlen (json);
  reply->content_type = CONTENT_JSON;
  reply->code = MHD_HTTP_OK;
}

extern void devsdk_publish_system_event (devsdk_service_t *svc, const char *action, iot_data_t * details)
{
  iot_data_t *event;

  event = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_string_map_add (event, "type", iot_data_alloc_string ("device", IOT_DATA_REF));
  iot_data_string_map_add (event, "action", iot_data_alloc_string (action, IOT_DATA_REF));
  iot_data_string_map_add (event, "source", iot_data_alloc_string (svc->name, IOT_DATA_REF));
  iot_data_string_map_add (event, "owner", iot_data_alloc_string (svc->name, IOT_DATA_REF));
  iot_data_string_map_add (event, "details", details);
  iot_data_string_map_add (event, "timestamp", iot_data_alloc_ui64 (iot_time_nsecs ()));

  char *t = malloc (strlen (action) + sizeof ("device/"));
  strcpy (t, "device/");
  strcat (t, action);
  char *topic = edgex_bus_mktopic (svc->msgbus, EDGEX_DEV_TOPIC_SYSTEM_EVENT, t);
  edgex_bus_post (svc->msgbus, topic, event);
  free (t);
  free (topic);
}

extern void devsdk_publish_discovery_event (devsdk_service_t *svc, const char * request_id,  const int8_t progress, const uint64_t discovered_devices)
{
  iot_data_t * details = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_string_map_add (details, "progress", iot_data_alloc_i8 (progress));
  if(discovered_devices) iot_data_string_map_add (details, "discoveredDeviceCount", iot_data_alloc_ui64 (discovered_devices));
  iot_data_string_map_add (details, "requestId", iot_data_alloc_string (request_id, IOT_DATA_REF));

  iot_data_t *event = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_string_map_add (event, "type", iot_data_alloc_string ("device", IOT_DATA_REF));
  iot_data_string_map_add (event, "action", iot_data_alloc_string ("discovery", IOT_DATA_REF));
  iot_data_string_map_add (event, "source", iot_data_alloc_string (svc->name, IOT_DATA_REF));
  iot_data_string_map_add (event, "owner", iot_data_alloc_string (svc->name, IOT_DATA_REF));
  iot_data_string_map_add (event, "details", details);
  iot_data_string_map_add (event, "timestamp", iot_data_alloc_ui64 (iot_time_nsecs ()));

  char *topic = edgex_bus_mktopic (svc->msgbus, EDGEX_DEV_TOPIC_SYSTEM_EVENT, "device/discovery");
  edgex_bus_post (svc->msgbus, topic, event);
  free (topic);

  iot_data_free (event);
}

static void devsdk_publish_metric (devsdk_service_t *svc, const char *mname, uint64_t val)
{
  iot_data_t *field;
  iot_data_t *fields;
  iot_data_t *metric;

  field = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_string_map_add (field, "name", iot_data_alloc_string ("counter-count", IOT_DATA_REF));
  iot_data_string_map_add (field, "value", iot_data_alloc_ui64 (val));
  fields = iot_data_alloc_vector (1);
  iot_data_vector_add (fields, 0, field);
  metric = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_string_map_add (metric, "apiVersion", iot_data_alloc_string (EDGEX_API_VERSION, IOT_DATA_REF));
  iot_data_string_map_add (metric, "name", iot_data_alloc_string (mname, IOT_DATA_REF));
  iot_data_string_map_add (metric, "fields", fields);
  iot_data_string_map_add (metric, "timestamp", iot_data_alloc_ui64 (iot_time_nsecs ()));

  char *topic = edgex_bus_mktopic (svc->msgbus, EDGEX_DEV_TOPIC_METRIC, mname);
  edgex_bus_post (svc->msgbus, topic, metric);
  free (topic);

  iot_data_free (metric);
}

static void *devsdk_run_metrics (void *p)
{
  devsdk_service_t *svc = (devsdk_service_t *)p;
  iot_log_debug (svc->logger, "Publishing metrics");
  edgex_device_alloc_crlid (NULL);
  if (svc->config.metrics.flags & EX_METRIC_EVSENT) devsdk_publish_metric (svc, "EventsSent", atomic_load (&svc->metrics.esent));
  if (svc->config.metrics.flags & EX_METRIC_RDGSENT) devsdk_publish_metric (svc, "ReadingsSent", atomic_load (&svc->metrics.rsent));
  if (svc->config.metrics.flags & EX_METRIC_RDCMDS) devsdk_publish_metric (svc, "ReadCommandsExecuted", atomic_load (&svc->metrics.rcexe));
  if (svc->config.metrics.flags & EX_METRIC_SECREQ) devsdk_publish_metric (svc, "SecuritySecretsRequested", atomic_load (&svc->metrics.secrq));
  if (svc->config.metrics.flags & EX_METRIC_SECSTO) devsdk_publish_metric (svc, "SecuritySecretsStored", atomic_load (&svc->metrics.secsto));
  edgex_device_free_crlid ();

  return NULL;
}

void devsdk_schedule_metrics (devsdk_service_t *svc)
{
  uint64_t interval = edgex_parsetime (svc->config.metrics.interval);
  if (svc->metricschedule)
  {
    iot_schedule_delete (svc->scheduler, svc->metricschedule);
    svc->metricschedule = NULL;
  }
  if (interval)
  {
    svc->metricschedule = iot_schedule_create (svc->scheduler, devsdk_run_metrics, NULL, svc, IOT_MS_TO_NS (interval), 0, 0, svc->thpool, -1);
    iot_schedule_add (svc->scheduler, svc->metricschedule);
  }
}

static void devsdk_get_deadline (devsdk_timeout *result, uint64_t starttime)
{
  unsigned long duration = devsdk_strtoul_dfl (getenv ("EDGEX_STARTUP_DURATION"), 60);
  unsigned long interval = devsdk_strtoul_dfl (getenv ("EDGEX_STARTUP_INTERVAL"), 1);
  result->deadline = starttime + 1000 * duration;
  result->interval = 1000 * interval;
}

static bool ping_client (iot_logger_t *lc, const char *sname, edgex_device_service_endpoint *ep, const devsdk_timeout *timeout, devsdk_error *err)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  uint64_t t1, t2;
  bool result;

  if (ep->host == NULL || ep->port == 0)
  {
    iot_log_error (lc, "Missing endpoint for %s service.", sname);
    *err = EDGEX_BAD_CONFIG;
    return false;
  }

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/v3/ping", ep->host, ep->port);

  while (true)
  {
    t1 = iot_time_msecs ();
    memset (&ctx, 0, sizeof (edgex_ctx));
    edgex_http_get (lc, &ctx, url, NULL, err);
    if (err->code == 0)
    {
      result = true;
      break;
    }
    t2 = iot_time_msecs ();
    if (t2 > timeout->deadline - timeout->interval)
    {
      result = false;
      break;
    }
    if (timeout->interval > t2 - t1)
    {
      iot_wait_msecs (timeout->interval - (t2 - t1));
    }
  }

  if (result)
  {
    iot_log_info (lc, "Found %s service at %s:%d", sname, ep->host, ep->port);
  }
  else
  {
    iot_log_error (lc, "Can't connect to %s service at %s:%d", sname, ep->host, ep->port);
    *err = EDGEX_REMOTE_SERVER_DOWN;
  }
  return result;
}

static void edgex_device_device_upload_obj (devsdk_service_t *svc, JSON_Object *jobj, devsdk_error *err)
{
  const char *dname = json_object_get_string (jobj, "name");
  if (dname)
  {
    if (!edgex_devmap_device_exists (svc->devices, dname))
    {
      if (json_object_get_string (jobj, "profileName"))
      {
        JSON_Value *jval = json_value_deep_copy (json_object_get_wrapping_value (jobj));
        JSON_Object *deviceobj = json_value_get_object (jval);
        json_object_set_string (deviceobj, "serviceName", svc->name);
        edgex_metadata_client_add_device_jobj (svc->logger, &svc->config.endpoints, svc->secretstore, deviceobj, err);
      }
      else
      {
        iot_log_warn (svc->logger, "Device upload: Missing device profileName definition");
      }
    }
    else
    {
      iot_log_info (svc->logger, "Device %s already exists: skipped", dname);
    }
  }
  else
  {
    iot_log_warn (svc->logger, "Device upload: Missing device name definition");
  }
}

static void edgex_device_devices_upload (devsdk_service_t *svc, devsdk_error *err)
{
  devsdk_strings *filenames = devsdk_scandir (svc->logger, svc->config.device.devicesdir, "json");
  iot_log_info (svc->logger, "Processing Devices from %s", svc->config.device.devicesdir);
  for (devsdk_strings *f = filenames; f; f = f->next)
  {
    JSON_Value *jval = json_parse_file (f->str);
    if (jval)
    {
      JSON_Array *jarr = json_value_get_array (jval);
      if (jarr)
      {
        size_t count = json_array_get_count (jarr);
        for (size_t i = 0; i < count; i++)
        {
          edgex_device_device_upload_obj (svc, json_array_get_object (jarr, i), err);
        }
      }
      else
      {
        JSON_Object *jobj = json_value_get_object (jval);
        if (jobj)
        {
          edgex_device_device_upload_obj (svc, json_value_get_object (jval), err);
        }
      }
      json_value_free (jval);
    }
    else
    {
      iot_log_error (svc->logger, "File does not parse as JSON");
      *err = EDGEX_CONF_PARSE_ERROR;
    }
    if (err->code)
    {
      iot_log_error (svc->logger, "Error processing file %s", f->str);
      break;
    }
  }
  devsdk_strings_free (filenames);
}

static void startConfigured (devsdk_service_t *svc, const devsdk_timeout *deadline, devsdk_error *err)
{
  char *topic;
  svc->adminstate = UNLOCKED;

  svc->eventq = iot_threadpool_alloc (1, svc->config.device.eventqlen, IOT_THREAD_NO_PRIORITY, IOT_THREAD_NO_AFFINITY, svc->logger);
  iot_threadpool_start (svc->eventq);

  // Initialize MessageBus client
  const char *bustype = iot_data_string_map_get_string (svc->config.sdkconf, EX_BUS_TYPE);
  if (strcmp (bustype, "mqtt") == 0)
  {
    svc->msgbus = edgex_bus_create_mqtt (svc->logger, svc->name, svc->config.sdkconf, svc->secretstore, svc->eventq, deadline);
  }
  else if (strcmp (bustype, "redis") == 0)
  {
    svc->msgbus = edgex_bus_create_redstr (svc->logger, svc->name, svc->config.sdkconf, svc->secretstore, svc->eventq, deadline, svc->secureMode);
  }
  else
  {
    iot_log_error (svc->logger, "Unknown Message Bus type %s", bustype);
  }
  if (svc->msgbus == NULL)
  {
    *err = EDGEX_REMOTE_SERVER_DOWN;
    return;
  }

  /* Wait for core-metadata to be available */

  if (!ping_client (svc->logger, "core-metadata", &svc->config.endpoints.metadata, deadline, err))
  {
    return;
  }

  *err = EDGEX_OK;

  /* Register device service in metadata */

  char *base = malloc (URL_BUF_SIZE);
  snprintf (base, URL_BUF_SIZE - 1, "http://%s:%u", svc->config.service.host, svc->config.service.port);

  edgex_deviceservice *ds;
  ds = edgex_metadata_client_get_deviceservice
    (svc->logger, &svc->config.endpoints, svc->secretstore, svc->name, err);
  if (err->code)
  {
    iot_log_error (svc->logger, "get_deviceservice failed");
    return;
  }

  if (ds == NULL)
  {
    uint64_t millis = iot_time_msecs ();

    ds = malloc (sizeof (edgex_deviceservice));
    memset (ds, 0, sizeof (edgex_deviceservice));
    ds->baseaddress = base;
    ds->name = strdup (svc->name);
    ds->adminState = UNLOCKED;
    ds->origin = millis;
    for (int n = 0; svc->config.service.labels[n]; n++)
    {
      ds->labels = devsdk_strings_new (svc->config.service.labels[n], ds->labels);
    }

    edgex_metadata_client_create_deviceservice (svc->logger, &svc->config.endpoints, svc->secretstore, ds, err);
    if (err->code)
    {
      iot_log_error
        (svc->logger, "Unable to create device service in metadata");
      return;
    }
  }
  else
  {
    svc->adminstate = ds->adminState;
    if (svc->adminstate == LOCKED)
    {
      iot_log_warn (svc->logger, "Starting service in LOCKED state");
    }
    if (strcmp (ds->baseaddress, base))
    {
      iot_log_info (svc->logger, "Updating service endpoint in metadata");
      free (ds->baseaddress);
      ds->baseaddress = base;
      edgex_metadata_client_update_deviceservice (svc->logger, &svc->config.endpoints, svc->secretstore, ds->name, ds->baseaddress, err);
      if (err->code)
      {
        iot_log_error (svc->logger, "update_deviceservice failed");
        return;
      }
    }
    else
    {
      free (base);
    }
  }
  edgex_deviceservice_free (ds);

  /* Load DeviceProfiles from files and register in metadata */

  if (strlen (svc->config.device.profilesdir))
  {
    edgex_device_profiles_upload (svc, err);
    if (err->code)
    {
      return;
    }
  }

  /* Obtain Devices from metadata */

  edgex_device *devs = edgex_metadata_client_get_devices
    (svc->logger, &svc->config.endpoints, svc->secretstore, svc->name, err);

  if (err->code)
  {
    iot_log_error (svc->logger, "Unable to retrieve device list from metadata");
    return;
  }

  for (edgex_device *d = devs; d; d = d->next)
  {
    if (edgex_deviceprofile_get_internal (svc, d->profile->name, err) == NULL)
    {
      iot_log_error (svc->logger, "No profile %s found for device %s", d->profile->name, d->name);
    }
  }

  if (err->code)
  {
    iot_log_error (svc->logger, "Error processing device list");
    return;
  }

  edgex_devmap_populate_devices (svc->devices, devs);
  edgex_device_free (svc, devs);

  /* Start REST server now so that we get the callbacks on device addition */

  const char *bindaddr = strlen (svc->config.service.bindaddr) ? svc->config.service.bindaddr : svc->config.service.host;
  svc->daemon = edgex_rest_server_create (svc->logger, bindaddr, svc->config.service.port, svc->config.service.maxreqsz, err);
  if (err->code)
  {
    return;
  }
  if (iot_data_string_map_get_bool (svc->config.sdkconf, "Service/CORSConfiguration/EnableCORS", false))
  {
    const char *origin = iot_data_string_map_get_string (svc->config.sdkconf, "Service/CORSConfiguration/CORSAllowedOrigin");
    bool creds = iot_data_string_map_get_bool (svc->config.sdkconf, "Service/CORSConfiguration/CORSAllowCredentials", false);
    const char *methods = iot_data_string_map_get_string (svc->config.sdkconf, "Service/CORSConfiguration/CORSAllowedMethods");
    const char *headers = iot_data_string_map_get_string (svc->config.sdkconf, "Service/CORSConfiguration/CORSAllowedHeaders");
    const char *expose = iot_data_string_map_get_string (svc->config.sdkconf, "Service/CORSConfiguration/CORSExposeHeaders");
    int64_t maxage = iot_data_ui32 (iot_data_string_map_get (svc->config.sdkconf, "Service/CORSConfiguration/CORSMaxAge"));
    edgex_rest_server_enable_cors (svc->daemon, origin, methods, headers, expose, creds, maxage);
  }

  topic = edgex_bus_mktopic (svc->msgbus, EDGEX_DEV_TOPIC_ADD_DEV, "{profile}");
  edgex_bus_register_handler (svc->msgbus, topic, svc, edgex_callback_add_device);
  free (topic);

  topic = edgex_bus_mktopic (svc->msgbus, "", EDGEX_DEV_TOPIC_VALIDATE);
  edgex_bus_register_handler (svc->msgbus, topic, svc, edgex_device_handler_validate_addr_v3);
  free (topic);

  /* Load Devices from files and register in metadata */

  if (strlen (svc->config.device.devicesdir))
  {
    edgex_device_devices_upload (svc, err);
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

  /* Get Provision Watchers */

  edgex_watcher *w = edgex_metadata_client_get_watchers (svc->logger, &svc->config.endpoints, svc->secretstore, svc->name, err);
  if (err->code)
  {
    iot_log_error (svc->logger, "Unable to retrieve provision watchers from metadata");
  }
  if (w)
  {
    iot_log_info
      (svc->logger, "Added %u provision watchers from metadata", edgex_watchlist_populate (svc->watchlist, w));
    edgex_watcher_free (w);
  }

  /* Start scheduled events */

  iot_scheduler_start (svc->scheduler);

  /* Register MessageBus handlers */

  topic = edgex_bus_mktopic (svc->msgbus, EDGEX_DEV_TOPIC_DEVICE, "{device}/{op}/{cmd}");
  edgex_bus_register_handler (svc->msgbus, topic, svc, edgex_device_handler_devicev3);
  free (topic);

  topic = edgex_bus_mktopic (svc->msgbus, EDGEX_DEV_TOPIC_DEVICESERVICE, "");
  edgex_bus_register_handler (svc->msgbus, topic, svc, edgex_callback_update_deviceservice);
  free (topic);

  topic = edgex_bus_mktopic (svc->msgbus, EDGEX_DEV_TOPIC_DEL_DEV, "{profile}");
  edgex_bus_register_handler (svc->msgbus, topic, svc, edgex_callback_delete_device);
  free (topic);

  topic = edgex_bus_mktopic (svc->msgbus, EDGEX_DEV_TOPIC_UPDATE_DEV, "{profile}");
  edgex_bus_register_handler (svc->msgbus, topic, svc, edgex_callback_update_device);
  free (topic);

  topic = edgex_bus_mktopic (svc->msgbus, EDGEX_DEV_TOPIC_ADD_PW, "{profile}");
  edgex_bus_register_handler (svc->msgbus, topic, svc, edgex_callback_add_pw);
  free (topic);

  topic = edgex_bus_mktopic (svc->msgbus, EDGEX_DEV_TOPIC_DEL_PW, "{profile}");
  edgex_bus_register_handler (svc->msgbus, topic, svc, edgex_callback_delete_pw);
  free (topic);

  topic = edgex_bus_mktopic (svc->msgbus, EDGEX_DEV_TOPIC_UPDATE_PW, "{profile}");
  edgex_bus_register_handler (svc->msgbus, topic, svc, edgex_callback_update_pw);
  free (topic);

  topic = edgex_bus_mktopic (svc->msgbus, EDGEX_DEV_TOPIC_UPDATE_PROFILE, "{profile}");
  edgex_bus_register_handler (svc->msgbus, topic, svc, edgex_callback_update_profile);
  free (topic);

  /* Register REST handlers */
  if (svc->secureMode)
  {
    svc->device_name_wrapper = (auth_wrapper_t){ svc, svc->secretstore, edgex_device_handler_device_namev2};
    edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API3_DEVICE_NAME, DevSDK_Get | DevSDK_Put, &svc->device_name_wrapper, http_auth_wrapper);

    svc->discovery_wrapper = (auth_wrapper_t){ svc, svc->secretstore, edgex_device_handler_discoveryv2};
    edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API3_DISCOVERY, DevSDK_Post, &svc->discovery_wrapper, http_auth_wrapper);

    svc->config_wrapper = (auth_wrapper_t){ svc, svc->secretstore, edgex_device_handler_configv2};
    edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API3_CONFIG, DevSDK_Get, &svc->config_wrapper, http_auth_wrapper);

    svc->secret_wrapper = (auth_wrapper_t){ svc, svc->secretstore, edgex_device_handler_secret};
    edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API3_SECRET, DevSDK_Post, &svc->secret_wrapper, http_auth_wrapper);

    svc->version_wrapper = (auth_wrapper_t){ svc, svc->secretstore, version_handler};
    edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API_VERSION, DevSDK_Get, &svc->version_wrapper, http_auth_wrapper);
  }
  else
  {
    edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API3_DEVICE_NAME, DevSDK_Get | DevSDK_Put, svc, edgex_device_handler_device_namev2);

    edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API3_DISCOVERY, DevSDK_Post, svc, edgex_device_handler_discoveryv2);

    edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API3_DISCOVERY_DELETE, DevSDK_Delete, svc, edgex_device_handler_discovery_delete);

    edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API3_CONFIG, DevSDK_Get, svc, edgex_device_handler_configv2);

    edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API3_SECRET, DevSDK_Post, svc, edgex_device_handler_secret);

    edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API_VERSION, DevSDK_Get, svc, version_handler);
  }

  // No auth wrapper for ping (required for health check)
  edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API3_PING, DevSDK_Get, svc, ping2_handler);

  /* Ready. Register ourselves and log that we have started. */

  if (svc->registry)
  {
    devsdk_registry_register_service
    (
      svc->registry,
      svc->name,
      svc->config.service.host,
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

  edgex_device_periodic_discovery_configure (svc->discovery, svc->config.device.discovery_enabled, svc->config.device.discovery_interval);

  svc->metricschedule = NULL;
  devsdk_schedule_metrics (svc);

  if (svc->config.service.startupmsg)
  {
    iot_log_info (svc->logger, svc->config.service.startupmsg);
  }
}

void devsdk_service_start (devsdk_service_t *svc, iot_data_t *driverdfls, devsdk_error *err)
{
  iot_data_t *config_file, *common_config_file = NULL;
  bool uploadConfig = false;
  iot_data_t *common_config_map, *private_config_map, *configmap, *deviceservices_config;

  if (svc->starttime)
  {
    iot_log_error (svc->logger, "devsdk_service_start() called for already-started service, skipping");
    return;
  }

  *err = EDGEX_OK;
  svc->starttime = iot_time_msecs();

  devsdk_timeout deadline;
  devsdk_get_deadline (&deadline, svc->starttime);

  iot_threadpool_start (svc->thpool);

  config_file = edgex_device_loadConfig (svc->logger, svc->confpath, err);
  if (err->code)
  {
    iot_log_error (svc->logger, "Unable to load config file: %s", err->reason);
    return;
  }

  common_config_map = edgex_common_config_defaults (svc->name);

  if (!svc->regURL)
  {
    common_config_file = edgex_device_loadConfig (svc->logger, svc->commonconffile, err);
    if (err->code)
    {
      iot_log_error (svc->logger, "Unable to load common config file: %s", err->reason);
      return;
    }
    iot_data_t *allservices_config = iot_data_string_map_get_map(common_config_file, ALL_SVCS_NODE);
    deviceservices_config = iot_data_string_map_get_map(common_config_file, DEV_SVCS_NODE);
    if (allservices_config)
    {
      edgex_device_overrideConfig_map(common_config_map, allservices_config);
    }
    if (deviceservices_config)
    {
      edgex_device_overrideConfig_map(common_config_map, deviceservices_config);
    }
  }

  private_config_map = edgex_private_config_defaults(driverdfls);
  edgex_device_overrideConfig_map (common_config_map, config_file);
  edgex_device_overrideConfig_map (private_config_map, config_file);
  edgex_device_overrideConfig_env (svc->logger, common_config_map);
  edgex_device_overrideConfig_env (svc->logger, private_config_map);

  configmap = iot_data_alloc_map (IOT_DATA_STRING);
  iot_data_map_merge(configmap, common_config_map);
  iot_data_map_merge(configmap, private_config_map);

  /* Set up SecretStore */

  char *secure = getenv (SECUREENV);
  if (secure && strcmp (secure, "false") == 0)
  {
    svc->secretstore = edgex_secrets_get_insecure ();
  }
  else
  {
    svc->secureMode = true;
    svc->secretstore = edgex_secrets_get_vault ();
  }
  if (!edgex_secrets_init (svc->secretstore, svc->logger, svc->scheduler, svc->thpool, svc->name, configmap, &svc->metrics))
  {
    *err = EDGEX_BAD_CONFIG;
    return;
  }

  if (svc->regURL)
  {
    if (*svc->regURL == '\0')
    {
      svc->regURL = edgex_device_getRegURL (config_file);
    }
    if (svc->regURL)
    {
      char *delim = strstr (svc->regURL, "://");
      if (delim)
      {
        int len = delim - svc->regURL;
        if (len == strlen ("consul") && strncmp (svc->regURL, "consul", len) == 0)
        {
          svc->registry = devsdk_registry_get_consul ();
        }
        else if (len == strlen ("consul.http") && strncmp (svc->regURL, "consul.http", len) == 0)
        {
          svc->registry = devsdk_registry_get_consul ();
        }
      }
    }
    if (svc->registry == NULL)
    {
      iot_log_error (svc->logger, "Registry was requested but no location given");
      *err = EDGEX_INVALID_ARG;
      return;
    }
  }

  if (svc->registry)
  {
    if (!devsdk_registry_init (svc->registry, svc->logger, svc->thpool, svc->secretstore, svc->regURL))
    {
      iot_log_error (svc->logger, "cant initialise registry service at %s", svc->regURL);
      *err = EDGEX_INVALID_ARG;
      return;
    }
    if (!devsdk_registry_waitfor (svc->registry, &deadline))
    {
      iot_log_error (svc->logger, "registry service not running at %s", svc->regURL);
      *err = EDGEX_REMOTE_SERVER_DOWN;
      return;
    }

    iot_log_info (svc->logger, "Found registry service at %s", svc->regURL);
    svc->stopconfig = malloc (sizeof (atomic_bool));
    atomic_init (svc->stopconfig, false);

    if (svc->overwriteconfig)
    {
      iot_log_info (svc->logger, "--overwrite option is set. Not geting configuration from registry.");
      uploadConfig = true;
    }
    else
    {
      devsdk_error e;

      // get common configuration from registry
      devsdk_nvpairs *commonconf = devsdk_registry_get_common_config (svc->registry, edgex_device_updateCommonConf, svc, svc->stopconfig, &e, &deadline);
      if (commonconf)
      {
        edgex_device_overrideConfig_nvpairs (configmap, commonconf);
        edgex_device_overrideConfig_env (svc->logger, configmap);
        devsdk_nvpairs_free (commonconf);
      }
      else
      {
        iot_log_error (svc->logger, "Unable to get common configuration from registry.");
        iot_data_free (config_file);
        return;
      }

      devsdk_nvpairs *regconf = devsdk_registry_get_config (svc->registry, svc->name, edgex_device_updateConf, svc, svc->stopconfig, &e);
      if (regconf)
      {
        edgex_device_overrideConfig_nvpairs (configmap, regconf);
        edgex_device_overrideConfig_env (svc->logger, configmap);
        devsdk_nvpairs_free (regconf);
      }
      else
      {
        iot_log_info (svc->logger, "Unable to get configuration from registry.");
        iot_log_info (svc->logger, "Will load from file.");
        uploadConfig = true;
      }
    }
  }

  edgex_device_populateConfig (svc, configmap);

  if (uploadConfig)
  {
    iot_log_info (svc->logger, "Uploading configuration to registry.");
    devsdk_registry_put_config (svc->registry, svc->name, private_config_map, err);
    if (err->code)
    {
      iot_log_error (svc->logger, "Unable to upload config: %s", err->reason);
      iot_data_free (config_file);
      return;
    }
  }

  if (svc->registry)
  {
    devsdk_registry_query_service (svc->registry, "core-metadata", &svc->config.endpoints.metadata.host, &svc->config.endpoints.metadata.port, &deadline, err);
    if (err->code)
    {
      iot_data_free (config_file);
      *svc->stopconfig = true;
      return;
    }
  }
  else
  {
    edgex_device_parseClients (svc->logger, iot_data_string_map_get (deviceservices_config, "Clients"), &svc->config.endpoints);
  }

  iot_data_free (config_file);
  iot_data_free (common_config_file);
  iot_data_free (common_config_map);
  iot_data_free (private_config_map);

  iot_log_info (svc->logger, "Starting %s device service, version %s", svc->name, svc->version);
  iot_log_info (svc->logger, "EdgeX device SDK for C, version " CSDK_VERSION_STR);
  iot_log_debug (svc->logger, "Service configuration follows:");
  edgex_device_dumpConfig (svc->logger, configmap);

  startConfigured (svc, &deadline, err);

  if (err->code == 0)
  {
    iot_log_info (svc->logger, "Service started in: %dms", iot_time_msecs() - svc->starttime);
    iot_log_info (svc->logger, "Listening on port: %d", svc->config.service.port);
  }
}

void devsdk_register_http_handler
(
  devsdk_service_t *svc,
  const char *url,
  devsdk_http_method methods,
  void *context,
  devsdk_http_handler_fn handler,
  devsdk_error *e
)
{
  *e = EDGEX_OK;
  if (svc == NULL || svc->daemon == NULL)
  {
    *e = EDGEX_HTTP_SERVER_FAIL;
    iot_log_error (svc ? svc->logger : iot_logger_default (), "devsdk_register_http_handler called before service is running");
  }
  else
  {
    auth_wrapper_t *dynamic_wrapper = calloc(sizeof(auth_wrapper_t), 1); // No unregister(); this memory will never be freed
    *dynamic_wrapper = (auth_wrapper_t){ context, svc->secretstore, handler};  // Use our secretstore, and caller's context
    edgex_rest_server_register_handler (svc->daemon, url, methods, dynamic_wrapper, http_auth_wrapper);
  }
}

void devsdk_post_readings
(
  devsdk_service_t *svc,
  const char *devname,
  const char *resname,
  devsdk_commandresult *values
)
{
  if (svc->adminstate == LOCKED)
  {
    iot_log_debug (svc->logger, "Post readings: dropping event as service is locked");
    return;
  }

  edgex_device *dev = edgex_devmap_device_byname (svc->devices, devname);
  if (dev == NULL)
  {
    iot_log_error (svc->logger, "Post readings: no such device %s", devname);
    return;
  }

  const edgex_cmdinfo *command = edgex_deviceprofile_findcommand (svc, resname, dev->profile, true);
  edgex_device_release (svc, dev);

  if (command)
  {
    edgex_event_cooked *event = edgex_data_process_event
      (devname, command, values, svc->config.device.datatransform);

    if (event)
    {
      edgex_device_alloc_crlid (NULL);
      if (svc->config.device.maxeventsize && edgex_event_cooked_size (event) > svc->config.device.maxeventsize * 1024)
      {
        iot_log_error (svc->logger, "Post readings: Event size (%d KiB) exceeds configured MaxEventSize", edgex_event_cooked_size (event) / 1024);
      }
      else
      {
        edgex_data_client_add_event (svc->msgbus, event, &svc->metrics);
      }

      if (svc->config.device.updatelastconnected)
      {
        devsdk_error err = EDGEX_OK;
        edgex_metadata_client_update_lastconnected (svc->logger, &svc->config.endpoints, svc->secretstore, devname, &err);
      }
      edgex_device_free_crlid();
      edgex_event_cooked_free (event);
    }
  }
  else
  {
    iot_log_error (svc->logger, "Post readings: no such resource %s", resname);
  }
}

iot_data_t *devsdk_get_secrets (devsdk_service_t *svc, const char *path)
{
  return edgex_secrets_get (svc->secretstore, path);
}

void devsdk_service_stop (devsdk_service_t *svc, bool force, devsdk_error *err)
{
  *err = EDGEX_OK;
  iot_log_debug (svc->logger, "Stop device service");
  if (svc->stopconfig)
  {
    *svc->stopconfig = true;
  }
  if (svc->daemon)
  {
    edgex_rest_server_destroy (svc->daemon);
  }
  if (svc->discovery)
  {
    edgex_device_periodic_discovery_stop (svc->discovery);
  }
  if (svc->metricschedule)
  {
    iot_schedule_delete (svc->scheduler, svc->metricschedule);
  }
  if (svc->scheduler)
  {
    iot_scheduler_stop (svc->scheduler);
  }
  if (svc->registry)
  {
    devsdk_registry_deregister_service (svc->registry, svc->name, err);
    if (err->code)
    {
      iot_log_error (svc->logger, "Unable to deregister service from registry");
    }
  }
  iot_threadpool_wait (svc->eventq);
  iot_threadpool_wait (svc->thpool);
  svc->userfns.stop (svc->userdata, force);
  edgex_devmap_clear (svc->devices);
  iot_log_info (svc->logger, "Stopped device service");
}

void devsdk_service_free (devsdk_service_t *svc)
{
  if (svc)
  {
    iot_scheduler_free (svc->scheduler);
    edgex_devmap_free (svc->devices);
    edgex_bus_free (svc->msgbus);
    edgex_watchlist_free (svc->watchlist);
    edgex_device_periodic_discovery_free (svc->discovery);
    iot_threadpool_free (svc->thpool);
    iot_threadpool_free (svc->eventq);
    devsdk_registry_free (svc->registry);
    edgex_secrets_fini (svc->secretstore);
    iot_logger_free (svc->logger);
    edgex_device_freeConfig (svc);
    free (svc->stopconfig);
    free (svc->confpath);
    free (svc->name);
    free (svc);
  }
}
