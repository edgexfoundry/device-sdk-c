/*
 * Copyright (c) 2018-2021
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "service.h"
#include "api.h"
#include "edgex-logging.h"
#include "device.h"
#include "discovery.h"
#include "callback2.h"
#include "metrics.h"
#include "errorlist.h"
#include "rest-server.h"
#include "profiles.h"
#include "metadata.h"
#include "data.h"
#include "data-mqtt.h"
#include "data-redstr.h"
#include "rest.h"
#include "edgex-rest.h"
#include "iot/time.h"
#include "iot/iot.h"
#include "filesys.h"
#include "devutil.h"
#include "edgex/csdk-defs.h"

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
  printf ("  -o, --overwrite            \tOverwrite configuration in provider with local configuration.\n"
          "                             \t*** Use with cation *** Use will clobber existing settings in provider,\n"
          "                             \tproblematic if those settings were edited by hand intentionally\n");
  printf ("  -f, --file                 \tIndicates name of the local configuration file. Defaults to configuration.toml\n");
  printf ("  -p, --profile=<name>       \tIndicate configuration profile other than default.\n");
  printf ("  -c, --confdir=<dir>        \tSpecify local configuration directory\n");
  printf ("  -r, --registry             \tIndicates service should use Registry.\n");
  printf ("  -i, --instance=<name>      \tSpecify device service instance name (if specified this is appended to the device service name).\n");
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
      testArg (arg, val, "-i", "--instance", (const char **)&svc->name, &result) ||
      testArg (arg, val, "-p", "--profile", &svc->profile, &result) ||
      testArg (arg, val, "-c", "--confdir", &svc->confdir, &result) ||
      testArg (arg, val, "-f", "--file", &svc->conffile, &result)
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

  checkEnv (&svc->regURL, "EDGEX_CONFIGURATION_PROVIDER");
  checkEnv (&svc->profile, "EDGEX_PROFILE");
  checkEnv (&svc->confdir, "EDGEX_CONF_DIR");
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
    pathlen = strlen (dir) + 1 + strlen ("configuration.toml") + 1;
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
    strcat (result, ".toml");
  }
  return result;
}

devsdk_service_t *devsdk_service_new
  (const char *defaultname, const char *version, void *impldata, devsdk_callbacks *implfns, int *argc, char **argv, devsdk_error *err)
{
  iot_logger_t *logger = iot_logger_alloc_custom (defaultname, IOT_LOG_TRACE, "", edgex_log_tostdout, NULL, true);
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

  if (!processCmdLine (argc, argv, result))
  {
    *err = EDGEX_INVALID_ARG;
    return NULL;
  }

  iot_init ();
  if (result->name)
  {
    const char *n = result->name;
    int l = strlen (n) + strlen (defaultname) + 2;
    result->name = malloc (l);
    strcpy (result->name, defaultname);
    strcat (result->name, "_");
    strcat (result->name, n);
    result->logger->name = result->name;
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
  result->discovery = edgex_device_periodic_discovery_alloc (result->logger, result->scheduler, result->thpool, implfns->discover, impldata);
  return result;
}

static void ping2_handler (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  edgex_pingresponse pr;

  edgex_baseresponse_populate ((edgex_baseresponse *)&pr, "v2", MHD_HTTP_OK, NULL);
  pr.timestamp = iot_time_secs ();
  edgex_pingresponse_write (&pr, reply);
}

static void version_handler (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;
  JSON_Value *val = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (val);
  json_object_set_string (obj, "version", svc->version);
  json_object_set_string (obj, "sdk_version", CSDK_VERSION_STR);
  char *json = json_serialize_to_string (val);
  json_value_free (val);
  reply->data.bytes = json;
  reply->data.size = strlen (json);
  reply->content_type = CONTENT_JSON;
  reply->code = MHD_HTTP_OK;
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

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/v2/ping", ep->host, ep->port);

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
      devsdk_wait_msecs (timeout->interval - (t2 - t1));
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
        edgex_metadata_client_add_device_jobj (svc->logger, &svc->config.endpoints, deviceobj, err);
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

static void edgex_device_device_upload_table (devsdk_service_t *svc, toml_table_t *toml, devsdk_error *err)
{
  toml_array_t *devs = toml_array_in (toml, "DeviceList");
  if (devs)
  {
    edgex_device_process_configured_devices (svc, devs, err);
  }
  else
  {
    iot_log_warn (svc->logger, "Device upload: No DeviceList in TOML file");
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
  filenames = devsdk_scandir (svc->logger, svc->config.device.devicesdir, "toml");
  for (devsdk_strings *f = filenames; f; f = f->next)
  {
    if (strcmp (f->str, svc->confpath) == 0)
    {
      continue;
    }
    FILE *fp = fopen (f->str, "r");
    if (fp)
    {
      char errbuf[ERRBUFSZ];
      toml_table_t *toml = toml_parse_file (fp, errbuf, ERRBUFSZ);
      fclose (fp);
      if (toml)
      {
        edgex_device_device_upload_table (svc, toml, err);
        toml_free (toml);
      }
      else
      {
        iot_log_error (svc->logger, "File does not parse as TOML");
        *err = EDGEX_CONF_PARSE_ERROR;
      }
    }
    else
    {
      iot_log_error (svc->logger, "Cant open file: %s", strerror (errno));
      *err = EDGEX_NO_CONF_FILE;
    }
    if (err->code)
    {
      iot_log_error (svc->logger, "Error processing file %s", f->str);
      break;
    }
  }
  devsdk_strings_free (filenames);
}

static void startConfigured (devsdk_service_t *svc, toml_table_t *config, const devsdk_timeout *deadline, devsdk_error *err)
{
  svc->adminstate = UNLOCKED;

  svc->eventq = iot_threadpool_alloc (1, svc->config.device.eventqlen, IOT_THREAD_NO_PRIORITY, IOT_THREAD_NO_AFFINITY, svc->logger);
  iot_threadpool_start (svc->eventq);

  /* Wait for metadata and data to be available */

  if (iot_data_string_map_get_bool (svc->config.sdkconf, "Device/UseMessageBus", false))
  {
    const char *bustype = iot_data_string_map_get_string (svc->config.sdkconf, EX_MQ_TYPE);
    if (strcmp (bustype, "mqtt") == 0)
    {
      svc->dataclient = edgex_data_client_new_mqtt (svc->config.sdkconf, svc->logger, deadline, svc->eventq);
    }
    else if (strcmp (bustype, "redis") == 0)
    {
      svc->dataclient = edgex_data_client_new_redstr (svc->config.sdkconf, svc->logger, deadline, svc->eventq);
    }
    else
    {
      iot_log_error (svc->logger, "Unknown Message Bus type %s", bustype);
    }
    if (svc->dataclient == NULL)
    {
      *err = EDGEX_REMOTE_SERVER_DOWN;
      return;
    }
  }
  else
  {
    svc->dataclient = edgex_data_client_new_rest (&svc->config.endpoints.data, svc->logger, svc->eventq);
    if (!ping_client (svc->logger, "core-data", &svc->config.endpoints.data, deadline, err))
    {
      return;
    }
  }

  if (!ping_client (svc->logger, "core-metadata", &svc->config.endpoints.metadata, deadline, err))
  {
    return;
  }

  char *secure = getenv (SECUREENV);
  if (secure && strcmp (secure, "false") == 0)
  {
    svc->secretstore = edgex_secrets_get_insecure ();
  }
  else
  {
    const char *sectype = iot_data_string_map_get_string (svc->config.sdkconf, "SecretStore/Type");
    if (strcmp (sectype, "vault") == 0)
    {
      svc->secretstore = edgex_secrets_get_vault ();
    }
    else
    {
      iot_log_error (svc->logger, "Unknown Secret Store type %s", sectype);
      *err = EDGEX_BAD_CONFIG;
      return;
    }
  }
  if (!edgex_secrets_init (svc->secretstore, svc->logger, svc->config.sdkconf))
  {
    *err = EDGEX_BAD_CONFIG;
    return;
  }

  *err = EDGEX_OK;

  /* Register device service in metadata */

  char *base = malloc (URL_BUF_SIZE);
  snprintf (base, URL_BUF_SIZE - 1, "http://%s:%u", svc->config.service.host, svc->config.service.port);

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

    edgex_metadata_client_create_deviceservice (svc->logger, &svc->config.endpoints, ds, err);
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
      edgex_metadata_client_update_deviceservice (svc->logger, &svc->config.endpoints, ds->name, ds->baseaddress, err);
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
    (svc->logger, &svc->config.endpoints, svc->name, err);

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

  edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API2_CALLBACK_DEVICE, DevSDK_Put | DevSDK_Post, svc, edgex_device_handler_callback_device);

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

  edgex_watcher *w = edgex_metadata_client_get_watchers (svc->logger, &svc->config.endpoints, svc->name, err);
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

  /* Register REST handlers */

  edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API2_CALLBACK_DEVICE_NAME, DevSDK_Delete, svc, edgex_device_handler_callback_device_name);

  edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API2_CALLBACK_PROFILE, DevSDK_Put | DevSDK_Post, svc, edgex_device_handler_callback_profile);

  edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API2_CALLBACK_WATCHER, DevSDK_Put | DevSDK_Post, svc, edgex_device_handler_callback_watcher);

  edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API2_CALLBACK_WATCHER_NAME, DevSDK_Delete, svc, edgex_device_handler_callback_watcher_name);

  edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API2_CALLBACK_SERVICE, DevSDK_Put, svc, edgex_device_handler_callback_service);

  edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API2_DEVICE_NAME, DevSDK_Get | DevSDK_Put, svc, edgex_device_handler_device_namev2);

  edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API2_DISCOVERY, DevSDK_Post, svc, edgex_device_handler_discoveryv2);

  edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API2_METRICS, DevSDK_Get, svc, edgex_device_handler_metricsv2);

  edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API2_CONFIG, DevSDK_Get, svc, edgex_device_handler_configv2);

  edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API2_SECRET, DevSDK_Post, svc, edgex_device_handler_secret);

  edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API_VERSION, DevSDK_Get, svc, version_handler);

  edgex_rest_server_register_handler (svc->daemon, EDGEX_DEV_API2_PING, DevSDK_Get, svc, ping2_handler);

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

  if (svc->config.service.startupmsg)
  {
    iot_log_info (svc->logger, svc->config.service.startupmsg);
  }
}

void devsdk_service_start (devsdk_service_t *svc, iot_data_t *driverdfls, devsdk_error *err)
{
  toml_table_t *configtoml = NULL;
  bool uploadConfig = false;
  iot_data_t *configmap;

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

  configmap = edgex_config_defaults (driverdfls, svc->name);

  if (svc->regURL)
  {
    if (*svc->regURL == '\0')
    {
      devsdk_error e;
      configtoml = edgex_device_loadConfig (svc->logger, svc->confpath, &e);
      svc->regURL = edgex_device_getRegURL (configtoml);
    }
    if (svc->regURL)
    {
      svc->registry = devsdk_registry_get_registry (svc->logger, svc->thpool, svc->regURL);
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
      devsdk_nvpairs *regconf = devsdk_registry_get_config (svc->registry, svc->name, edgex_device_updateConf, svc, svc->stopconfig, &e);
      if (regconf)
      {
        edgex_device_overrideConfig_nvpairs (configmap, regconf);
        edgex_device_overrideConfig_env (svc->logger, configmap);
        edgex_device_populateConfig (svc, configmap);
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

  if (uploadConfig || (svc->registry == NULL))
  {
    if (configtoml == NULL)
    {
      configtoml = edgex_device_loadConfig (svc->logger, svc->confpath, err);
      if (err->code)
      {
        return;
      }
    }

    edgex_device_overrideConfig_toml (configmap, configtoml);
    edgex_device_overrideConfig_env (svc->logger, configmap);
    edgex_device_populateConfig (svc, configmap);

    if (uploadConfig)
    {
      iot_log_info (svc->logger, "Uploading configuration to registry.");
      devsdk_registry_put_config (svc->registry, svc->name, configmap, err);
      if (err->code)
      {
        iot_log_error (svc->logger, "Unable to upload config: %s", err->reason);
        toml_free (configtoml);
        return;
      }
    }
  }

  if (svc->registry)
  {
    devsdk_error e;
    devsdk_registry_query_service (svc->registry, "core-metadata", &svc->config.endpoints.metadata.host, &svc->config.endpoints.metadata.port, &e);
    devsdk_registry_query_service (svc->registry, "core-data", &svc->config.endpoints.data.host, &svc->config.endpoints.data.port, &e);
  }
  else
  {
    edgex_device_parseTomlClients (svc->logger, toml_table_in (configtoml, "Clients"), &svc->config.endpoints, err);
  }

  iot_log_info (svc->logger, "Starting %s device service, version %s", svc->name, svc->version);
  iot_log_info (svc->logger, "EdgeX device SDK for C, version " CSDK_VERSION_STR);
  iot_log_debug (svc->logger, "Service configuration follows:");
  edgex_device_dumpConfig (svc->logger, configmap);

  startConfigured (svc, configtoml, &deadline, err);

  toml_free (configtoml);

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
    edgex_rest_server_register_handler (svc->daemon, url, methods, context, handler);
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
      edgex_data_client_add_event (svc->dataclient, event);
      if (svc->config.device.updatelastconnected)
      {
        devsdk_error err = EDGEX_OK;
        edgex_metadata_client_update_lastconnected (svc->logger, &svc->config.endpoints, devname, &err);
      }
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
  edgex_secrets_fini (svc->secretstore);
  iot_log_info (svc->logger, "Stopped device service");
}

void devsdk_service_free (devsdk_service_t *svc)
{
  if (svc)
  {
    iot_scheduler_free (svc->scheduler);
    edgex_devmap_free (svc->devices);
    edgex_data_client_free (svc->dataclient);
    edgex_watchlist_free (svc->watchlist);
    edgex_device_periodic_discovery_free (svc->discovery);
    iot_threadpool_free (svc->thpool);
    iot_threadpool_free (svc->eventq);
    devsdk_registry_free (svc->registry);
    devsdk_registry_fini ();
    iot_logger_free (svc->logger);
    edgex_device_freeConfig (svc);
    free (svc->stopconfig);
    free (svc->confpath);
    free (svc->name);
    free (svc);
    iot_fini ();
  }
}
