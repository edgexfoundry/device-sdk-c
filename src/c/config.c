/*
 * Copyright (c) 2018-2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#define DYN_NAME "Writable"
#define DRV_NAME "Driver"

#define DYN_PREFIX DYN_NAME "/"
#define DYN_PREFIXLEN (sizeof (DYN_PREFIX) - 1)

#define DRV_PREFIX DRV_NAME "/"
#define DRV_PREFIXLEN (sizeof (DRV_PREFIX) - 1)

#define DYN_DRV_PREFIX DYN_PREFIX DRV_PREFIX
#define DYN_DRV_PREFIXLEN (sizeof (DYN_DRV_PREFIX) - 1)

#include "config.h"
#include "service.h"
#include "data-mqtt.h"
#include "data-redstr.h"
#include "errorlist.h"
#include "edgex-rest.h"
#include "edgex-logging.h"
#include "devutil.h"
#include "autoevent.h"
#include "device.h"
#include "edgex/devices.h"

#include <microhttpd.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/utsname.h>

#define ERRBUFSZ 1024
#define DEFAULTREG "consul.http://localhost:8500"

iot_data_t *edgex_config_defaults (const iot_data_t *driverconf)
{
  struct utsname utsbuffer;
  uname (&utsbuffer);

  iot_data_t *result = iot_data_alloc_map (IOT_DATA_STRING);

  iot_data_string_map_add (result, DYN_PREFIX "LogLevel", iot_data_alloc_string ("WARNING", IOT_DATA_REF));
  iot_data_string_map_add (result, DYN_PREFIX "Device/DataTransform", iot_data_alloc_bool (true));
  iot_data_string_map_add (result, DYN_PREFIX "Device/Discovery/Enabled", iot_data_alloc_bool (true));
  iot_data_string_map_add (result, DYN_PREFIX "Device/Discovery/Interval", iot_data_alloc_ui32 (0));
  iot_data_string_map_add (result, DYN_PREFIX "Device/UpdateLastConnected", iot_data_alloc_bool (false));
  iot_data_string_map_add (result, DYN_PREFIX "Device/MaxCmdOps", iot_data_alloc_ui32 (0));
  iot_data_string_map_add (result, DYN_PREFIX "Device/MaxCmdResultLen", iot_data_alloc_ui32 (0));

  iot_data_string_map_add (result, "Service/Host", iot_data_alloc_string (utsbuffer.nodename, IOT_DATA_COPY));
  iot_data_string_map_add (result, "Service/Port", iot_data_alloc_ui16 (49999));
  iot_data_string_map_add (result, "Service/Timeout", iot_data_alloc_ui32 (1000));
  iot_data_string_map_add (result, "Service/ConnectRetries", iot_data_alloc_ui32 (20));
  iot_data_string_map_add (result, "Service/StartupMsg", iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (result, "Service/CheckInterval", iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (result, "Service/Labels", iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (result, "Service/ServerBindAddr", iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (result, "Service/MaxRequestSize", iot_data_alloc_ui64 (0));
  iot_data_string_map_add (result, "Service/UseMessageBus", iot_data_alloc_bool (false));

  iot_data_string_map_add (result, "Device/ProfilesDir", iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (result, "Device/DevicesDir", iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (result, "Device/EventQLength", iot_data_alloc_ui32 (0));

  iot_data_string_map_add (result, EX_MQ_TYPE, iot_data_alloc_string ("", IOT_DATA_REF));
  edgex_mqtt_config_defaults (result);
  // NB redis-streams uses a subset of the mqtt options

  if (driverconf)
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

toml_table_t *edgex_device_loadConfig (iot_logger_t *lc, const char *path, devsdk_error *err)
{
  toml_table_t *result = NULL;
  FILE *fp;
  char errbuf[ERRBUFSZ];

  fp = fopen (path, "r");
  if (fp)
  {
    result = toml_parse_file (fp, errbuf, ERRBUFSZ);
    fclose (fp);
    if (result == NULL)
    {
      iot_log_error (lc, "Configuration file parse error: %s", errbuf);
      *err = EDGEX_CONF_PARSE_ERROR;
    }
  }
  else
  {
    iot_log_error (lc, "Cant open file %s : %s", path, strerror (errno));
    *err = EDGEX_NO_CONF_FILE;
  }

  return result;
}

static void edgex_config_setloglevel (iot_logger_t *lc, const char *lstr, iot_loglevel_t *result)
{
  iot_loglevel_t l;
  for (l = IOT_LOG_ERROR; l <= IOT_LOG_TRACE; l++)
  {
    if (strcasecmp (lstr, edgex_logger_levelname (l)) == 0)
    {
      if (*result != l)
      {
        *result = l;
        iot_logger_set_level (lc, IOT_LOG_INFO);
        iot_log_info (lc, "Setting LogLevel to %s", lstr);
        iot_logger_set_level (lc, l);
      }
      break;
    }
  }
  if (l > IOT_LOG_TRACE)
  {
    iot_log_error (lc, "Invalid LogLevel %s", lstr);
  }
}

/* As toml_rtos but return null instead of a zero-length string */

static void toml_rtos2 (const char *s, char **ret)
{
  if (s)
  {
    toml_rtos (s, ret);
    if (*ret && (**ret == 0))
    {
      free (*ret);
      *ret = NULL;
    }
  }
}

/* As toml_rtob but return a bool */

static void toml_rtob2 (const char *raw, bool *ret)
{
  if (raw)
  {
    int dummy;
    toml_rtob (raw, &dummy);
    *ret = dummy;
  }
}

/* As toml_rtoi but return uint16 */

static void toml_rtoui16
  (const char *raw, uint16_t *ret, iot_logger_t *lc, devsdk_error *err)
{
  if (raw)
  {
    int64_t dummy;
    if (toml_rtoi (raw, &dummy) == 0 && dummy >= 0 && dummy <= UINT16_MAX)
    {
      *ret = dummy;
    }
    else
    {
      iot_log_error (lc, "Unable to parse %s as uint16", raw);
      *err = EDGEX_BAD_CONFIG;
    }
  }
}

char *edgex_device_getRegURL (toml_table_t *config)
{
  toml_table_t *table = NULL;
  char *rtype = NULL;
  char *rhost = NULL;
  int64_t rport = 0;
  char *result = NULL;
  int n;

  if (config)
  {
    table = toml_table_in (config, "Registry");
  }
  if (table)
  {
    toml_rtos (toml_raw_in (table, "Type"), &rtype);
    toml_rtos (toml_raw_in (table, "Host"), &rhost);
    toml_rtoi (toml_raw_in (table, "Port"), &rport);
  }

  if (rtype && *rtype && rhost && *rhost && rport)
  {
    n = snprintf (NULL, 0, "%s://%s:%" PRIi64, rtype, rhost, rport) + 1;
    result = malloc (n);
    snprintf (result, n, "%s://%s:%" PRIi64, rtype, rhost, rport);
  }
  else
  {
    result = DEFAULTREG;
  }

  free (rtype);
  free (rhost);
  return result;
}

static void parseClient
  (iot_logger_t *lc, toml_table_t *client, edgex_device_service_endpoint *endpoint, devsdk_error *err)
{
  if (client)
  {
    toml_rtos2 (toml_raw_in (client, "Host"), &endpoint->host);
    toml_rtoui16 (toml_raw_in (client, "Port"), &endpoint->port, lc, err);
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

void edgex_device_parseTomlClients
  (iot_logger_t *lc, toml_table_t *clients, edgex_service_endpoints *endpoints, devsdk_error *err)
{
  if (clients)
  {
    parseClient (lc, toml_table_in (clients, "edgex-core-data"), &endpoints->data, err);
    parseClient (lc, toml_table_in (clients, "edgex-core-metadata"), &endpoints->metadata, err);
  }
  checkClientOverride (lc, "EDGEX_CORE_DATA", &endpoints->data);
  checkClientOverride (lc, "EDGEX_CORE_METADATA", &endpoints->metadata);
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

static const char *findEntry (char *key, toml_table_t *table)
{
  const char *result = NULL;
  if (table)
  {
    char *slash = strchr (key, '/');
    if (slash)
    {
      *slash = '\0';
      result = findEntry (slash + 1, toml_table_in (table, key));
      *slash = '/';
    }
    else
    {
      result = toml_raw_in (table, key);
    }
  }
  return result;
}

void edgex_device_overrideConfig_toml (iot_data_t *config, toml_table_t *toml)
{
  char *key;
  const char *raw;
  iot_data_map_iter_t iter;

  iot_data_map_iter (config, &iter);
  while (iot_data_map_iter_next (&iter))
  {
    key = strdup (iot_data_map_iter_string_key (&iter));
    raw = findEntry (key, toml);
    if (raw)
    {
      iot_data_t *newval = NULL;
      if (iot_data_type (iot_data_map_iter_value (&iter)) == IOT_DATA_STRING)
      {
        char *newtxt;
        if (toml_rtos (raw, &newtxt) != -1)
        {
          newval = iot_data_alloc_string (newtxt, IOT_DATA_TAKE);
        }
      }
      else
      {
        newval = edgex_data_from_string (iot_data_type (iot_data_map_iter_value (&iter)), raw);
      }
      if (newval)
      {
        iot_data_free (iot_data_map_iter_replace_value (&iter, newval));
      }
    }
    free (key);
  }
}

void edgex_device_overrideConfig_env (iot_logger_t *lc, iot_data_t *config)
{
  char *query = NULL;
  size_t qsize = 0;
  const char *key;
  iot_data_map_iter_t iter;

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
      iot_data_t *newval = edgex_data_from_string (iot_data_type (iot_data_map_iter_value (&iter)), newtxt);
      if (newval)
      {
        iot_log_info (lc, "Override config %s = %s", key, newtxt);
        iot_data_free (iot_data_map_iter_replace_value (&iter, newval));
      }
    }
  }
  free (query);
}

void edgex_device_overrideConfig_nvpairs (iot_data_t *config, const devsdk_nvpairs *pairs)
{
  const char *raw;
  iot_data_map_iter_t iter;

  iot_data_map_iter (config, &iter);
  while (iot_data_map_iter_next (&iter))
  {
    raw = devsdk_nvpairs_value (pairs, iot_data_map_iter_string_key (&iter));
    if (raw)
    {
      iot_data_t *newval = edgex_data_from_string (iot_data_type (iot_data_map_iter_value (&iter)), raw);
      if (newval)
      {
        iot_data_free (iot_data_map_iter_replace_value (&iter, newval));
      }
    }
  }
}

static void edgex_device_populateConfigFromMap (edgex_device_config *config, const iot_data_t *map)
{
  config->service.host = iot_data_string_map_get_string (map, "Service/Host");
  config->service.port = iot_data_ui16 (iot_data_string_map_get (map, "Service/Port"));
  uint32_t tm = iot_data_ui32 (iot_data_string_map_get (map, "Service/Timeout"));
  config->service.timeout.tv_sec = tm / 1000;
  config->service.timeout.tv_nsec = 1000000 * (tm % 1000);
  config->service.connectretries = iot_data_ui32 (iot_data_string_map_get (map, "Service/ConnectRetries"));
  config->service.startupmsg = iot_data_string_map_get_string (map, "Service/StartupMsg");
  config->service.checkinterval = iot_data_string_map_get_string (map, "Service/CheckInterval");
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

  const char *lstr = iot_data_string_map_get_string (map, "Service/Labels");
  if (lstr && *lstr)
  {
    const char *iter = lstr;
    int n = 1;
    while ((iter = strchr (iter, ',')))
    {
      iter++;
      n++;
    }
    config->service.labels = malloc (sizeof (char *) * (n + 1));

    iter = lstr;
    const char *next;
    n = 0;
    while ((next = strchr (iter, ',')))
    {
      config->service.labels[n++] = strndup (iter, next - iter);
    }
    config->service.labels[n++] = strdup (iter);
    config->service.labels[n] = NULL;
  }
  else
  {
    config->service.labels = malloc (sizeof (char *));
    config->service.labels[0] = NULL;
  }

  config->device.datatransform = iot_data_bool (iot_data_string_map_get (map, DYN_PREFIX "Device/DataTransform"));
  config->device.discovery_enabled = iot_data_bool (iot_data_string_map_get (map, DYN_PREFIX "Device/Discovery/Enabled"));
  config->device.discovery_interval = iot_data_ui32 (iot_data_string_map_get (map, DYN_PREFIX "Device/Discovery/Interval"));
  config->device.maxcmdops = iot_data_ui32 (iot_data_string_map_get (map, DYN_PREFIX "Device/MaxCmdOps"));
  config->device.maxcmdresultlen = iot_data_ui32 (iot_data_string_map_get (map, DYN_PREFIX "Device/MaxCmdResultLen"));
  config->device.profilesdir = iot_data_string_map_get_string (map, "Device/ProfilesDir");
  config->device.devicesdir = iot_data_string_map_get_string (map, "Device/DevicesDir");
  config->device.updatelastconnected = iot_data_bool (iot_data_string_map_get (map, DYN_PREFIX "Device/UpdateLastConnected"));
  config->device.eventqlen = iot_data_ui32 (iot_data_string_map_get (map, "Device/EventQLength"));
}

void edgex_device_populateConfig (devsdk_service_t *svc, iot_data_t *config)
{
  iot_data_map_iter_t iter;

  svc->config.sdkconf = config;
  svc->config.driverconf = iot_data_alloc_map (IOT_DATA_STRING);

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

  edgex_device_populateConfigFromMap (&svc->config, config);

  edgex_config_setloglevel (svc->logger, iot_data_string_map_get_string (config, DYN_PREFIX "LogLevel"), &svc->config.loglevel);
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
  if (updatedriver)
  {
    svc->userfns.reconfigure (svc->userdata, svc->config.driverconf);
  }
}

void edgex_device_dumpConfig (iot_logger_t *lc, iot_data_t *config)
{
  char *val;
  iot_data_map_iter_t iter;
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

  free (svc->config.endpoints.data.host);
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
  json_object_set_uint
    (dobj, "MaxCmdResultLen", svc->config.device.maxcmdresultlen);
  json_object_set_string (dobj, "ProfilesDir", svc->config.device.profilesdir);
  json_object_set_string (dobj, "DevicesDir", svc->config.device.devicesdir);
  json_object_set_boolean
    (dobj, "UpdateLastConnected", svc->config.device.updatelastconnected);
  json_object_set_uint (dobj, "EventQLength", svc->config.device.eventqlen);
  json_object_set_value (wobj, "Device", dval);

  json_object_set_value (obj, DYN_NAME, wval);

  const char *mqtype = iot_data_string_map_get_string (svc->config.sdkconf, EX_MQ_TYPE);
  if (strcmp (mqtype, "mqtt") == 0)
  {
    JSON_Value *mqval = edgex_mqtt_config_json (svc->config.sdkconf);
    json_object_set_string (json_value_get_object (mqval), "Type", mqtype);
    json_object_set_value (obj, "MessageQueue", mqval);
  }
  else if (strcmp (mqtype, "redis") == 0)
  {
    JSON_Value *mqval = edgex_redstr_config_json (svc->config.sdkconf);
    json_object_set_string (json_value_get_object (mqval), "Type", mqtype);
    json_object_set_value (obj, "MessageQueue", mqval);
  }

  JSON_Value *cval = json_value_init_object ();
  JSON_Object *cobj = json_value_get_object (cval);

  JSON_Value *mval = json_value_init_object ();
  JSON_Object *mobj = json_value_get_object (mval);
  json_object_set_string (mobj, "Host", svc->config.endpoints.metadata.host);
  json_object_set_uint (mobj, "Port", svc->config.endpoints.metadata.port);
  json_object_set_value (cobj, "Metadata", mval);

  dval = json_value_init_object ();
  dobj = json_value_get_object (dval);
  json_object_set_string (dobj, "Host", svc->config.endpoints.data.host);
  json_object_set_uint (dobj, "Port", svc->config.endpoints.data.port);
  json_object_set_value (cobj, "Data", dval);

  json_object_set_value (obj, "Clients", cval);

  JSON_Value *sval = json_value_init_object ();
  JSON_Object *sobj = json_value_get_object (sval);
  json_object_set_string (sobj, "Host", svc->config.service.host);
  json_object_set_uint (sobj, "Port", svc->config.service.port);

  uint64_t tm = svc->config.service.timeout.tv_nsec / 1000000;
  tm += svc->config.service.timeout.tv_sec * 1000;
  json_object_set_uint (sobj, "Timeout", tm);
  json_object_set_uint
    (sobj, "ConnectRetries", svc->config.service.connectretries);
  json_object_set_string (sobj, "StartupMsg", svc->config.service.startupmsg);
  json_object_set_string
    (sobj, "CheckInterval", svc->config.service.checkinterval);
  json_object_set_string (sobj, "ServerBindAddr", svc->config.service.bindaddr);
  json_object_set_uint (sobj, "MaxRequestSize", svc->config.service.maxreqsz);
  json_object_set_boolean (sobj, "UseMessageBus", iot_data_string_map_get_bool (svc->config.sdkconf, "Service/UseMessageBus", false));

  JSON_Value *lval = json_value_init_array ();
  JSON_Array *larr = json_value_get_array (lval);
  for (int i = 0; svc->config.service.labels[i]; i++)
  {
    json_array_append_string (larr, svc->config.service.labels[i]);
  }
  json_object_set_value (sobj, "Labels", lval);

  json_object_set_value (obj, "Service", sval);

  if (svc->config.driverconf)
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
  edgex_configresponse *cr = malloc (sizeof (edgex_configresponse));

  edgex_baseresponse_populate ((edgex_baseresponse *)cr, "v2", MHD_HTTP_OK, NULL);
  cr->config = edgex_device_config_toJson ((devsdk_service_t *)ctx);

  edgex_configresponse_write (cr, reply);
  edgex_configresponse_free (cr);
}

void edgex_device_process_configured_devices
  (devsdk_service_t *svc, toml_array_t *devs, devsdk_error *err)
{
  if (devs)
  {
    char *devname;
    const char *raw;
    edgex_device *existing;
    char *profile_name;
    char *description;
    devsdk_protocols *protocols;
    edgex_device_autoevents *autos;
    devsdk_strings *labels;
    toml_table_t *table;
    toml_table_t *aetable;
    toml_table_t *pptable;
    toml_array_t *arr;
    int n = 0;

    while ((table = toml_table_at (devs, n++)))
    {
      raw = toml_raw_in (table, "Name");
      toml_rtos2 (raw, &devname);
      existing = edgex_devmap_device_byname (svc->devices, devname);
      if (existing)
      {
        edgex_device_release (existing);
        iot_log_info (svc->logger, "Device %s already exists: skipped", devname);
      }
      else
      {
        /* Protocols */

        pptable = toml_table_in (table, "Protocols");
        if (pptable)
        {
          const char *key;
          protocols = NULL;
          for (int i = 0; 0 != (key = toml_key_in (pptable, i)); i++)
          {
            toml_table_t *pprops = toml_table_in (pptable, key);
            if (pprops)
            {
              devsdk_nvpairs *props = NULL;
              const char *pkey;
              for (int j = 0; 0 != (pkey = toml_key_in (pprops, j)); j++)
              {
                raw = toml_raw_in (pprops, pkey);
                if (raw)
                {
                  char *val;
                  if (toml_rtos (raw, &val) == -1)
                  {
                    val = strdup (raw);
                  }
                  props = devsdk_nvpairs_new (pkey, val, props);
                  free (val);
                }
              }
              protocols = devsdk_protocols_new (key, props, protocols);
              devsdk_nvpairs_free (props);
            }
            else
            {
              iot_log_error (svc->logger, "Arrays and subtables not supported in Protocol");
              *err = EDGEX_BAD_CONFIG;
              return;
            }
          }

          /* AutoEvents */

          autos = NULL;
          arr = toml_array_in (table, "AutoEvents");
          if (arr)
          {
            int i = 0;
            while ((aetable = toml_table_at (arr, i++)))
            {
              edgex_device_autoevents *newauto =
                calloc (1, sizeof (edgex_device_autoevents));
              toml_rtos2
                (toml_raw_in (aetable, "Resource"), &newauto->resource);
              toml_rtos2
                (toml_raw_in (aetable, "Frequency"), &newauto->frequency);
              toml_rtob2
                (toml_raw_in (aetable, "OnChange"), &newauto->onChange);
              newauto->next = autos;
              autos = newauto;
            }
          }

          /* The rest of the device */

          labels = NULL;
          profile_name = NULL;
          description = NULL;
          raw = toml_raw_in (table, "Profile");
          toml_rtos2 (raw, &profile_name);
          raw = toml_raw_in (table, "Description");
          toml_rtos2 (raw, &description);
          arr = toml_array_in (table, "Labels");
          if (arr)
          {
            for (int n = 0; (raw = toml_raw_at (arr, n)); n++)
            {
              char *l = NULL;
              toml_rtos2 (raw, &l);
              labels = devsdk_strings_new (l, labels);
            }
          }

          *err = EDGEX_OK;
          edgex_add_device (svc, devname, description, labels, profile_name, protocols, false, autos, err);

          devsdk_strings_free (labels);
          devsdk_protocols_free (protocols);
          edgex_device_autoevents_free (autos);
          free (profile_name);
          free (description);

          if (err->code)
          {
            iot_log_error (svc->logger, "Error registering device %s", devname);
            free (devname);
            break;
          }
        }
        else
        {
          iot_log_error
            (svc->logger, "No Protocols section for device %s", devname);
          *err = EDGEX_BAD_CONFIG;
          free (devname);
          break;
        }
      }
      free (devname);
    }
  }
}
