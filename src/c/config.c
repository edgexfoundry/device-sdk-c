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
#include "autoevent.h"
#include "edgex/device-mgmt.h"

#include <microhttpd.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void toml_rtos2 (const char *s, char **ret);

#define ERRBUFSZ 1024

toml_table_t *edgex_device_loadConfig
(
  iot_logger_t *lc,
  const char *dir,
  const char *profile,
  edgex_error *err
)
{
  toml_table_t *result = NULL;
  FILE *fp;
  char errbuf[ERRBUFSZ];
  char *filename;

  int pathlen = strlen (dir) + 1 + strlen ("configuration.toml") + 1;
  if (profile && *profile)
  {
    pathlen += (strlen (profile) + 1);
  }
  filename = malloc (pathlen);
  strcpy (filename, dir);
  strcat (filename, "/");
  strcat (filename, "configuration");
  if (profile && *profile)
  {
    strcat (filename, "-");
    strcat (filename, profile);
  }
  strcat (filename, ".toml");

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
  for (l = TRACE; l <= ERROR; l++)
  {
    if (strcasecmp (lstr, iot_logger_levelname (l)) == 0)
    {
      if (*result != l)
      {
        *result = l;
        iot_logger_setlevel (lc, INFO);
        iot_log_info (lc, "Setting LogLevel to %s", lstr);
        iot_logger_setlevel (lc, l);
      }
      break;
    }
  }
  if (l > ERROR)
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

/* Wrap toml_rtoi for uint16, uint32. */

static void toml_rtoui16
  (const char *raw, uint16_t *ret, iot_logger_t *lc, edgex_error *err)
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

static void toml_rtoui32
  (const char *raw, uint32_t *ret, iot_logger_t *lc, edgex_error *err)
{
  if (raw)
  {
    int64_t dummy;
    if (toml_rtoi (raw, &dummy) == 0 && dummy >= 0 && dummy <= UINT32_MAX)
    {
      *ret = dummy;
    }
    else
    {
      iot_log_error (lc, "Unable to parse %s as uint32", raw);
      *err = EDGEX_BAD_CONFIG;
    }
  }
}

#define GET_CONFIG_STRING(KEY, ELEMENT) \
toml_rtos2 (toml_raw_in (table, #KEY), &svc->config.ELEMENT);

#define GET_CONFIG_UINT16(KEY, ELEMENT) \
toml_rtoui16 (toml_raw_in(table, #KEY), &svc->config.ELEMENT, svc->logger, err);

#define GET_CONFIG_UINT32(KEY, ELEMENT) \
toml_rtoui32 (toml_raw_in(table, #KEY), &svc->config.ELEMENT, svc->logger, err);

#define GET_CONFIG_BOOL(KEY, ELEMENT) \
toml_rtob2 (toml_raw_in(table, #KEY), &svc->config.ELEMENT);

void edgex_device_populateConfig
  (edgex_device_service *svc, toml_table_t *config, edgex_error *err)
{
  const char *raw;
  char *namestr;
  toml_table_t *table;
  toml_table_t *subtable;
  toml_array_t *arr;

  svc->config.device.discovery = true;
  svc->config.device.datatransform = true;

  table = toml_table_in (config, "Service");
  if (table)
  {
    GET_CONFIG_STRING(Host, service.host);
    GET_CONFIG_UINT16(Port, service.port);
    GET_CONFIG_UINT32(Timeout, service.timeout);
    GET_CONFIG_UINT32(ConnectRetries, service.connectretries);
    GET_CONFIG_STRING(StartupMsg, service.startupmsg);
    GET_CONFIG_STRING(CheckInterval, service.checkinterval);
    int n = 0;
    arr = toml_array_in (table, "Labels");
    if (arr)
    {
      while (toml_raw_at (arr, n))
      { n++; }
    }
    svc->config.service.labels = malloc (sizeof (char *) * (n + 1));
    for (int i = 0; i < n; i++)
    {
      raw = toml_raw_at (arr, i);
      toml_rtos2 (raw, &svc->config.service.labels[i]);
    }
    svc->config.service.labels[n] = NULL;
  }

  subtable = toml_table_in (config, "Clients");
  if (subtable)
  {
    table = toml_table_in (subtable, "Data");
    if (table)
    {
      GET_CONFIG_STRING(Host, endpoints.data.host);
      GET_CONFIG_UINT16(Port, endpoints.data.port);
    }
    table = toml_table_in (subtable, "Metadata");
    if (table)
    {
      GET_CONFIG_STRING(Host, endpoints.metadata.host);
      GET_CONFIG_UINT16(Port, endpoints.metadata.port);
    }
    table = toml_table_in (subtable, "Logging");
    if (table)
    {
      GET_CONFIG_STRING(Host, endpoints.logging.host);
      GET_CONFIG_UINT16(Port, endpoints.logging.port);
    }
  }

  table = toml_table_in (config, "Device");
  if (table)
  {
    GET_CONFIG_BOOL(DataTransform, device.datatransform);
    GET_CONFIG_BOOL(Discovery, device.discovery);
    GET_CONFIG_STRING(InitCmd, device.initcmd);
    GET_CONFIG_STRING(InitCmdArgs, device.initcmdargs);
    GET_CONFIG_UINT32(MaxCmdOps, device.maxcmdops);
    GET_CONFIG_UINT32(MaxCmdResultLen, device.maxcmdresultlen);
    GET_CONFIG_STRING(RemoveCmd, device.removecmd);
    GET_CONFIG_STRING(RemoveCmdArgs, device.removecmdargs);
    GET_CONFIG_STRING(ProfilesDir, device.profilesdir);
    GET_CONFIG_BOOL(SendReadingsOnChanged, device.sendreadingsonchanged);
  }

  table = toml_table_in (config, "Driver");
  if (table)
  {
    const char *key;
    for (int i = 0; 0 != (key = toml_key_in (table, i)); i++)
    {
      raw = toml_raw_in (table, key);
      if (raw)
      {
        edgex_nvpairs *pair = malloc (sizeof (edgex_nvpairs));
        pair->name = strdup (key);
        if (toml_rtos (raw, &pair->value) == -1)
        {
          pair->value = strdup (raw);
        }
        pair->next = svc->config.driverconf;
        svc->config.driverconf = pair;
      }
      else
      {
        iot_log_error
          (svc->logger, "Arrays and subtables not supported in Driver table");
        *err = EDGEX_BAD_CONFIG;
        return;
      }
    }
  }

  table = toml_table_in (config, "Logging");
  if (table)
  {
    char *levelstr = NULL;
    GET_CONFIG_BOOL(EnableRemote, logging.useremote);
    GET_CONFIG_STRING(File, logging.file);
    toml_rtos2 (toml_raw_in (table, "LogLevel"), &levelstr);
    if (levelstr)
    {
      edgex_config_setloglevel (svc->logger, levelstr, &svc->config.logging.level);
      free (levelstr);
    }
  }

  arr = toml_array_in (config, "Watchers");
  if (arr)
  {
    int n = 0;
    while ((table = toml_table_at (arr, n++)))
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
      toml_array_t *arr2 = toml_array_in (table, "Identifiers");
      if (arr2)
      {
        while (toml_raw_at (arr2, n))
        { n++; }
      }
      watcher.ids = malloc (sizeof (char *) * (n + 1));
      for (int j = 0; j < n; j++)
      {
        raw = toml_raw_at (arr2, j);
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

static edgex_nvpairs *makepair
  (const char *name, const char *value, edgex_nvpairs *list)
{
  edgex_nvpairs *result = malloc (sizeof (edgex_nvpairs));
  result->name = strdup (name);
  result->value = strdup (value);
  result->next = list;
  return result;
}


static char *get_nv_config_string
  (const edgex_nvpairs *config, const char *key)
{
  for (const edgex_nvpairs *iter = config; iter; iter = iter->next)
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
  const edgex_nvpairs *config,
  const char *key,
  edgex_error *err
)
{
  for (const edgex_nvpairs *iter = config; iter; iter = iter->next)
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
  const edgex_nvpairs *config,
  const char *key,
  edgex_error *err
)
{
  for (const edgex_nvpairs *iter = config; iter; iter = iter->next)
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
  (const edgex_nvpairs *config, const char *key, bool dfl)
{
  for (const edgex_nvpairs *iter = config; iter; iter = iter->next)
  {
    if (strcasecmp (iter->name, key) == 0)
    {
      return (strcasecmp (iter->value, "true") == 0);
    }
  }
  return dfl;
}

void edgex_device_populateConfigNV
  (edgex_device_service *svc, const edgex_nvpairs *config, edgex_error *err)
{
  svc->config.service.host = get_nv_config_string (config, "Service/Host");
  svc->config.service.port =
    get_nv_config_uint16 (svc->logger, config, "Service/Port", err);
  svc->config.service.timeout =
    get_nv_config_uint32 (svc->logger, config, "Service/Timeout", err);
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
  svc->config.device.discovery =
    get_nv_config_bool (config, "Device/Discovery", true);
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

  for (const edgex_nvpairs *iter = config; iter; iter = iter->next)
  {
    if (strncmp (iter->name, "Driver/", strlen ("Driver/")) == 0)
    {
      svc->config.driverconf = makepair
        (iter->name + strlen ("Driver/"), iter->value, svc->config.driverconf);
    }
  }

  svc->config.logging.useremote =
    get_nv_config_bool (config, "Logging/EnableRemote", false);
  svc->config.logging.file = get_nv_config_string (config, "Logging/File");
}

void edgex_device_updateConf (void *p, const edgex_nvpairs *config)
{
  edgex_device_service *svc = (edgex_device_service *)p;
  char *lname = get_nv_config_string (config, "Writable/LogLevel");
  if (lname)
  {
    edgex_config_setloglevel (svc->logger, lname, &svc->config.logging.level);
    free (lname);
  }
}

#define PUT_CONFIG_STRING(X,Y) \
  if (svc->config.Y) result = makepair (#X, svc->config.Y, result)
#define PUT_CONFIG_INT(X,Y) \
  sprintf (buf, "%ld", (long)svc->config.Y); result = makepair (#X, buf, result)
#define PUT_CONFIG_UINT(X,Y) \
  sprintf (buf, "%lu", (unsigned long)svc->config.Y); result = makepair (#X, buf, result)
#define PUT_CONFIG_BOOL(X,Y) \
  result = makepair (#X, svc->config.Y ? "true" : "false", result)

edgex_nvpairs *edgex_device_getConfig (const edgex_device_service *svc)
{
  char buf[32];
  edgex_nvpairs *result = NULL;

  PUT_CONFIG_STRING(Service/Host, service.host);
  PUT_CONFIG_UINT(Service/Port, service.port);
  PUT_CONFIG_UINT(Service/Timeout, service.timeout);
  PUT_CONFIG_UINT(Service/ConnectRetries, service.connectretries);
  PUT_CONFIG_STRING(Service/StartupMsg, service.startupmsg);
  PUT_CONFIG_STRING(Service/CheckInterval, service.checkinterval);

  int labellen = 0;
  for (int i = 0; svc->config.service.labels[i]; i++)
  {
    labellen += (strlen (svc->config.service.labels[i]) + 1);
  }
  if (labellen)
  {
    char *labels = malloc (labellen);
    labels[0] = '\0';
    for (int i = 0; svc->config.service.labels[i]; i++)
    {
      if (i)
      {
        strcat (labels, ",");
      }
      strcat (labels, svc->config.service.labels[i]);
    }
    result = makepair ("Service/Labels", labels, result);
    free (labels);
  }

  PUT_CONFIG_BOOL(Device/DataTransform, device.datatransform);
  PUT_CONFIG_BOOL(Device/Discovery, device.discovery);
  PUT_CONFIG_STRING(Device/InitCmd, device.initcmd);
  PUT_CONFIG_STRING(Device/InitCmdArgs, device.initcmdargs);
  PUT_CONFIG_UINT(Device/MaxCmdOps, device.maxcmdops);
  PUT_CONFIG_UINT(Device/MaxCmdResultLen, device.maxcmdresultlen);
  PUT_CONFIG_STRING(Device/RemoveCmd, device.removecmd);
  PUT_CONFIG_STRING(Device/RemoveCmdArgs, device.removecmdargs);
  PUT_CONFIG_STRING(Device/ProfilesDir, device.profilesdir);
  PUT_CONFIG_BOOL(Device/SendReadingsOnChanged, device.sendreadingsonchanged);

  for (edgex_nvpairs *iter = svc->config.driverconf; iter; iter = iter->next)
  {
    edgex_nvpairs *pair = malloc (sizeof (edgex_nvpairs));
    pair->name = malloc (strlen (iter->name) + strlen ("Driver/") + 1);
    sprintf (pair->name, "Driver/%s", iter->name);
    pair->value = strdup (iter->value);
    pair->next = result;
    result = pair;
  }

  PUT_CONFIG_BOOL(Logging/EnableRemote, logging.useremote);
  PUT_CONFIG_STRING(Logging/File, logging.file);

  result = makepair
    ("Writable/LogLevel", iot_logger_levelname (svc->config.logging.level), result);

  return result;
}

void edgex_device_validateConfig (edgex_device_service *svc, edgex_error *err)
{
  if (svc->config.endpoints.data.host == 0)
  {
    iot_log_error (svc->logger, "config: no hostname for core-data");
    *err = EDGEX_BAD_CONFIG;
  }
  if (svc->config.endpoints.data.port == 0)
  {
    iot_log_error (svc->logger, "config: no port for core-data");
    *err = EDGEX_BAD_CONFIG;
  }
  if (svc->config.endpoints.metadata.host == 0)
  {
    iot_log_error (svc->logger, "config: no hostname for core-metadata");
    *err = EDGEX_BAD_CONFIG;
  }
  if (svc->config.endpoints.metadata.port == 0)
  {
    iot_log_error (svc->logger, "config: no port for core-metadata");
    *err = EDGEX_BAD_CONFIG;
  }
}

void edgex_device_dumpConfig (edgex_device_service *svc)
{
  edgex_nvpairs *conf;

  iot_log_debug (svc->logger, "Service configuration follows:");
  conf = edgex_device_getConfig (svc);
  for (const edgex_nvpairs *iter = conf; iter; iter = iter->next)
  {
    iot_log_debug (svc->logger, "%s=%s", iter->name, iter->value);
  }
  edgex_nvpairs_free (conf);
}

void edgex_device_freeConfig (edgex_device_service *svc)
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

  edgex_nvpairs_free (svc->config.driverconf);

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

int edgex_device_handler_config
(
  void *ctx,
  char *url,
  edgex_http_method method,
  const char *upload_data,
  size_t upload_data_size,
  void **reply,
  size_t *reply_size,
  const char **reply_type
)
{
  edgex_device_service *svc = (edgex_device_service *)ctx;

  JSON_Value *val = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (val);

  JSON_Value *cval = json_value_init_object ();
  JSON_Object *cobj = json_value_get_object (cval);

  JSON_Value *mval = json_value_init_object ();
  JSON_Object *mobj = json_value_get_object (mval);
  json_object_set_string (mobj, "Host", svc->config.endpoints.metadata.host);
  json_object_set_number (mobj, "Port", svc->config.endpoints.metadata.port);
  json_object_set_value (cobj, "Metadata", mval);

  JSON_Value *dval = json_value_init_object ();
  JSON_Object *dobj = json_value_get_object (dval);
  json_object_set_string (dobj, "Host", svc->config.endpoints.data.host);
  json_object_set_number (dobj, "Port", svc->config.endpoints.data.port);
  json_object_set_value (cobj, "Data", dval);

  JSON_Value *lsval = json_value_init_object ();
  JSON_Object *lsobj = json_value_get_object (lsval);
  json_object_set_string (lsobj, "Host", svc->config.endpoints.logging.host);
  json_object_set_number (lsobj, "Port", svc->config.endpoints.logging.port);
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
  json_object_set_number (sobj, "Port", svc->config.service.port);
  json_object_set_number (sobj, "Timeout", svc->config.service.timeout);
  json_object_set_number
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
  json_object_set_boolean
    (dobj, "DataTransform", svc->config.device.datatransform);
  json_object_set_boolean (dobj, "Discovery", svc->config.device.discovery);
  json_object_set_string (dobj, "InitCmd", svc->config.device.initcmd);
  json_object_set_string (dobj, "InitCmdArgs", svc->config.device.initcmdargs);
  json_object_set_number (dobj, "MaxCmdOps", svc->config.device.maxcmdops);
  json_object_set_number
    (dobj, "MaxCmdResultLen", svc->config.device.maxcmdresultlen);
  json_object_set_string (dobj, "RemoveCmd", svc->config.device.removecmd);
  json_object_set_string
    (dobj, "RemoveCmdArgs", svc->config.device.removecmdargs);
  json_object_set_string (dobj, "ProfilesDir", svc->config.device.profilesdir);
  json_object_set_boolean
    (dobj, "SendReadingsOnChanged", svc->config.device.sendreadingsonchanged);
  json_object_set_value (obj, "Device", dval);

  edgex_nvpairs *iter = svc->config.driverconf;
  if (iter)
  {
    dval = json_value_init_object ();
    dobj = json_value_get_object (dval);
    while (iter)
    {
      json_object_set_string (dobj, iter->name, iter->value);
      iter = iter->next;
    }
    json_object_set_value (obj, "Driver", dval);
  }

  *reply = json_serialize_to_string (val);
  *reply_size = strlen (*reply);
  *reply_type = "application/json";
  json_value_free (val);

  return MHD_HTTP_OK;
}

void edgex_device_process_configured_devices
  (edgex_device_service *svc, toml_array_t *devs, edgex_error *err)
{
  if (devs)
  {
    char *devname;
    const char *raw;
    edgex_device *existing;
    char *profile_name;
    char *description;
    edgex_protocols *protocols;
    edgex_device_autoevents *autos;
    edgex_strings *labels;
    edgex_strings *newlabel;
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
              edgex_protocols *newprots = malloc (sizeof (edgex_protocols));
              newprots->name = strdup (key);
              newprots->properties = NULL;
              newprots->next = protocols;
              for (int j = 0; 0 != (key = toml_key_in (pprops, j)); j++)
              {
                raw = toml_raw_in (pprops, key);
                if (raw)
                {
                  edgex_nvpairs *pair = malloc (sizeof (edgex_nvpairs));
                  pair->name = strdup (key);
                  if (toml_rtos (raw, &pair->value) == -1)
                  {
                    pair->value = strdup (raw);
                  }
                  pair->next = newprots->properties;
                  newprots->properties = pair;
                }
              }
              protocols = newprots;
            }
            else
            {
              iot_log_error
                (svc->logger, "Arrays and subtables not supported in Protocol");
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
              newlabel = malloc (sizeof (edgex_strings));
              newlabel->next = labels;
              toml_rtos2 (raw, &newlabel->str);
              labels = newlabel;
            }
          }

          *err = EDGEX_OK;
          free (edgex_device_add_device (svc, devname, description, labels, profile_name, protocols, autos, err));

          edgex_strings_free (labels);
          edgex_protocols_free (protocols);
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
