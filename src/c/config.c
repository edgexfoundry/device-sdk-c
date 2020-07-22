/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#define DRV_PREFIX "Driver/"
#define DRV_PREFIXLEN (sizeof (DRV_PREFIX) - 1)

#include "config.h"
#include "service.h"
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

iot_data_t *edgex_config_defaults (const char *dflprofiledir, const iot_data_t *driverconf)
{
  struct utsname utsbuffer;
  uname (&utsbuffer);

  iot_data_t *result = iot_data_alloc_map (IOT_DATA_STRING);

  iot_data_string_map_add (result, "Service/Host", iot_data_alloc_string (utsbuffer.nodename, IOT_DATA_COPY));
  iot_data_string_map_add (result, "Service/Port", iot_data_alloc_ui16 (49999));
  iot_data_string_map_add (result, "Service/Timeout", iot_data_alloc_ui32 (0));
  iot_data_string_map_add (result, "Service/ConnectRetries", iot_data_alloc_ui32 (0));
  iot_data_string_map_add (result, "Service/StartupMsg", iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (result, "Service/CheckInterval", iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (result, "Service/Labels", iot_data_alloc_string ("", IOT_DATA_REF));
  iot_data_string_map_add (result, "Service/ServerBindAddr", iot_data_alloc_string ("0.0.0.0", IOT_DATA_REF));

  iot_data_string_map_add (result, "Device/DataTransform", iot_data_alloc_bool (true));
  iot_data_string_map_add (result, "Device/Discovery/Enabled", iot_data_alloc_bool (true));
  iot_data_string_map_add (result, "Device/Discovery/Interval", iot_data_alloc_ui32 (0));
  iot_data_string_map_add (result, "Device/MaxCmdOps", iot_data_alloc_ui32 (0));
  iot_data_string_map_add (result, "Device/MaxCmdResultLen", iot_data_alloc_ui32 (0));
  iot_data_string_map_add (result, "Device/ProfilesDir", iot_data_alloc_string (dflprofiledir, IOT_DATA_COPY));
  iot_data_string_map_add (result, "Device/UpdateLastConnected", iot_data_alloc_bool (false));
  iot_data_string_map_add (result, "Device/EventQLength", iot_data_alloc_ui32 (0));

  iot_data_string_map_add (result, "Logging/LogLevel", iot_data_alloc_string ("WARNING", IOT_DATA_REF));

  if (driverconf)
  {
    iot_data_map_iter_t iter;
    iot_data_map_iter (driverconf, &iter);
    while (iot_data_map_iter_next (&iter))
    {
      const char *key = iot_data_map_iter_string_key (&iter);
      char *dkey = malloc (DRV_PREFIXLEN + strlen (key) + 1);
      strcpy (dkey, DRV_PREFIX);
      strcat (dkey, key);
      iot_data_map_add (result, iot_data_alloc_string (dkey, IOT_DATA_TAKE), iot_data_copy (iot_data_map_iter_value (&iter)));
    }
  }
  return result;
}

toml_table_t *edgex_device_loadConfig
(
  iot_logger_t *lc,
  const char *dir,
  const char *fname,
  const char *profile,
  devsdk_error *err
)
{
  toml_table_t *result = NULL;
  FILE *fp;
  char errbuf[ERRBUFSZ];
  char *filename;
  int pathlen;

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
  filename = malloc (pathlen);
  strcpy (filename, dir);
  strcat (filename, "/");
  if (fname && *fname)
  {
    strcat (filename, fname);
  }
  else
  {
    strcat (filename, "configuration");
    if (profile && *profile)
    {
      strcat (filename, "-");
      strcat (filename, profile);
    }
    strcat (filename, ".toml");
  }

  fp = fopen (filename, "r");
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
    iot_log_error
      (lc, "Cant open file %s : %s", filename, strerror (errno));
    *err = EDGEX_NO_CONF_FILE;
  }

  free (filename);
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

/* Recursively parse a toml table into name-value pairs. Note that the toml parser does various string manipulations,
   so reading a raw value into an int or double, then sprintf-ing it back to a string is not necessarily redundant. */

static devsdk_nvpairs *processTable (toml_table_t *config, devsdk_nvpairs *result, const char *prefix)
{
  unsigned i = 0;
  const char *key;
  const char *raw;
  toml_table_t *tab;
  int64_t dummyi;
  double dummyd;
  char *fullname;

  if (strcmp (prefix, "Clients") == 0)
  {
    return result;   // clients table is handled in parseTomlClients()
  }

  while ((key = toml_key_in (config, i++)))
  {
    if (strlen (prefix))
    {
      fullname = malloc (strlen (prefix) + strlen (key) + 2);
      strcpy (fullname, prefix);
      strcat (fullname, "/");
      strcat (fullname, key);
    }
    else
    {
      fullname = strdup (key);
    }
    if ((raw = toml_raw_in (config, key)))
    {
      if (strcmp (raw, "true") == 0 || strcmp (raw, "false") == 0)
      {
        result = devsdk_nvpairs_new (fullname, raw, result);
      }
      else if (toml_rtoi (raw, &dummyi) == 0)
      {
        char val[32];
        sprintf (val, "%" PRIi64, dummyi);
        result = devsdk_nvpairs_new (fullname, val, result);
      }
      else if (toml_rtod (raw, &dummyd) == 0)
      {
        char val[32];
        sprintf (val, "%f", dummyd);
        result = devsdk_nvpairs_new (fullname, val, result);
      }
      else
      {
        char *val = NULL;
        toml_rtos (raw, &val);
        if (val && *val)
        {
          result = devsdk_nvpairs_new (fullname, val, result);
        }
        free (val);
      }
    }
    else if ((tab = toml_table_in (config, key)))
    {
      result = processTable (tab, result, fullname);
    }
    free (fullname);
  }
  return result;
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

void edgex_device_parseTomlClients
  (iot_logger_t *lc, toml_table_t *clients, edgex_service_endpoints *endpoints, devsdk_error *err)
{
  if (clients)
  {
    parseClient (lc, toml_table_in (clients, "Data"), &endpoints->data, err);
    parseClient (lc, toml_table_in (clients, "Metadata"), &endpoints->metadata, err);
  }
}

static char *checkOverride (char *qstr)
{
  char *env;
  char *slash = qstr;

  while ((slash = strchr (slash, '/')))
  {
    *slash = '_';
  }
  env = getenv (qstr);
  if (env == NULL)
  {
    for (char *c = qstr; *c; c++)
    {
      if (islower (*c))
      {
        *c = toupper (*c);
      }
    }
    env = getenv (qstr);
  }
  return env;
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

void edgex_device_overrideConfig_toml (iot_data_t *config, toml_table_t *toml, bool v1compat)
{
  char *key;
  const char *raw;
  iot_data_map_iter_t iter;

  if (v1compat)
  {
    // Add placeholder defaults for [Driver] configuration
    devsdk_nvpairs *allconf = processTable (toml, NULL, "");
    for (const devsdk_nvpairs *iter = allconf; iter; iter = iter->next)
    {
      if (strncmp (iter->name, DRV_PREFIX, DRV_PREFIXLEN) == 0)
      {
        iot_data_map_add (config, iot_data_alloc_string (iter->name, IOT_DATA_COPY), iot_data_alloc_string ("", IOT_DATA_REF));
      }
    }
    devsdk_nvpairs_free (allconf);
  }

  iot_data_map_iter (config, &iter);
  while (iot_data_map_iter_next (&iter))
  {
    key = strdup (iot_data_map_iter_string_key (&iter));
    raw = findEntry (key, toml);
    if (raw)
    {
      iot_data_t *newval;
      if (iot_data_type (iot_data_map_iter_value (&iter)) == IOT_DATA_STRING)
      {
        char *newtxt;
        toml_rtos (raw, &newtxt);
        newval = iot_data_alloc_string (newtxt, IOT_DATA_TAKE);
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

void edgex_device_overrideConfig_env (iot_logger_t *lc, const char *sname, iot_data_t *config)
{
  char *query = NULL;
  size_t qsize = 0;
  const char *key;
  iot_data_map_iter_t iter;
  size_t extra = strlen (sname) + 2;

  iot_data_map_iter (config, &iter);
  while (iot_data_map_iter_next (&iter))
  {
    key = iot_data_map_iter_string_key (&iter);
    size_t req = strlen (key) + extra;
    if (qsize < req)
    {
      query = realloc (query, req);
      qsize = req;
    }
    strcpy (query, sname);
    strcat (query, "_");
    strcat (query, key);
    char *newtxt = checkOverride (query);
    if (newtxt == NULL)
    {
      for (int i = 0; i < strlen (sname); i++)
      {
        query[i] = (sname[i] == '-') ? '_' : sname[i];
      }
      newtxt = checkOverride (query);
    }
    if (newtxt == NULL)
    {
      strcpy (query, key);
      newtxt = checkOverride (query);
    }
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

  config->device.datatransform = iot_data_bool (iot_data_string_map_get (map, "Device/DataTransform"));
  config->device.discovery_enabled = iot_data_bool (iot_data_string_map_get (map, "Device/Discovery/Enabled"));
  config->device.discovery_interval = iot_data_ui32 (iot_data_string_map_get (map, "Device/Discovery/Interval"));
  config->device.maxcmdops = iot_data_ui32 (iot_data_string_map_get (map, "Device/MaxCmdOps"));
  config->device.maxcmdresultlen = iot_data_ui32 (iot_data_string_map_get (map, "Device/MaxCmdResultLen"));
  config->device.profilesdir = iot_data_string_map_get_string (map, "Device/ProfilesDir");
  config->device.updatelastconnected = iot_data_bool (iot_data_string_map_get (map, "Device/UpdateLastConnected"));
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
  }

  edgex_device_populateConfigFromMap (&svc->config, config);

  edgex_config_setloglevel (svc->logger, iot_data_string_map_get_string (config, "Logging/LogLevel"), &svc->config.logging.level);
}

void edgex_device_updateConf (void *p, const devsdk_nvpairs *config)
{
  iot_data_map_iter_t iter;
  bool updatedriver = false;
  devsdk_service_t *svc = (devsdk_service_t *)p;

  edgex_device_overrideConfig_nvpairs (svc->config.sdkconf, config);
  edgex_device_populateConfigFromMap (&svc->config, svc->config.sdkconf);

  const char *lname = devsdk_nvpairs_value (config, "Writable/LogLevel");
  if (lname == NULL)
  {
    lname = devsdk_nvpairs_value (config, "Logging/LogLevel");
  }
  if (lname)
  {
    edgex_config_setloglevel (svc->logger, lname, &svc->config.logging.level);
  }

  iot_data_map_iter (svc->config.sdkconf, &iter);
  while (iot_data_map_iter_next (&iter))
  {
    const char *key = iot_data_map_iter_string_key (&iter);
    if (strncmp (key, DRV_PREFIX, DRV_PREFIXLEN) == 0)
    {
      const iot_data_t *driverval = iot_data_string_map_get (svc->config.driverconf, key + DRV_PREFIXLEN);
      if (driverval && !iot_data_equal (driverval, iot_data_map_iter_value (&iter)))
      {
        updatedriver = true;
        iot_data_map_add (svc->config.driverconf, iot_data_alloc_string (key + DRV_PREFIXLEN, IOT_DATA_COPY), iot_data_copy (iot_data_map_iter_value (&iter)));
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

static char *edgex_device_serialize_config (devsdk_service_t *svc)
{
  JSON_Value *val = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (val);

  JSON_Value *cval = json_value_init_object ();
  JSON_Object *cobj = json_value_get_object (cval);

  JSON_Value *mval = json_value_init_object ();
  JSON_Object *mobj = json_value_get_object (mval);
  json_object_set_string (mobj, "Host", svc->config.endpoints.metadata.host);
  json_object_set_uint (mobj, "Port", svc->config.endpoints.metadata.port);
  json_object_set_value (cobj, "Metadata", mval);

  JSON_Value *dval = json_value_init_object ();
  JSON_Object *dobj = json_value_get_object (dval);
  json_object_set_string (dobj, "Host", svc->config.endpoints.data.host);
  json_object_set_uint (dobj, "Port", svc->config.endpoints.data.port);
  json_object_set_value (cobj, "Data", dval);

  json_object_set_value (obj, "Clients", cval);

  JSON_Value *lval = json_value_init_object ();
  JSON_Object *lobj = json_value_get_object (lval);
  json_object_set_string (lobj, "LogLevel", edgex_logger_levelname (svc->config.logging.level));
  json_object_set_value (obj, "Logging", lval);

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

  lval = json_value_init_array ();
  JSON_Array *larr = json_value_get_array (lval);
  for (int i = 0; svc->config.service.labels[i]; i++)
  {
    json_array_append_string (larr, svc->config.service.labels[i]);
  }
  json_object_set_value (sobj, "Labels", lval);

  json_object_set_value (obj, "Service", sval);

  dval = json_value_init_object ();
  dobj = json_value_get_object (dval);

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
  json_object_set_boolean
    (dobj, "UpdateLastConnected", svc->config.device.updatelastconnected);
  json_object_set_uint (dobj, "EventQLength", svc->config.device.eventqlen);
  json_object_set_value (obj, "Device", dval);

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
    json_object_set_value (obj, "Driver", dval);
  }

  char *result = json_serialize_to_string (val);
  json_value_free (val);
  return result;
}

void edgex_device_handler_config (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  char *json = edgex_device_serialize_config ((devsdk_service_t *)ctx);
  reply->data.bytes = json;
  reply->data.size = strlen (json);
  reply->content_type = CONTENT_JSON;
  reply->code = MHD_HTTP_OK;
}

void edgex_device_handler_configv2 (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  edgex_baserequest *br;
  edgex_configresponse *cr = malloc (sizeof (edgex_configresponse));

  br = edgex_baserequest_read (req->data);
  edgex_baseresponse_populate ((edgex_baseresponse *)cr, br->requestId, MHD_HTTP_OK, NULL);
  cr->config = edgex_device_serialize_config ((devsdk_service_t *)ctx);

  edgex_configresponse_write (cr, reply);
  edgex_configresponse_free (cr);
  edgex_baserequest_free (br);
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
    devsdk_strings *newlabel;
    toml_table_t *table;
    toml_table_t *aetable;
    toml_table_t *pptable;
    toml_array_t *arr;
    int n = 0;

    iot_log_info (svc->logger, "Processing DeviceList from configuration");
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
              newlabel = malloc (sizeof (devsdk_strings));
              newlabel->next = labels;
              toml_rtos2 (raw, &newlabel->str);
              labels = newlabel;
            }
          }

          *err = EDGEX_OK;
          free (edgex_add_device (svc, devname, description, labels, profile_name, protocols, false, autos, err));

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
