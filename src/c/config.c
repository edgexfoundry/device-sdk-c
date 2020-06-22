/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "config.h"
#include "service.h"
#include "errorlist.h"
#include "edgex-rest.h"
#include "devutil.h"
#include "autoevent.h"
#include "edgex/devices.h"

#include <microhttpd.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* TODO: Use iot_data_t maps as the "native" representation throughout */

#define ERRBUFSZ 1024
#define DEFAULTREG "consul.http://localhost:8500"

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

devsdk_nvpairs *edgex_device_parseToml (toml_table_t *config)
{
  return processTable (config, NULL, "");
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
    parseClient (lc, toml_table_in (clients, "Logging"), &endpoints->logging, err);
  }
}

static bool checkOverride (char *qstr, char **val)
{
  char *env;
  char *slash = qstr;

  while ((slash = strchr (slash, '/')))
  {
    *slash = '_';
  }
  env = getenv (qstr);
  if (env)
  {
    free (*val); 
    *val = strdup (env);
    return true;
  }

  for (char *c = qstr; *c; c++)
  {
    if (islower (*c))
    {
      *c = toupper (*c);
    }
  }
  env = getenv (qstr);
  if (env)
  {
    free (*val);
    *val = strdup (env);
    return true;
  }

  return false;
}

void edgex_device_overrideConfig (iot_logger_t *lc, const char *sname, devsdk_nvpairs *config)
{
  char *query = NULL;
  size_t qsize = 0;

  for (devsdk_nvpairs *iter = config; iter; iter = iter->next)
  {
    size_t req = strlen (iter->name) + strlen (sname) + 2;
    if (qsize < req)
    {
      query = realloc (query, req);
      qsize = req;
    }
    strcpy (query, sname);
    strcat (query, "_");
    strcat (query, iter->name);
    if (checkOverride (query, &iter->value))
    {
      iot_log_info (lc, "Override config %s = %s", iter->name, iter->value);
    }
    else
    {
      for (int i = 0; i < strlen (sname); i++)
      {
        query[i] = (sname[i] == '-') ? '_' : sname[i];
      }
      if (checkOverride (query, &iter->value))
      {
        iot_log_info (lc, "Override config %s = %s", iter->name, iter->value);
      }
      else
      {
        strcpy (query, iter->name);
        if (checkOverride (query, &iter->value))
        {
          iot_log_info (lc, "Override config %s = %s", iter->name, iter->value);
        }
      }
    }
  }
  free (query);
}

#if 0
void edgex_device_process_configured_watchers
  (devsdk_service_t *svc, toml_array_t *watchers, devsdk_error *err)
{
  const char *raw;
  char *namestr;
  toml_table_t *table;

  if (watchers)
  {
    int n = 0;
    while ((table = toml_table_at (watchers, n++)))
    {
      edgex_device_watcherinfo watcher;
      watcher.profile = NULL;
      watcher.key = NULL;
      watcher.matchstring = NULL;
      namestr = NULL;
      toml_rtos2 (toml_raw_in (table, "Name"), &namestr);
      toml_rtos2 (toml_raw_in (table, "DeviceProfile"), &watcher.profile);
      toml_rtos2 (toml_raw_in (table, "Key"), &watcher.key);
      toml_rtos2 (toml_raw_in (table, "MatchString"), &watcher.matchstring);
      int n = 0;
      toml_array_t *arr = toml_array_in (table, "Identifiers");
      if (arr)
      {
        while (toml_raw_at (arr, n))
        { n++; }
      }
      watcher.ids = malloc (sizeof (char *) * (n + 1));
      for (int j = 0; j < n; j++)
      {
        raw = toml_raw_at (arr, j);
        toml_rtos2 (raw, &watcher.ids[j]);
      }
      watcher.ids[n] = NULL;
      if (namestr && watcher.profile && watcher.key)
      {
        edgex_map_set (&svc->config.watchers, namestr, watcher);
      }
      else
      {
        iot_log_error
        (
          svc->logger,
          "Watcher: Name, DeviceProfile and Key must be configured"
        );
        free (watcher.profile);
        free (watcher.key);
        free (watcher.matchstring);
        if (watcher.ids)
        {
          for (int n = 0; watcher.ids[n]; n++)
          {
            free (watcher.ids[n]);
          }
          free (watcher.ids);
        }
        *err = EDGEX_BAD_CONFIG;
      }
    }
  }
}
#endif

static char *get_nv_config_string
  (const devsdk_nvpairs *config, const char *key)
{
  for (const devsdk_nvpairs *iter = config; iter; iter = iter->next)
  {
    if (strcasecmp (iter->name, key) == 0)
    {
      return strdup (iter->value);
    }
  }
  return NULL;
}

static uint32_t get_nv_config_uint32
(
  iot_logger_t *lc,
  const devsdk_nvpairs *config,
  const char *key,
  devsdk_error *err
)
{
  for (const devsdk_nvpairs *iter = config; iter; iter = iter->next)
  {
    if (strcasecmp (iter->name, key) == 0)
    {
      unsigned long long tmp;
      errno = 0;
      tmp = strtoull (iter->value, NULL, 0);
      if (errno == 0 && tmp >= 0 && tmp <= UINT32_MAX)
      {
        return tmp;
      }
      else
      {
        *err = EDGEX_BAD_CONFIG;
        iot_log_error (lc, "Unable to parse %s as uint32", iter->value);
        return 0;
      }
    }
  }
  return 0;
}

static uint16_t get_nv_config_uint16
(
  iot_logger_t *lc,
  const devsdk_nvpairs *config,
  const char *key,
  devsdk_error *err
)
{
  for (const devsdk_nvpairs *iter = config; iter; iter = iter->next)
  {
    if (strcasecmp (iter->name, key) == 0)
    {
      unsigned long long tmp;
      errno = 0;
      tmp = strtoull (iter->value, NULL, 0);
      if (errno == 0 && tmp >= 0 && tmp <= UINT16_MAX)
      {
        return tmp;
      }
      else
      {
        *err = EDGEX_BAD_CONFIG;
        iot_log_error (lc, "Unable to parse %s as uint16", iter->value);
        return 0;
      }
    }
  }
  return 0;
}

static bool get_nv_config_bool
  (const devsdk_nvpairs *config, const char *key, bool dfl)
{
  for (const devsdk_nvpairs *iter = config; iter; iter = iter->next)
  {
    if (strcasecmp (iter->name, key) == 0)
    {
      return (strcasecmp (iter->value, "true") == 0);
    }
  }
  return dfl;
}

void edgex_device_populateConfig
  (devsdk_service_t *svc, const devsdk_nvpairs *config, devsdk_error *err)
{
  svc->config.service.host = get_nv_config_string (config, "Service/Host");
  svc->config.service.port =
    get_nv_config_uint16 (svc->logger, config, "Service/Port", err);
  uint32_t tm =
    get_nv_config_uint32 (svc->logger, config, "Service/Timeout", err);
  svc->config.service.timeout.tv_sec = tm / 1000;
  svc->config.service.timeout.tv_nsec = 1000000 * (tm % 1000);
  svc->config.service.connectretries =
    get_nv_config_uint32 (svc->logger, config, "Service/ConnectRetries", err);
  svc->config.service.startupmsg =
    get_nv_config_string (config, "Service/StartupMsg");
  svc->config.service.checkinterval =
    get_nv_config_string (config, "Service/CheckInterval");

  char *lstr = get_nv_config_string (config, "Service/Labels");
  if (lstr)
  {
    char *iter = lstr;
    int n = 1;
    while ((iter = strchr (iter, ',')))
    {
      iter++;
      n++;
    }
    svc->config.service.labels = malloc (sizeof (char *) * (n + 1));

    char *ctx;
    n = 0;
    iter = strtok_r (lstr, ",", &ctx);
    do
    {
      svc->config.service.labels[n++] = strdup (iter);
      iter = strtok_r (NULL, ",", &ctx);
    } while (iter);
    svc->config.service.labels[n] = NULL;
    free (lstr);
  }
  else
  {
    svc->config.service.labels = malloc (sizeof (char *));
    svc->config.service.labels[0] = NULL;
  }

  svc->config.device.datatransform =
    get_nv_config_bool (config, "Device/DataTransform", true);
  svc->config.device.discovery_enabled = get_nv_config_bool (config, "Device/Discovery/Enabled", true);
  svc->config.device.discovery_interval = get_nv_config_uint32 (svc->logger, config, "Device/Discovery/Interval", err);
  svc->config.device.initcmd = get_nv_config_string (config, "Device/InitCmd");
  svc->config.device.initcmdargs =
    get_nv_config_string (config, "Device/InitCmdArgs");
  svc->config.device.maxcmdops =
    get_nv_config_uint32 (svc->logger, config, "Device/MaxCmdOps", err);
  svc->config.device.maxcmdresultlen =
    get_nv_config_uint32 (svc->logger, config, "Device/MaxCmdResultLen", err);
  svc->config.device.removecmd =
    get_nv_config_string (config, "Device/RemoveCmd");
  svc->config.device.removecmdargs =
    get_nv_config_string (config, "Device/RemoveCmdArgs");
  svc->config.device.profilesdir =
    get_nv_config_string (config, "Device/ProfilesDir");
  svc->config.device.sendreadingsonchanged =
    get_nv_config_bool (config, "Device/SendReadingsOnChanged", false);
  svc->config.device.updatelastconnected =
    get_nv_config_bool (config, "Device/UpdateLastConnected", false);
  svc->config.device.eventqlen =
    get_nv_config_uint32 (svc->logger, config, "Device/EventQLength", err);

  svc->config.driverconf = iot_data_alloc_map (IOT_DATA_STRING);
  for (const devsdk_nvpairs *iter = config; iter; iter = iter->next)
  {
    if (strncmp (iter->name, "Driver/", strlen ("Driver/")) == 0)
    {
      iot_data_map_add
        (svc->config.driverconf, iot_data_alloc_string (iter->name + strlen ("Driver/"), IOT_DATA_COPY), iot_data_alloc_string (iter->value, IOT_DATA_COPY));
    }
  }

  svc->config.logging.useremote =
    get_nv_config_bool (config, "Logging/EnableRemote", false);
  svc->config.logging.file = get_nv_config_string (config, "Logging/File");

  edgex_device_updateConf (svc, config);
}

void edgex_device_updateConf (void *p, const devsdk_nvpairs *config)
{
  devsdk_service_t *svc = (devsdk_service_t *)p;
  char *lname = get_nv_config_string (config, "Writable/LogLevel");
  if (lname == NULL)
  {
    lname = get_nv_config_string (config, "Logging/LogLevel");
  }
  if (lname)
  {
    edgex_config_setloglevel (svc->logger, lname, &svc->config.logging.level);
    free (lname);
  }
}

void edgex_device_freeConfig (devsdk_service_t *svc)
{
  edgex_device_watcherinfo *watcher;
  edgex_map_iter iter;
  const char *key;

  free (svc->config.endpoints.data.host);
  free (svc->config.endpoints.metadata.host);
  free (svc->config.endpoints.logging.host);
  free (svc->config.logging.file);
  free (svc->config.service.host);
  free (svc->config.service.startupmsg);
  free (svc->config.service.checkinterval);
  free (svc->config.device.initcmd);
  free (svc->config.device.initcmdargs);
  free (svc->config.device.removecmd);
  free (svc->config.device.removecmdargs);
  free (svc->config.device.profilesdir);

  if (svc->config.service.labels)
  {
    for (int i = 0; svc->config.service.labels[i]; i++)
    {
      free (svc->config.service.labels[i]);
    }
    free (svc->config.service.labels);
  }

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

void edgex_device_handler_config (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *)ctx;

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

  JSON_Value *lsval = json_value_init_object ();
  JSON_Object *lsobj = json_value_get_object (lsval);
  json_object_set_string (lsobj, "Host", svc->config.endpoints.logging.host);
  json_object_set_uint (lsobj, "Port", svc->config.endpoints.logging.port);
  json_object_set_value (cobj, "Data", lsval);

  json_object_set_value (obj, "Clients", cval);

  JSON_Value *lval = json_value_init_object ();
  JSON_Object *lobj = json_value_get_object (lval);
  json_object_set_string (lobj, "File", svc->config.logging.file);
  json_object_set_boolean (lobj, "EnableRemote", svc->config.logging.useremote);
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
  json_object_set_string (dobj, "InitCmd", svc->config.device.initcmd);
  json_object_set_string (dobj, "InitCmdArgs", svc->config.device.initcmdargs);
  json_object_set_uint (dobj, "MaxCmdOps", svc->config.device.maxcmdops);
  json_object_set_uint
    (dobj, "MaxCmdResultLen", svc->config.device.maxcmdresultlen);
  json_object_set_string (dobj, "RemoveCmd", svc->config.device.removecmd);
  json_object_set_string
    (dobj, "RemoveCmdArgs", svc->config.device.removecmdargs);
  json_object_set_string (dobj, "ProfilesDir", svc->config.device.profilesdir);
  json_object_set_boolean
    (dobj, "SendReadingsOnChanged", svc->config.device.sendreadingsonchanged);
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

  char *json = json_serialize_to_string (val);
  json_value_free (val);

  reply->data.bytes = json;
  reply->data.size = strlen (json);
  reply->content_type = CONTENT_JSON;
  reply->code = MHD_HTTP_OK;
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
                calloc (sizeof (edgex_device_autoevents), 1);
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
