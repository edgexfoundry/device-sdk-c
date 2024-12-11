/*
 * Copyright (c) 2018-2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#define DYN_NAME "Writable"
#define DRV_NAME "Driver"
#define INSECURE_NAME "InsecureSecrets"

#define DYN_PREFIX DYN_NAME "/"
#define DYN_PREFIXLEN (sizeof (DYN_PREFIX) - 1)

#define DRV_PREFIX DRV_NAME "/"
#define DRV_PREFIXLEN (sizeof (DRV_PREFIX) - 1)

#define DYN_DRV_PREFIX DYN_PREFIX DRV_PREFIX
#define DYN_DRV_PREFIXLEN (sizeof (DYN_DRV_PREFIX) - 1)

#include "config.h"
#include "api.h"
#include "service.h"
#include "errorlist.h"
#include "edgex-rest.h"
#include "edgex-logging.h"
#include "devutil.h"
#include "device.h"

#include <microhttpd.h>

#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

#include <iot/file.h>

#include <iot/data.h>

#include <iot/time.h>

#define ERRBUFSZ 1024
#define DEFAULTREG "keeper.http://localhost:59890"
#define DEFAULTSECPATH "%s/"
#define DEFAULTSECCAFILE "/tmp/edgex/secrets/%s/secrets-token.json"
#define DEFAULTMETRICSTOPIC "edgex/telemetry"

iot_data_t *edgex_common_config_defaults (const char *svcname)
{
  struct utsname utsbuffer;
  uname (&utsbuffer);
  char *secpath = malloc (sizeof (DEFAULTSECPATH) + strlen (svcname));
  sprintf (secpath, DEFAULTSECPATH, svcname);
  char *seccafile = malloc (sizeof (DEFAULTSECCAFILE) + strlen (svcname));
  sprintf (seccafile, DEFAULTSECCAFILE, svcname);

  iot_data_t *result = iot_data_alloc_map (IOT_DATA_STRING);

  iot_data_string_map_add (result, "Device/DataTransform", iot_data_alloc_bool (true));
  iot_data_string_map_add (result, "Device/Discovery/Enabled", iot_data_alloc_bool (true));
  iot_data_string_map_add (result, "Device/Discovery/Interval", iot_data_alloc_ui32 (0));
  iot_data_string_map_add (result, "Device/MaxCmdOps", iot_data_alloc_ui32 (0));

  iot_data_string_map_add (result, DYN_PREFIX "Telemetry/Interval", iot_data_alloc_string ("30s", IOT_DATA_REF));
  iot_data_string_map_add (result, DYN_PREFIX "Telemetry/Metrics/EventsSent", iot_data_alloc_bool (false));
  iot_data_string_map_add (result, DYN_PREFIX "Telemetry/Metrics/ReadingsSent", iot_data_alloc_bool (false));
  iot_data_string_map_add (result, DYN_PREFIX "Telemetry/Metrics/SecuritySecretsRequested", iot_data_alloc_bool (false));
  iot_data_string_map_add (result, DYN_PREFIX "Telemetry/Metrics/SecuritySecretsStored", iot_data_alloc_bool (false));

  iot_data_string_map_add (result, "Service/Host", iot_data_alloc_string (utsbuffer.nodename, IOT_DATA_COPY));
  iot_data_string_map_add (result, "Service/Port", iot_data_alloc_ui16 (59999));
  iot_data_string_map_add (result, "Service/RequestTimeout", iot_data_alloc_string ("5s", IOT_DATA_REF));
  iot_data_string_map_add (result, "Service/StartupMsg", iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (result, "Service/HealthCheckInterval", iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (result, "Service/ServerBindAddr", iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (result, "Service/MaxRequestSize", iot_data_alloc_ui64 (0));
  iot_data_string_map_add (result, "Service/CORSConfiguration/EnableCORS", iot_data_alloc_bool (false));
  iot_data_string_map_add (result, "Service/CORSConfiguration/CORSAllowCredentials", iot_data_alloc_bool (false));
  iot_data_string_map_add (result, "Service/CORSConfiguration/CORSAllowedOrigin", iot_data_alloc_string ("https://localhost", IOT_DATA_REF));
  iot_data_string_map_add (result, "Service/CORSConfiguration/CORSAllowedMethods", iot_data_alloc_string ("GET, POST, PUT, PATCH, DELETE", IOT_DATA_REF));
  iot_data_string_map_add (result, "Service/CORSConfiguration/CORSAllowedHeaders", iot_data_alloc_string ("Authorization, Accept, Accept-Language, Content-Language, Content-Type, X-Correlation-ID", IOT_DATA_REF));
  iot_data_string_map_add (result, "Service/CORSConfiguration/CORSExposeHeaders", iot_data_alloc_string ("Cache-Control, Content-Language, Content-Length, Content-Type, Expires, Last-Modified, Pragma, X-Correlation-ID", IOT_DATA_REF));
  iot_data_string_map_add (result, "Service/CORSConfiguration/CORSMaxAge", iot_data_alloc_ui32 (3600));

  iot_data_string_map_add (result, "Device/Labels", iot_data_alloc_typed_vector (0, IOT_DATA_STRING));
  iot_data_string_map_add (result, "Device/ProfilesDir", iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (result, "Device/DevicesDir", iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (result, "Device/EventQLength", iot_data_alloc_ui32 (0));
  iot_data_string_map_add (result, "Device/AllowedFails", iot_data_alloc_i32 (0));
  iot_data_string_map_add (result, "Device/DeviceDownTimeout", iot_data_alloc_ui64 (0));

  iot_data_string_map_add (result, EX_BUS_TYPE, iot_data_alloc_string ("mqtt", IOT_DATA_REF));
  edgex_bus_config_defaults (result, svcname);

  iot_data_string_map_add (result, "SecretStore/Type", iot_data_alloc_string ("vault", IOT_DATA_REF));
  iot_data_string_map_add (result, "SecretStore/Host", iot_data_alloc_string ("localhost", IOT_DATA_REF));
  iot_data_string_map_add (result, "SecretStore/Port", iot_data_alloc_ui16 (8200));
  iot_data_string_map_add (result, "SecretStore/Protocol", iot_data_alloc_string ("http", IOT_DATA_REF));
  iot_data_string_map_add (result, "SecretStore/Path", iot_data_alloc_string (secpath, IOT_DATA_TAKE));
  iot_data_string_map_add (result, "SecretStore/RootCaCertPath", iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (result, "SecretStore/ServerName", iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (result, "SecretStore/TokenFile", iot_data_alloc_string (seccafile, IOT_DATA_TAKE));
  iot_data_string_map_add (result, "SecretStore/Authentication/AuthType", iot_data_alloc_string ("X-Vault-Token", IOT_DATA_REF));
  iot_data_string_map_add (result, "SecretStore/SecretsFile", iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (result, "SecretStore/DisableScrubSecretsFile", iot_data_alloc_bool (false));

  return result;
}

iot_data_t *edgex_private_config_defaults (const iot_data_t *driverconf)
{
  struct utsname utsbuffer;
  uname (&utsbuffer);
  iot_data_t *result = iot_data_alloc_map (IOT_DATA_STRING);

  iot_data_string_map_add (result, DYN_PREFIX "LogLevel", iot_data_alloc_string ("WARNING", IOT_DATA_REF));

  iot_data_string_map_add (result, "Device/UpdateLastConnected", iot_data_alloc_bool (false));
  iot_data_string_map_add (result, "MaxEventSize", iot_data_alloc_ui32 (0));

  iot_data_string_map_add (result, DYN_PREFIX "Telemetry/PublishTopicPrefix", iot_data_alloc_string (DEFAULTMETRICSTOPIC, IOT_DATA_REF));
  iot_data_string_map_add (result, DYN_PREFIX "Telemetry/Metrics/ReadCommandsExecuted", iot_data_alloc_bool (false));

  iot_data_string_map_add (result, "Service/Host", iot_data_alloc_string (utsbuffer.nodename, IOT_DATA_COPY));
  iot_data_string_map_add (result, "Service/Port", iot_data_alloc_ui16 (59999));
  iot_data_string_map_add (result, "Service/StartupMsg", iot_data_alloc_string ("", IOT_DATA_REF));

  if (driverconf && (iot_data_type(driverconf) == IOT_DATA_MAP))
  {
    iot_data_map_iter_t iter;
    iot_data_map_iter (driverconf, &iter);
    while (iot_data_map_iter_next (&iter))
    {
      const char *key = iot_data_map_iter_string_key (&iter);
      char *dkey = malloc (DRV_PREFIXLEN + strlen (key) + 1);
      if (strncmp (key, DYN_PREFIX, DYN_PREFIXLEN) == 0)
      {
        strcpy (dkey, DYN_PREFIX);
        strcat (dkey, DRV_PREFIX);
        strcat (dkey, key + DYN_PREFIXLEN);
      }
      else
      {
        strcpy (dkey, DRV_PREFIX);
        strcat (dkey, key);
      }
      iot_data_map_add (result, iot_data_alloc_string (dkey, IOT_DATA_TAKE), iot_data_copy (iot_data_map_iter_value (&iter)));
    }
  }
  return result;
}

iot_data_t *edgex_device_loadConfig (iot_logger_t *lc, const char *path, devsdk_error *err)
{
  iot_data_t *result = NULL;
  char *conf = iot_file_read (path);
  if (conf)
  {
    iot_data_t *ex;
    result = iot_data_from_yaml (conf, &ex);
    if (result == NULL)
    {
      iot_log_error (lc, "Configuration file parse error: %s", iot_data_string (ex));
      *err = EDGEX_CONF_PARSE_ERROR;
    }
  }
  else
  {
    iot_log_error (lc, "Cant open file %s", path);
    *err = EDGEX_NO_CONF_FILE;
  }

  free (conf);
  return result;
}

static void edgex_config_setloglevel (iot_logger_t *lc, const char *lstr, iot_loglevel_t *result)
{
  iot_loglevel_t l;
  if (edgex_logger_nametolevel (lstr, &l))
  {
    if (*result != l)
    {
      *result = l;
      iot_logger_set_level (lc, IOT_LOG_INFO);
      iot_log_info (lc, "Setting LogLevel to %s", lstr);
      iot_logger_set_level (lc, l);
    }
  }
  else
  {
    iot_log_error (lc, "Invalid LogLevel %s", lstr);
  }
}

char *edgex_device_getRegURL (const iot_data_t *config)
{
  const iot_data_t *table = NULL;
  const char *rtype = NULL;
  const char *rhost = NULL;
  int64_t rport = 0;
  char *result = NULL;

  if (config)
  {
    table = iot_data_string_map_get (config, "Registry");
  }
  if (table)
  {
    rtype = iot_data_string_map_get_string (table, "Type");
    rhost = iot_data_string_map_get_string (table, "Host");
    rport = iot_data_string_map_get_i64  (table, "Port", 0);
  }

  if (rtype && *rtype && rhost && *rhost && rport)
  {
    int n = snprintf (NULL, 0, "%s://%s:%" PRIi64, rtype, rhost, rport) + 1;
    result = malloc (n);
    snprintf (result, n, "%s://%s:%" PRIi64, rtype, rhost, rport);
  }
  else
  {
    result = DEFAULTREG;
  }

  return result;
}

static void parseClient (const iot_data_t *client, edgex_device_service_endpoint *endpoint)
{
  if (client)
  {
    endpoint->host = strdup (iot_data_string_map_get_string (client, "Host"));
    endpoint->port = iot_data_string_map_get_i64 (client, "Port", 0);
  }
}

static void checkClientOverride (iot_logger_t *lc, char *name, edgex_device_service_endpoint *endpoint)
{
  bool log = false;
  const char *host;
  const char *portstr;
  uint16_t port = 0;
  char *qstr = malloc (strlen (name) + sizeof ("CLIENTS/x/HOST"));
  strcpy (qstr, "CLIENTS_");
  strcat (qstr, name);
  strcat (qstr, "_HOST");
  host = getenv (qstr);
  strcpy (qstr + strlen (qstr) - 4, "PORT");
  portstr = getenv (qstr);
  if (portstr)
  {
    port = atoi (portstr);
  }

  if (host)
  {
    free (endpoint->host);
    endpoint->host = strdup (host);
    log = true;
  }
  if (port)
  {
    endpoint->port = port;
    log = true;
  }
  if (log)
  {
    iot_log_info (lc, "Override %s service location = %s:%u", name, endpoint->host, endpoint->port);
  }
  free (qstr);
}

void edgex_device_parseClients (iot_logger_t *lc, const iot_data_t *clients, edgex_service_endpoints *endpoints)
{
  if (clients)
  {
    parseClient (iot_data_string_map_get (clients, "core-metadata"), &endpoints->metadata);
  }
  checkClientOverride (lc, "CORE_METADATA", &endpoints->metadata);
}

static void addInsecureSecretsMap (iot_data_t *confmap, const iot_data_t *config)
{
  if ((!config) || (iot_data_type(config) != IOT_DATA_MAP))
  {
    return;
  }
  const iot_data_t *sub = iot_data_string_map_get (config, DYN_NAME);
  if (sub && (iot_data_type(sub) == IOT_DATA_MAP))
  {
    sub = iot_data_string_map_get (sub, INSECURE_NAME);
  }
  if (sub && (iot_data_type(sub) == IOT_DATA_MAP))
  {
    iot_data_map_iter_t iter;
    iot_data_map_iter (sub, &iter);
    while (iot_data_map_iter_next (&iter))
    {
      const iot_data_t *tab = iot_data_map_iter_value (&iter);
      if (tab && (iot_data_type(tab) == IOT_DATA_MAP))
      {	
        const iot_data_t *sname = iot_data_string_map_get (tab, "SecretName");
	if (sname)
	{
	  const iot_data_t *secrets = iot_data_string_map_get (tab, "SecretData");
	  char *cpath;
	  const char *key = iot_data_map_iter_string_key (&iter);
	  iot_data_map_iter_t elem;
	  if (secrets && (iot_data_type(secrets) == IOT_DATA_MAP))
          {
	    iot_data_map_iter (secrets, &elem);
	    while (iot_data_map_iter_next (&elem))
            {
	      cpath = malloc (sizeof (DYN_PREFIX) + sizeof (INSECURE_NAME) + strlen (key) + sizeof ("/SecretData/") + strlen (iot_data_map_iter_string_key (&elem)));
	      strcpy (cpath, DYN_PREFIX);
	      strcat (cpath, INSECURE_NAME);
	      strcat (cpath, "/");
	      strcat (cpath, key);
	      strcat (cpath, "/SecretData/");
	      strcat (cpath, iot_data_map_iter_string_key (&elem));
	      iot_data_map_add (confmap, iot_data_alloc_string (cpath, IOT_DATA_TAKE), iot_data_add_ref (iot_data_map_iter_value (&elem)));
	    }
	  }
          cpath = malloc (sizeof (DYN_PREFIX) + sizeof (INSECURE_NAME) + strlen (key) + sizeof ("/SecretName/"));
          strcpy (cpath, DYN_PREFIX);
          strcat (cpath, INSECURE_NAME);
          strcat (cpath, "/");
          strcat (cpath, key);
          strcat (cpath, "/SecretName");
          iot_data_map_add (confmap, iot_data_alloc_string (cpath, IOT_DATA_TAKE), iot_data_add_ref (sname));
        }
      }
    }
  }
}

static char *checkOverride (char *qstr)
{
  for (char *c = qstr; *c; c++)
  {
    if (*c == '/')
    {
      *c = '_';
    }
    else if (islower (*c))
    {
      *c = toupper (*c);
    }
  }

  return getenv (qstr);
}

static const iot_data_t *findEntry (const iot_data_t *map, const char *key_in)
{
  char *key = strdup (key_in);
  const iot_data_t *result = NULL;
  if (map)
  {
    char *slash = strchr (key, '/');
    if (slash)
    {
      *slash = '\0';
      result = findEntry (iot_data_string_map_get (map, key), slash + 1);
      *slash = '/';
    }
    else
    {
      result = iot_data_string_map_get (map, key);
    }
  }
  free (key);
  return result;
}

void edgex_device_overrideConfig_map (iot_data_t *config, const iot_data_t *map)
{
  const iot_data_t *replace;
  iot_data_map_iter_t iter;

  if (config && (iot_data_type(config) == IOT_DATA_MAP))
  {
    iot_data_map_iter (config, &iter);
    while (iot_data_map_iter_next (&iter))
    {
      replace = findEntry (map, iot_data_map_iter_string_key (&iter));
      if (replace)
      {
	iot_data_t *newval = iot_data_transform (replace, iot_data_type (iot_data_map_iter_value (&iter)));
	if (newval)
        {
	  iot_data_free (iot_data_map_iter_replace_value (&iter, newval));
	}
      }
    }
  }
  addInsecureSecretsMap (config, map);
}

void edgex_device_overrideConfig_env (iot_logger_t *lc, iot_data_t *config)
{
  char *query = NULL;
  size_t qsize = 0;
  const char *key;
  iot_data_map_iter_t iter;

  if ((!config) || (iot_data_type(config) != IOT_DATA_MAP))
  {
    return;
  }
  iot_data_map_iter (config, &iter);
  while (iot_data_map_iter_next (&iter))
  {
    key = iot_data_map_iter_string_key (&iter);
    size_t req = strlen (key) + 1;
    if (qsize < req)
    {
      query = realloc (query, req);
      qsize = req;
    }
    strcpy (query, key);
    char *newtxt = checkOverride (query);
    if (newtxt)
    {
      iot_data_t *newval = iot_data_alloc_from_string (iot_data_type (iot_data_map_iter_value (&iter)), newtxt);
      if (newval)
      {
        iot_log_info (lc, "Override config %s = %s", key, newtxt);
        iot_data_free (iot_data_map_iter_replace_value (&iter, newval));
      }
    }
  }
  free (query);
}

static void addInsecureSecretsPairs (iot_data_t *confmap, const devsdk_nvpairs *config)
{
  for (const devsdk_nvpairs *p1 = config; p1; p1 = p1->next)
  {
    if (strncmp (p1->name, DYN_PREFIX INSECURE_NAME "/", sizeof (DYN_PREFIX INSECURE_NAME)) == 0)
    {
      iot_data_map_add (confmap, iot_data_alloc_string (strdup (p1->name), IOT_DATA_TAKE), iot_data_alloc_string (strdup (p1->value), IOT_DATA_TAKE));
    }
  }
}

void edgex_device_overrideConfig_nvpairs (iot_data_t *config, const devsdk_nvpairs *pairs)
{
  const char *raw;
  iot_data_map_iter_t iter;

  if (config && (iot_data_type(config) == IOT_DATA_MAP))
  {
    iot_data_map_iter (config, &iter);
    while (iot_data_map_iter_next (&iter))
    {
      raw = devsdk_nvpairs_value (pairs, iot_data_map_iter_string_key (&iter));
      if (raw)
      {
	iot_data_t *newval = iot_data_alloc_from_string (iot_data_type (iot_data_map_iter_value (&iter)), raw);
	if (newval)
	{
	  iot_data_free (iot_data_map_iter_replace_value (&iter, newval));
	}
      }
    }
  }
  addInsecureSecretsPairs (config, pairs);
}

static void edgex_device_populateCommonConfigFromMap (edgex_device_config *config, const iot_data_t *map)
{
  config->service.timeout = edgex_parsetime (iot_data_string_map_get_string (map, "Service/RequestTimeout"));
  config->service.checkinterval = iot_data_string_map_get_string (map, "Service/HealthCheckInterval");
  config->service.bindaddr = iot_data_string_map_get_string (map, "Service/ServerBindAddr");
  config->service.maxreqsz = iot_data_ui64 (iot_data_string_map_get (map, "Service/MaxRequestSize"));

  if (config->service.labels)
  {
    for (int i = 0; config->service.labels[i]; i++)
    {
      free (config->service.labels[i]);
    }
    free (config->service.labels);
  }

  const iot_data_t *labs = iot_data_string_map_get_vector (map, "Device/Labels");
  if (labs && iot_data_vector_type (labs) == IOT_DATA_STRING)
  {
    uint32_t i;
    config->service.labels = malloc (sizeof (char *) * (iot_data_vector_size (labs) + 1));
    for (i = 0; i < iot_data_vector_size (labs); i++)
    {
      config->service.labels[i] = strdup (iot_data_string (iot_data_vector_get (labs, i)));
    }
    config->service.labels[i] = NULL;
  }
  else
  {
    config->service.labels = malloc (sizeof (char *));
    config->service.labels[0] = NULL;
  }

  config->device.datatransform = iot_data_bool (iot_data_string_map_get (map, "Device/DataTransform"));
  config->device.discovery_enabled = iot_data_bool (iot_data_string_map_get (map, "Device/Discovery/Enabled"));
  config->device.discovery_interval = iot_data_ui32 (iot_data_string_map_get (map, "Device/Discovery/Interval"));
  config->device.maxcmdops = iot_data_ui32 (iot_data_string_map_get (map, "Device/MaxCmdOps"));
  config->device.maxeventsize = iot_data_ui32 (iot_data_string_map_get (map, "MaxEventSize"));
  config->device.profilesdir = iot_data_string_map_get_string (map, "Device/ProfilesDir");
  config->device.devicesdir = iot_data_string_map_get_string (map, "Device/DevicesDir");
  config->device.allowed_fails = iot_data_ui32 (iot_data_string_map_get (map, "Device/AllowedFails"));
  config->device.dev_downtime = iot_data_ui64 (iot_data_string_map_get (map, "Device/DeviceDownTimeout"));

  config->metrics.interval = iot_data_string_map_get_string (map, DYN_PREFIX "Telemetry/Interval");
  config->metrics.flags = iot_data_bool (iot_data_string_map_get (map, DYN_PREFIX "Telemetry/Metrics/EventsSent")) ? EX_METRIC_EVSENT : 0;
  if (iot_data_bool (iot_data_string_map_get (map, DYN_PREFIX "Telemetry/Metrics/ReadingsSent"))) config->metrics.flags |= EX_METRIC_RDGSENT;
  if (iot_data_bool (iot_data_string_map_get (map, DYN_PREFIX "Telemetry/Metrics/SecuritySecretsRequested"))) config->metrics.flags |= EX_METRIC_SECREQ;
  if (iot_data_bool (iot_data_string_map_get (map, DYN_PREFIX "Telemetry/Metrics/SecuritySecretsStored"))) config->metrics.flags |= EX_METRIC_SECSTO;
}

static void edgex_device_populateConfigFromMap (edgex_device_config *config, const iot_data_t *map)
{
  config->service.host = iot_data_string_map_get_string (map, "Service/Host");
  config->service.port = iot_data_ui16 (iot_data_string_map_get (map, "Service/Port"));
  config->service.startupmsg = iot_data_string_map_get_string (map, "Service/StartupMsg");

  config->device.updatelastconnected = iot_data_bool (iot_data_string_map_get (map, "Device/UpdateLastConnected"));
  config->device.eventqlen = iot_data_ui32 (iot_data_string_map_get (map, "Device/EventQLength"));

  config->metrics.topic = iot_data_string_map_get_string (map, DYN_PREFIX "Telemetry/PublishTopicPrefix");
  if (iot_data_bool (iot_data_string_map_get (map, DYN_PREFIX "Telemetry/Metrics/ReadCommandsExecuted"))) config->metrics.flags |= EX_METRIC_RDCMDS;
}

void edgex_device_populateConfig (devsdk_service_t *svc, iot_data_t *config)
{
  iot_data_map_iter_t iter;

  svc->config.sdkconf = config;
  svc->config.driverconf = iot_data_alloc_map (IOT_DATA_STRING);

  if (config && (iot_data_type(config) == IOT_DATA_MAP))
  {
    iot_data_map_iter (config, &iter);
    while (iot_data_map_iter_next (&iter))
    {
      const char *key = iot_data_map_iter_string_key (&iter);
      if (strncmp (key, DRV_PREFIX, DRV_PREFIXLEN) == 0)
      {
	iot_data_map_add (svc->config.driverconf, iot_data_alloc_string (key + DRV_PREFIXLEN, IOT_DATA_COPY), iot_data_copy (iot_data_map_iter_value (&iter)));
      }
      else if (strncmp (key, DYN_DRV_PREFIX, DYN_DRV_PREFIXLEN) == 0)
      {
	char *nkey = malloc (strlen (key) - DRV_PREFIXLEN + 1);
	strcpy (nkey, DYN_PREFIX);
	strcat (nkey, key + DYN_DRV_PREFIXLEN);
	iot_data_map_add (svc->config.driverconf, iot_data_alloc_string (nkey, IOT_DATA_TAKE), iot_data_copy (iot_data_map_iter_value (&iter)));
      }
    }
  }

  edgex_device_populateCommonConfigFromMap (&svc->config, config);
  edgex_device_populateConfigFromMap (&svc->config, config);

  edgex_config_setloglevel (svc->logger, iot_data_string_map_get_string (config, DYN_PREFIX "LogLevel"), &svc->config.loglevel);
}

void edgex_device_updateCommonConf (void *p, const devsdk_nvpairs *config)
{
  bool updatemetrics = false;
  devsdk_service_t *svc = (devsdk_service_t *)p;

  devsdk_timeout *timeout = (devsdk_timeout *)malloc(sizeof(devsdk_timeout));
  uint64_t t1, t2;
  timeout->deadline = iot_time_secs () + 10;
  timeout->interval = 1;
  t1 = iot_time_secs ();
  while (svc->config.sdkconf == NULL)
  {
    t2 = iot_time_secs ();
    if (t2 > timeout->deadline - timeout->interval)
    {
      iot_log_error (svc->logger, "SDK configuration is not ready");
      return;
    }
    if (timeout->interval > t2 - t1)
    {
      iot_log_warn(svc->logger, "waiting for SDK configuration to be available.");
      iot_wait_secs (timeout->interval - (t2 - t1));
    }
    t1 = iot_time_secs ();
  }
  free (timeout);

  iot_log_info (svc->logger, "Reconfiguring");

  edgex_device_overrideConfig_nvpairs (svc->config.sdkconf, config);
  updatemetrics = strcmp (svc->config.metrics.interval, iot_data_string_map_get_string (svc->config.sdkconf, DYN_PREFIX "Telemetry/Interval"));
  edgex_device_populateCommonConfigFromMap (&svc->config, svc->config.sdkconf);

  if (updatemetrics)
  {
    devsdk_schedule_metrics (svc);
  }
}

void edgex_device_updateConf (void *p, const devsdk_nvpairs *config)
{
  iot_data_map_iter_t iter;
  bool updatedriver = false;
  devsdk_service_t *svc = (devsdk_service_t *)p;

  iot_log_info (svc->logger, "Reconfiguring");

  edgex_device_overrideConfig_nvpairs (svc->config.sdkconf, config);
  edgex_device_populateConfigFromMap (&svc->config, svc->config.sdkconf);

  const char *lname = devsdk_nvpairs_value (config, DYN_PREFIX "LogLevel");
  if (lname)
  {
    edgex_config_setloglevel (svc->logger, lname, &svc->config.loglevel);
  }

  edgex_device_periodic_discovery_configure (svc->discovery, svc->config.device.discovery_enabled, svc->config.device.discovery_interval);

  if (svc->secretstore)
  {
    edgex_secrets_reconfigure (svc->secretstore, svc->config.sdkconf);
  }

  if (svc->config.sdkconf && (iot_data_type(svc->config.sdkconf) == IOT_DATA_MAP))
  {
    iot_data_map_iter (svc->config.sdkconf, &iter);
    while (iot_data_map_iter_next (&iter))
    {
      const char *key = iot_data_map_iter_string_key (&iter);
      if (strncmp (key, DYN_DRV_PREFIX, DYN_DRV_PREFIXLEN) == 0)
      {
	char *nkey = malloc (strlen (key) - DRV_PREFIXLEN + 1);
	strcpy (nkey, DYN_PREFIX);
	strcat (nkey, key + DYN_DRV_PREFIXLEN);
	const iot_data_t *driverval = iot_data_string_map_get (svc->config.driverconf, nkey);
	if (driverval && !iot_data_equal (driverval, iot_data_map_iter_value (&iter)))
        {
	  updatedriver = true;
	  iot_data_map_add (svc->config.driverconf, iot_data_alloc_string (nkey, IOT_DATA_TAKE), iot_data_copy (iot_data_map_iter_value (&iter)));
        }
	else
        {
	  free (nkey);
	}
      }
    }
  }
  if (updatedriver)
  {
    svc->userfns.reconfigure (svc->userdata, svc->config.driverconf);
  }
}

void edgex_device_dumpConfig (iot_logger_t *lc, iot_data_t *config)
{
  char *val;
  iot_data_map_iter_t iter;
  if ((!config) || (iot_data_type(config) != IOT_DATA_MAP))
  {
    return;
  }
  iot_data_map_iter (config, &iter);
  while (iot_data_map_iter_next (&iter))
  {
    val = iot_data_to_json (iot_data_map_iter_value (&iter));
    iot_log_debug (lc, "%s=%s", iot_data_map_iter_string_key (&iter), val);
    free (val);
  }
}

void edgex_device_freeConfig (devsdk_service_t *svc)
{
  edgex_device_watcherinfo *watcher;
  edgex_map_iter iter;
  const char *key;

  if (svc->config.service.labels)
  {
    for (int i = 0; svc->config.service.labels[i]; i++)
    {
      free (svc->config.service.labels[i]);
    }
    free (svc->config.service.labels);
  }

  free (svc->config.endpoints.metadata.host);

  iot_data_free (svc->config.sdkconf);
  iot_data_free (svc->config.driverconf);

  iter = edgex_map_iter (svc->config.watchers);
  while ((key = edgex_map_next (&svc->config.watchers, &iter)))
  {
    watcher = edgex_map_get (&svc->config.watchers, key);
    free (watcher->profile);
    free (watcher->key);
    free (watcher->matchstring);
    for (int i = 0; watcher->ids[i]; i++)
    {
      free (watcher->ids[i]);
    }
    free (watcher->ids);
  }
  edgex_map_deinit (&svc->config.watchers);
}

static JSON_Value *edgex_device_config_toJson (devsdk_service_t *svc)
{
  JSON_Value *val = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (val);

  JSON_Value *wval = json_value_init_object ();
  JSON_Object *wobj = json_value_get_object (wval);
  json_object_set_string (wobj, "LogLevel", edgex_logger_levelname (svc->config.loglevel));

  JSON_Value *dval = json_value_init_object ();
  JSON_Object *dobj = json_value_get_object (dval);

  JSON_Value *ddval = json_value_init_object ();
  JSON_Object *ddobj = json_value_get_object (ddval);
  json_object_set_boolean (ddobj, "Enabled", svc->config.device.discovery_enabled);
  json_object_set_uint (ddobj, "Interval", svc->config.device.discovery_interval);
  json_object_set_value (dobj, "Discovery", ddval);

  json_object_set_boolean
    (dobj, "DataTransform", svc->config.device.datatransform);
  json_object_set_uint (dobj, "MaxCmdOps", svc->config.device.maxcmdops);
  json_object_set_uint (dobj, "MaxEventSize", svc->config.device.maxeventsize);
  json_object_set_string (dobj, "ProfilesDir", svc->config.device.profilesdir);
  json_object_set_string (dobj, "DevicesDir", svc->config.device.devicesdir);
  json_object_set_boolean
    (dobj, "UpdateLastConnected", svc->config.device.updatelastconnected);
  json_object_set_uint (dobj, "EventQLength", svc->config.device.eventqlen);
  json_object_set_uint (dobj, "AllowedFails", svc->config.device.allowed_fails);
  json_object_set_uint (dobj, "DeviceDownTimeout", svc->config.device.dev_downtime);

  JSON_Value *lval = json_value_init_array ();
  JSON_Array *larr = json_value_get_array (lval);
  for (int i = 0; svc->config.service.labels[i]; i++)
  {
    json_array_append_string (larr, svc->config.service.labels[i]);
  }
  json_object_set_value (dobj, "Labels", lval);
  json_object_set_value (wobj, "Device", dval);

  json_object_set_value (obj, DYN_NAME, wval);

  const char *mqtype = iot_data_string_map_get_string (svc->config.sdkconf, EX_BUS_TYPE);
  JSON_Value *mqval = edgex_bus_config_json (svc->config.sdkconf);
  json_object_set_string (json_value_get_object (mqval), "Type", mqtype);
  json_object_set_value (obj, "MessageQueue", mqval);

  JSON_Value *cval = json_value_init_object ();
  JSON_Object *cobj = json_value_get_object (cval);

  JSON_Value *mval = json_value_init_object ();
  JSON_Object *mobj = json_value_get_object (mval);
  json_object_set_string (mobj, "Host", svc->config.endpoints.metadata.host);
  json_object_set_uint (mobj, "Port", svc->config.endpoints.metadata.port);
  json_object_set_value (cobj, "Metadata", mval);

  json_object_set_value (obj, "Clients", cval);

  mval = json_value_init_object ();
  mobj = json_value_get_object (mval);
  json_object_set_string (mobj, "Interval", svc->config.metrics.interval);
  json_object_set_string (mobj, "PublishTopicPrefix", svc->config.metrics.topic);
  json_object_set_boolean (mobj, "EventsSent", svc->config.metrics.flags & EX_METRIC_EVSENT);
  json_object_set_boolean (mobj, "ReadingsSent", svc->config.metrics.flags & EX_METRIC_RDGSENT);
  json_object_set_boolean (mobj, "ReadCommandsExecuted", svc->config.metrics.flags & EX_METRIC_RDCMDS);
  json_object_set_boolean (mobj, "SecuritySecretsRequested", svc->config.metrics.flags & EX_METRIC_SECREQ);
  json_object_set_boolean (mobj, "SecuritySecretsStored", svc->config.metrics.flags & EX_METRIC_SECSTO);
  json_object_set_value (obj, "Telemetry", mval);

  JSON_Value *sval = json_value_init_object ();
  JSON_Object *sobj = json_value_get_object (sval);
  json_object_set_string (sobj, "Host", svc->config.service.host);
  json_object_set_uint (sobj, "Port", svc->config.service.port);

  json_object_set_string (sobj, "RequestTimeout", iot_data_string_map_get_string (svc->config.sdkconf, "Service/RequestTimeout"));
  json_object_set_string (sobj, "StartupMsg", svc->config.service.startupmsg);
  json_object_set_string
    (sobj, "HealthCheckInterval", svc->config.service.checkinterval);
  json_object_set_string (sobj, "ServerBindAddr", svc->config.service.bindaddr);
  json_object_set_uint (sobj, "MaxRequestSize", svc->config.service.maxreqsz);

  JSON_Value *scval = json_value_init_object ();
  JSON_Object *scobj = json_value_get_object (scval);
  json_object_set_boolean (scobj, "EnableCORS", iot_data_bool (iot_data_string_map_get (svc->config.sdkconf, "Service/CORSConfiguration/EnableCORS")));
  json_object_set_boolean
    (scobj, "CORSAllowCredentials", iot_data_bool (iot_data_string_map_get (svc->config.sdkconf, "Service/CORSConfiguration/CORSAllowCredentials")));
  json_object_set_string (scobj, "CORSAllowedOrigin", iot_data_string_map_get_string (svc->config.sdkconf, "Service/CORSConfiguration/CORSAllowedOrigin"));
  json_object_set_string (scobj, "CORSAllowedMethods", iot_data_string_map_get_string (svc->config.sdkconf, "Service/CORSConfiguration/CORSAllowedMethods"));
  json_object_set_string (scobj, "CORSAllowedHeaders", iot_data_string_map_get_string (svc->config.sdkconf, "Service/CORSConfiguration/CORSAllowedHeaders"));
  json_object_set_string (scobj, "CORSExposeHeaders", iot_data_string_map_get_string (svc->config.sdkconf, "Service/CORSConfiguration/CORSExposeHeaders"));
  json_object_set_uint (scobj, "CORSMaxAge", iot_data_ui32 (iot_data_string_map_get (svc->config.sdkconf, "Service/CORSConfiguration/CORSMaxAge")));
  json_object_set_value (sobj, "CORSConfiguration", scval);

  json_object_set_value (obj, "Service", sval);

  if (svc->config.driverconf && (iot_data_type(svc->config.driverconf) == IOT_DATA_MAP))
  {
    iot_data_map_iter_t iter;
    iot_data_map_iter (svc->config.driverconf, &iter);
    dval = json_value_init_object ();
    dobj = json_value_get_object (dval);
    while (iot_data_map_iter_next (&iter))
    {
      json_object_set_string (dobj, iot_data_map_iter_string_key (&iter), iot_data_map_iter_string_value (&iter));
    }
    json_object_set_value (obj, DRV_NAME, dval);
  }

  return val;
}

void edgex_device_handler_configv2 (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *)ctx;
  edgex_configresponse *cr = malloc (sizeof (edgex_configresponse));

  edgex_baseresponse_populate ((edgex_baseresponse *)cr, EDGEX_API_VERSION, MHD_HTTP_OK, NULL);
  cr->config = edgex_device_config_toJson ((devsdk_service_t *)ctx);
  cr->svcname = svc->name;

  edgex_configresponse_write (cr, reply);
  edgex_configresponse_free (cr);
}
