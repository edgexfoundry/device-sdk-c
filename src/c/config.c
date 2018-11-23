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
#include "edgex_rest.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void toml_rtos2 (const char *s, char **ret);

#define ERRBUFSZ 1024

toml_table_t *edgex_device_loadConfig
(
  iot_logging_client *lc,
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

/* Extract a frequency in seconds from an ISO8601 period */

typedef struct pmap
{
  char p;
  int factor;
} pmap;

static const pmap dates[] =
{
  {'Y', 31536000},    // 365d
  {'M', 16934400},    // 28d
  {'W', 604800},
  {'D', 86400},
  {0,   0}
};

static const pmap times[] =
{
  {'H', 3600},
  {'M', 60},
  {'S', 1},
  {0,   0}
};

const char *edgex_device_config_parse8601 (const char *str, int *result)
{
  char *endptr;
  int component;
  const pmap *curmap = dates;
  const char *iter = str;

  if (*iter++ != 'P')
  {
    return "Period string must begin with 'P'";
  }

  *result = 0;
  while (*iter)
  {
    if (*iter == 'T')
    {
      if (curmap == dates)
      {
        curmap = times;
        iter++;
      }
      else
      {
        return "Time separator 'T' can only be used once";
      }
    }
    component = strtol (iter, &endptr, 10);
    if (endptr == iter)
    {
      return "Unable to parse decimal";
    }
    for (int i = 0; curmap[i].p; i++)
    {
      if (*endptr == curmap[i].p)
      {
        *result += component * curmap[i].factor;
        iter = endptr + 1;
        break;
      }
    }
    if (iter <= endptr)
    {
      return "Unrecognized suffix - Use [YMWD] in the date position and [HMS] in the time position";
    }
  }
  return NULL;
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
  (const char *raw, uint16_t *ret, iot_logging_client *lc, edgex_error *err)
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
  (const char *raw, uint32_t *ret, iot_logging_client *lc, edgex_error *err)
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
    GET_CONFIG_UINT32(ReadMaxLimit, service.readmaxlimit);
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
    GET_CONFIG_STRING(RemoteURL, logging.remoteurl);
    GET_CONFIG_STRING(File, logging.file);
  }

  arr = toml_array_in (config, "Schedules");
  if (arr)
  {
    int n = 0;
    while ((table = toml_table_at (arr, n++)))
    {
      char *freqstr = NULL;
      namestr = NULL;
      int interval = 0;
      toml_rtos2 (toml_raw_in (table, "Frequency"), &freqstr);
      toml_rtos2 (toml_raw_in (table, "Name"), &namestr);
      if (namestr && freqstr)
      {
        const char *errmsg = edgex_device_config_parse8601 (freqstr, &interval);
        if (errmsg)
        {
          iot_log_error
            (svc->logger, "Parse error for schedule %s: %s", namestr, err);
          *err = EDGEX_BAD_CONFIG;
          free (freqstr);
        }
        else
        {
          edgex_map_set (&svc->config.schedules, namestr, freqstr);
        }
      }
      else
      {
        iot_log_error
          (svc->logger, "Parse error: Schedule requires Name and Frequency");
        *err = EDGEX_BAD_CONFIG;
        free (freqstr);
      }
      free (namestr);
    }
  }

  arr = toml_array_in (config, "ScheduleEvents");
  if (arr)
  {
    int n = 0;
    while ((table = toml_table_at (arr, n++)))
    {
      edgex_device_scheduleeventinfo info;
      info.schedule = NULL;
      info.path = NULL;
      namestr = NULL;
      toml_rtos2 (toml_raw_in (table, "Name"), &namestr);
      toml_rtos2 (toml_raw_in (table, "Schedule"), &info.schedule);
      toml_rtos2 (toml_raw_in (table, "Path"), &info.path);

      if (namestr && info.schedule && info.path)
      {
        edgex_map_set (&svc->config.scheduleevents, namestr, info);
      }
      else
      {
        free (info.schedule);
        free (info.path);
        iot_log_error
        (
          svc->logger,
          "Parse error: ScheduleEvent requires Name, Schedule and Path"
        );
        *err = EDGEX_BAD_CONFIG;
      }
      free (namestr);
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
  iot_logging_client *lc,
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
  iot_logging_client *lc,
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
  svc->config.service.readmaxlimit =
    get_nv_config_uint32 (svc->logger, config, "Service/ReadMaxLimit", err);
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

  svc->config.endpoints.data.host =
    get_nv_config_string (config, "Clients/Data/Host");
  svc->config.endpoints.data.port =
    get_nv_config_uint16 (svc->logger, config, "Clients/Data/Port", err);
  svc->config.endpoints.metadata.host =
    get_nv_config_string (config, "Clients/Metadata/Host");
  svc->config.endpoints.metadata.port =
    get_nv_config_uint16 (svc->logger, config, "Clients/Metadata/Port", err);

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

  svc->config.logging.remoteurl =
    get_nv_config_string (config, "Logging/RemoteURL");
  svc->config.logging.file = get_nv_config_string (config, "Logging/File");
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
  PUT_CONFIG_UINT(Service/ReadMaxLimit, service.readmaxlimit);
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

  PUT_CONFIG_STRING(Clients/Data/Host, endpoints.data.host);
  PUT_CONFIG_UINT(Clients/Data/Port, endpoints.data.port);
  PUT_CONFIG_STRING(Clients/Metadata/Host, endpoints.metadata.host);
  PUT_CONFIG_UINT(Clients/Metadata/Port, endpoints.metadata.port);

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

  PUT_CONFIG_STRING(Logging/RemoteURL, logging.remoteurl);
  PUT_CONFIG_STRING(Logging/File, logging.file);

  return result;
}

void edgex_device_validateConfig (edgex_device_service *svc, edgex_error *err)
{
  if (svc->config.endpoints.data.host == 0)
  {
    iot_log_error (svc->logger, "config: clients.data hostname unset");
    *err = EDGEX_BAD_CONFIG;
  }
  if (svc->config.endpoints.data.port == 0)
  {
    iot_log_error (svc->logger, "config: clients.data port unset");
    *err = EDGEX_BAD_CONFIG;
  }
  if (svc->config.endpoints.metadata.host == 0)
  {
    iot_log_error (svc->logger, "config: clients.metadata hostname unset");
    *err = EDGEX_BAD_CONFIG;
  }
  if (svc->config.endpoints.metadata.port == 0)
  {
    iot_log_error (svc->logger, "config: clients.metadata port unset");
    *err = EDGEX_BAD_CONFIG;
  }
  const edgex_device_scheduleeventinfo *evt;
  const char *key;
  edgex_map_iter i = edgex_map_iter (svc->config.scheduleevents);
  while ((key = edgex_map_next (&svc->config.scheduleevents, &i)))
  {
    evt = edgex_map_get (&svc->config.scheduleevents, key);
    if (edgex_map_get (&svc->config.schedules, evt->schedule) == NULL)
    {
      iot_log_error (svc->logger,
                     "config: In ScheduleEvent %s, no such Schedule %s",
                     key, evt->schedule);
      *err = EDGEX_BAD_CONFIG;
    }
  }
}

static void dumpArray (iot_logging_client *, const char *, char **);

void dumpArray (iot_logging_client *log, const char *name, char **list)
{
  char *arr;
  size_t arrlen = 1;
  for (int i = 0; list[i]; i++)
  {
    arrlen += strlen (list[i]) + 4;
  }
  arr = malloc (arrlen);
  arr[0] = '\0';
  for (int i = 0; list[i]; i++)
  {
    strcat (arr, "\"");
    strcat (arr, list[i]);
    strcat (arr, "\"");
    if (list[i + 1])
    {
      strcat (arr, ", ");
    }
  }
  iot_log_debug (log, "%s = [ %s ]", name, arr);
  free (arr);
}

#define DUMP_STR(TEXT, VAR) if (svc->config.VAR) iot_log_debug (svc->logger, TEXT " = \"%s\" ", svc->config.VAR)
#define DUMP_INT(TEXT, VAR) iot_log_debug (svc->logger, TEXT " = %ld", svc->config.VAR)
#define DUMP_UNS(TEXT, VAR) iot_log_debug (svc->logger, TEXT " = %u", svc->config.VAR)
#define DUMP_LIT(TEXT) iot_log_debug (svc->logger, TEXT)
#define DUMP_ARR(TEXT, VAR) dumpArray(svc->logger, TEXT, svc->config.VAR)
#define DUMP_BOO(TEXT, VAR) iot_log_debug (svc->logger, TEXT " = %s", svc->config.VAR ? "true" : "false")

void edgex_device_dumpConfig (edgex_device_service *svc)
{
  const char *key;
  edgex_device_scheduleeventinfo *schedevt;
  edgex_device_watcherinfo *watcher;

  iot_log_debug (svc->logger, "Service configuration follows:");

  DUMP_LIT ("[Clients]");
  DUMP_LIT ("   [Clients.Data]");
  DUMP_STR ("      Host", endpoints.data.host);
  DUMP_UNS ("      Port", endpoints.data.port);
  DUMP_LIT ("   [Clients.Metadata]");
  DUMP_STR ("      Host", endpoints.metadata.host);
  DUMP_UNS ("      Port", endpoints.metadata.port);
  DUMP_LIT ("[Logging]");
  DUMP_STR ("   RemoteURL", logging.remoteurl);
  DUMP_STR ("   File", logging.file);
  DUMP_LIT ("[Service]");
  DUMP_STR ("   Host", service.host);
  DUMP_UNS ("   Port", service.port);
  DUMP_UNS ("   Timeout", service.timeout);
  DUMP_UNS ("   ConnectRetries", service.connectretries);
  DUMP_STR ("   StartupMsg", service.startupmsg);
  DUMP_UNS ("   ReadMaxLimit", service.readmaxlimit);
  DUMP_STR ("   CheckInterval", service.checkinterval);
  DUMP_ARR ("   Labels", service.labels);
  DUMP_LIT ("[Device]");
  DUMP_BOO ("   DataTransform", device.datatransform);
  DUMP_BOO ("   Discovery", device.discovery);
  DUMP_STR ("   InitCmd", device.initcmd);
  DUMP_STR ("   InitCmdArgs", device.initcmdargs);
  DUMP_UNS ("   MaxCmdOps", device.maxcmdops);
  DUMP_UNS ("   MaxCmdResultLen", device.maxcmdresultlen);
  DUMP_STR ("   RemoveCmd", device.removecmd);
  DUMP_STR ("   RemoveCmdArgs", device.removecmdargs);
  DUMP_STR ("   ProfilesDir", device.profilesdir);
  DUMP_BOO ("   SendReadingsOnChanged", device.sendreadingsonchanged);

  edgex_nvpairs *iter = svc->config.driverconf;
  if (iter)
  {
    DUMP_LIT ("[Driver]");
  }
  while (iter)
  {
    iot_log_debug (svc->logger, "  %s = \"%s\"", iter->name, iter->value);
    iter = iter->next;
  }

  edgex_map_iter i = edgex_map_iter (svc->config.schedules);
  while ((key = edgex_map_next (&svc->config.schedules, &i)))
  {
    DUMP_LIT ("[[Schedules]]");
    iot_log_debug (svc->logger, "  Name = \"%s\"", key);
    iot_log_debug (svc->logger, "  Frequency = \"%s\"",
                   *edgex_map_get (&svc->config.schedules, key));
  }

  i = edgex_map_iter (svc->config.scheduleevents);
  while ((key = edgex_map_next (&svc->config.scheduleevents, &i)))
  {
    DUMP_LIT ("[[ScheduleEvents]]");
    iot_log_debug (svc->logger, "  Name = \"%s\"", key);
    schedevt = edgex_map_get (&svc->config.scheduleevents, key);
    iot_log_debug (svc->logger, "  Schedule = \"%s\"", schedevt->schedule);
    iot_log_debug (svc->logger, "  Path = \"%s\"", schedevt->path);
  }

  i = edgex_map_iter (svc->config.watchers);
  while ((key = edgex_map_next (&svc->config.watchers, &i)))
  {
    DUMP_LIT ("[[Watchers]]");
    iot_log_debug (svc->logger, "  Name = \"%s\"", key);
    watcher = edgex_map_get (&svc->config.watchers, key);
    iot_log_debug (svc->logger, "  DeviceProfile = \"%s\"",
                   watcher->profile);
    iot_log_debug (svc->logger, "  Key = \"%s\"", watcher->key);
    dumpArray (svc->logger, "  Identifiers", watcher->ids);
    iot_log_debug (svc->logger, "  MatchString = \"%s\"",
                   watcher->matchstring);
  }
}

void edgex_device_freeConfig (edgex_device_service *svc)
{
  edgex_device_scheduleeventinfo *schedevt;
  edgex_device_watcherinfo *watcher;
  edgex_map_iter iter;
  const char *key;

  free (svc->config.endpoints.consul.host);
  free (svc->config.endpoints.data.host);
  free (svc->config.endpoints.metadata.host);
  free (svc->config.logging.file);
  free (svc->config.logging.remoteurl);
  free (svc->config.service.host);
  free (svc->config.service.startupmsg);
  free (svc->config.service.checkinterval);
  free (svc->config.device.initcmd);
  free (svc->config.device.initcmdargs);
  free (svc->config.device.removecmd);
  free (svc->config.device.removecmdargs);
  free (svc->config.device.profilesdir);

  for (int i = 0; svc->config.service.labels[i]; i++)
  {
    free (svc->config.service.labels[i]);
  }
  free (svc->config.service.labels);

  edgex_nvpairs_free (svc->config.driverconf);

  iter = edgex_map_iter (svc->config.schedules);
  while ((key = edgex_map_next (&svc->config.schedules, &iter)))
  {
    free (*edgex_map_get (&svc->config.schedules, key));
  }
  edgex_map_deinit (&svc->config.schedules);

  iter = edgex_map_iter (svc->config.scheduleevents);
  while ((key = edgex_map_next (&svc->config.scheduleevents, &iter)))
  {
    schedevt = edgex_map_get (&svc->config.scheduleevents, key);
    free (schedevt->schedule);
    free (schedevt->path);
  }
  edgex_map_deinit (&svc->config.scheduleevents);

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

void edgex_device_process_configured_devices
  (edgex_device_service *svc, toml_array_t *devs, edgex_error *err)
{
  if (devs)
  {
    char *devname;
    const char *raw;
    char **existing;
    char *profile_name;
    char *description;
    edgex_addressable *address;
    edgex_strings *labels;
    edgex_strings *newlabel;
    toml_table_t *table;
    toml_table_t *addtable;
    toml_array_t *arr;
    int n = 0;

    while ((table = toml_table_at (devs, n++)))
    {
      raw = toml_raw_in (table, "Name");
      toml_rtos2 (raw, &devname);
      pthread_rwlock_rdlock (&svc->deviceslock);
      existing = edgex_map_get (&svc->name_to_id, devname);
      pthread_rwlock_unlock (&svc->deviceslock);
      if (existing == NULL)
      {
        /* Addressable */

        addtable = toml_table_in (table, "Addressable");
        if (addtable)
        {
          address = malloc (sizeof (edgex_addressable));
          memset (address, 0, sizeof (edgex_addressable));
          raw = toml_raw_in (addtable, "Name");
          toml_rtos2 (raw, &address->name);
          raw = toml_raw_in (addtable, "Address");
          toml_rtos2 (raw, &address->address);
          raw = toml_raw_in (addtable, "Method");
          toml_rtos2 (raw, &address->method);
          raw = toml_raw_in (addtable, "Path");
          toml_rtos2 (raw, &address->path);
          raw = toml_raw_in (addtable, "User");
          toml_rtos2 (raw, &address->user);
          raw = toml_raw_in (addtable, "Password");
          toml_rtos2 (raw, &address->password);
          raw = toml_raw_in (addtable, "Protocol");
          toml_rtos2 (raw, &address->protocol);
          raw = toml_raw_in (addtable, "Publisher");
          toml_rtos2 (raw, &address->publisher);
          raw = toml_raw_in (addtable, "Topic");
          toml_rtos2 (raw, &address->topic);
          raw = toml_raw_in (addtable, "Port");
          if (raw)
          {
            int64_t p;
            toml_rtoi (raw, &p);
            if (p > 0)
            {
              address->port = p;
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
          free (edgex_device_add_device
            (svc, devname, description, labels, profile_name, address, err));

          edgex_strings_free (labels);
          edgex_addressable_free (address);
          free (profile_name);
          free (description);

          if (err->code)
          {
            iot_log_error (svc->logger, "Error registering device %s", devname);
            break;
          }
        }
        else
        {
          iot_log_error
            (svc->logger, "No Addressable section for device %s", devname);
          *err = EDGEX_BAD_CONFIG;
          break;
        }
      }
      free (devname);
    }
  }
}
