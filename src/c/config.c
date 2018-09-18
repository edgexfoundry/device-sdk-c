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

static const char *parse8601 (const char *str, int *result);

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

const char *parse8601 (const char *str, int *result)
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

void toml_rtos2 (const char *s, char **ret)
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

#define GET_CONFIG_STRING(KEY, ELEMENT) \
toml_rtos2 (toml_raw_in (table, #KEY), &svc->config.ELEMENT);
#define GET_CONFIG_INT(KEY, ELEMENT) \
raw = toml_raw_in (table, #KEY); \
if (raw) \
{ \
  toml_rtoi (raw, &svc->config.ELEMENT); \
}
#define GET_CONFIG_BOOL(KEY, ELEMENT) \
raw = toml_raw_in (table, #KEY); \
if (raw) \
{ \
  int dummy; \
  toml_rtob (raw, &dummy); \
  svc->config.ELEMENT = dummy; \
}

void edgex_device_populateConfig
  (edgex_device_service *svc, toml_table_t *config)
{
  const char *raw;
  const char *key;
  toml_table_t *table;
  toml_table_t *subtable;
  toml_array_t *arr;

  table = toml_table_in (config, "Service");
  if (table)
  {
    GET_CONFIG_STRING(Host, service.host);
    GET_CONFIG_INT(Port, service.port);
    GET_CONFIG_INT(ConnectRetries, service.connectretries);
    GET_CONFIG_STRING(HealthCheck, service.healthcheck);
    GET_CONFIG_STRING(OpenMsg, service.openmsg);
    GET_CONFIG_INT(ReadMaxLimit, service.readmaxlimit);
    GET_CONFIG_INT(Timeout, service.timeout);
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

  table = toml_table_in (config, "Consul");
  if (table)
  {
    GET_CONFIG_STRING(Host, endpoints.consul.host);
    GET_CONFIG_INT(Port, endpoints.consul.port);
  }

  subtable = toml_table_in (config, "Clients");
  if (subtable)
  {
    table = toml_table_in (subtable, "Data");
    if (table)
    {
      GET_CONFIG_STRING(Host, endpoints.data.host);
      GET_CONFIG_INT(Port, endpoints.data.port);
    }
    table = toml_table_in (subtable, "Metadata");
    if (table)
    {
      GET_CONFIG_STRING(Host, endpoints.metadata.host);
      GET_CONFIG_INT(Port, endpoints.metadata.port);
    }
  }

  table = toml_table_in (config, "Devices");
  if (table)
  {
    GET_CONFIG_BOOL(DataTransform, devices.datatransform);
    GET_CONFIG_BOOL(Discovery, devices.discovery);
    GET_CONFIG_STRING(InitCmd, devices.initcmd);
    GET_CONFIG_STRING(InitCmdArgs, devices.initcmdargs);
    GET_CONFIG_INT(MaxCmdOps, devices.maxcmdops);
    GET_CONFIG_INT(MaxCmdResultLen, devices.maxcmdresultlen);
    GET_CONFIG_STRING(RemoveCmd, devices.removecmd);
    GET_CONFIG_STRING(RemoveCmdArgs, devices.removecmdargs);
    GET_CONFIG_STRING(ProfilesDir, devices.profilesdir);
    GET_CONFIG_BOOL(SendReadingsOnChanged, devices.sendreadingsonchanged);
  }

  table = toml_table_in (config, "Logging");
  if (table)
  {
    GET_CONFIG_STRING(RemoteURL, logging.remoteurl);
    GET_CONFIG_STRING(File, logging.file);
  }

  table = toml_table_in (config, "Schedules");
  if (table)
  {
    char *freqstr;
    for (int i = 0; NULL != (key = toml_key_in (table, i)); i++)
    {
      subtable = toml_table_in (table, key);
      freqstr = NULL;
      raw = toml_raw_in (subtable, "Frequency");
      toml_rtos2 (raw, &freqstr);
      if (freqstr)
      {
        int interval = 0;
        const char *err = parse8601 (freqstr, &interval);
        if (err)
        {
          iot_log_error
            (svc->logger, "Parse error for schedule %s: %s", key, err);
        }
        else
        {
          edgex_map_set (&svc->config.schedules, key, interval);
        }
        free (freqstr);
      }
      else
      {
        iot_log_error
          (svc->logger, "No frequency configured for schedule %s", key);
      }
    }
  }

  table = toml_table_in (config, "ScheduleEvents");
  if (table)
  {
    edgex_device_scheduleeventinfo info;
    for (int i = 0; NULL != (key = toml_key_in (table, i)); i++)
    {
      subtable = toml_table_in (table, key);
      info.schedule = NULL;
      info.path = NULL;
      raw = toml_raw_in (subtable, "Schedule");
      toml_rtos2 (raw, &info.schedule);
      raw = toml_raw_in (subtable, "Path");
      toml_rtos2 (raw, &info.path);
      if (info.schedule && info.path)
      {
        edgex_map_set (&svc->config.scheduleevents, key, info);
      }
      else
      {
        free (info.schedule);
        free (info.path);
        iot_log_error
        (
          svc->logger,
          "Schedule Event %s: Schedule and Path must be configured", key
        );
      }
    }
  }

  table = toml_table_in (config, "Watchers");
  if (table)
  {
    edgex_device_watcherinfo watcher;
    for (int i = 0; NULL != (key = toml_key_in (table, i)); i++)
    {
      subtable = toml_table_in (table, key);
      watcher.profile = NULL;
      watcher.key = NULL;
      watcher.matchstring = NULL;
      raw = toml_raw_in (subtable, "DeviceProfile");
      toml_rtos2 (raw, &watcher.profile);
      raw = toml_raw_in (subtable, "Key");
      toml_rtos2 (raw, &watcher.key);
      raw = toml_raw_in (subtable, "MatchString");
      toml_rtos2 (raw, &watcher.matchstring);
      int n = 0;
      arr = toml_array_in (subtable, "Identifiers");
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
      if (watcher.profile && watcher.key)
      {
        edgex_map_set (&svc->config.watchers, key, watcher);
      }
      else
      {
        iot_log_error
        (
          svc->logger,
          "Watcher %s: DeviceProfile and Key must be configured", key
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
      }
    }
  }
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
#define DUMP_INT(TEXT, VAR) iot_log_debug (svc->logger, TEXT " = %d", svc->config.VAR)
#define DUMP_LIT(TEXT) iot_log_debug (svc->logger, TEXT)
#define DUMP_ARR(TEXT, VAR) dumpArray(svc->logger, TEXT, svc->config.VAR)
#define DUMP_BOO(TEXT, VAR) iot_log_debug (svc->logger, TEXT " = %s", svc->config.VAR ? "true" : "false")

void edgex_device_dumpConfig (edgex_device_service *svc)
{
  const char *key;
  edgex_device_scheduleeventinfo *schedevt;
  edgex_device_watcherinfo *watcher;

  iot_log_debug (svc->logger, "Service configuration follows:");

  DUMP_LIT ("[Consul]");
  DUMP_STR ("   Host", endpoints.consul.host);
  DUMP_INT ("   Port", endpoints.consul.port);
  DUMP_LIT ("[Clients]");
  DUMP_LIT ("   [Clients.Data]");
  DUMP_STR ("      Host", endpoints.data.host);
  DUMP_INT ("      Port", endpoints.data.port);
  DUMP_LIT ("   [Clients.Metadata]");
  DUMP_STR ("      Host", endpoints.metadata.host);
  DUMP_INT ("      Port", endpoints.metadata.port);
  DUMP_LIT ("[Logging]");
  DUMP_STR ("   RemoteURL", logging.remoteurl);
  DUMP_STR ("   File", logging.file);
  DUMP_LIT ("[Service]");
  DUMP_STR ("   Host", service.host);
  DUMP_INT ("   Port", service.port);
  DUMP_INT ("   ConnectRetries", service.connectretries);
  DUMP_STR ("   HealthCheck", service.healthcheck);
  DUMP_STR ("   OpenMsg", service.openmsg);
  DUMP_INT ("   ReadMaxLimit", service.readmaxlimit);
  DUMP_INT ("   Timeout", service.timeout);
  DUMP_ARR ("   Labels", service.labels);
  DUMP_LIT ("[Devices]");
  DUMP_BOO ("   DataTransform", devices.datatransform);
  DUMP_BOO ("   Discovery", devices.discovery);
  DUMP_STR ("   InitCmd", devices.initcmd);
  DUMP_STR ("   InitCmdArgs", devices.initcmdargs);
  DUMP_INT ("   MaxCmdOps", devices.maxcmdops);
  DUMP_INT ("   MaxCmdResultLen", devices.maxcmdresultlen);
  DUMP_STR ("   RemoveCmd", devices.removecmd);
  DUMP_STR ("   RemoveCmdArgs", devices.removecmdargs);
  DUMP_STR ("   ProfilesDir", devices.profilesdir);
  DUMP_BOO ("   SendReadingsOnChanged", devices.sendreadingsonchanged);

  DUMP_LIT ("[Schedules]");
  edgex_map_iter i = edgex_map_iter (svc->config.schedules);
  while ((key = edgex_map_next (&svc->config.schedules, &i)))
  {
    iot_log_debug (svc->logger, "   [%s]", key);
    iot_log_debug (svc->logger, "      Period = %d seconds",
                   *edgex_map_get (&svc->config.schedules, key));
  }

  DUMP_LIT ("[ScheduleEvents]");
  i = edgex_map_iter (svc->config.scheduleevents);
  while ((key = edgex_map_next (&svc->config.scheduleevents, &i)))
  {
    iot_log_debug (svc->logger, "   [%s]", key);
    schedevt = edgex_map_get (&svc->config.scheduleevents, key);
    iot_log_debug (svc->logger, "      Schedule = \"%s\"", schedevt->schedule);
    iot_log_debug (svc->logger, "      Path = \"%s\"", schedevt->path);
  }

  DUMP_LIT ("[Watchers]");
  i = edgex_map_iter (svc->config.watchers);
  while ((key = edgex_map_next (&svc->config.watchers, &i)))
  {
    iot_log_debug (svc->logger, "   [%s]", key);
    watcher = edgex_map_get (&svc->config.watchers, key);
    iot_log_debug (svc->logger, "      DeviceProfile = \"%s\"",
                   watcher->profile);
    iot_log_debug (svc->logger, "      Key = \"%s\"", watcher->key);
    dumpArray (svc->logger, "      Identifiers", watcher->ids);
    iot_log_debug (svc->logger, "      MatchString = \"%s\"",
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
  free (svc->config.service.healthcheck);
  free (svc->config.service.openmsg);
  free (svc->config.devices.initcmd);
  free (svc->config.devices.initcmdargs);
  free (svc->config.devices.removecmd);
  free (svc->config.devices.removecmdargs);
  free (svc->config.devices.profilesdir);

  for (int i = 0; svc->config.service.labels[i]; i++)
  {
    free (svc->config.service.labels[i]);
  }
  free (svc->config.service.labels);

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
  (edgex_device_service *svc, toml_table_t *devtable)
{
  if (devtable)
  {
    edgex_error err;
    const char *key;
    const char *raw;
    edgex_device *existing;
    char *profile_name;
    char *description;
    edgex_addressable *address;
    edgex_strings *labels;
    edgex_strings *newlabel;
    toml_table_t *subtable;
    toml_table_t *addtable;
    toml_array_t *arr;

    for (int i = 0; NULL != (key = toml_key_in (devtable, i)); i++)
    {
      pthread_rwlock_rdlock (&svc->deviceslock);
      existing = edgex_map_get (&svc->devices, key);
      pthread_rwlock_unlock (&svc->deviceslock);
      if (existing == NULL)
      {
        subtable = toml_table_in (devtable, key);

        /* Addressable */

        addtable = toml_table_in (subtable, "Addressable");
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
          raw = toml_raw_in (subtable, "Profile");
          toml_rtos2 (raw, &profile_name);
          raw = toml_raw_in (subtable, "Description");
          toml_rtos2 (raw, &description);
          arr = toml_array_in (subtable, "Labels");
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

          err = EDGEX_OK;
          edgex_device_add_device
            (svc, key, description, labels, profile_name, address, &err);
          if (err.code)
          {
            iot_log_error
              (svc->logger, "Error registering device %s", key);
          }
          edgex_strings_free (labels);
          edgex_addressable_free (address);
          free (profile_name);
          free (description);
        }
        else
        {
          iot_log_error
            (svc->logger, "No Addressable section for device %s", key);
        }
      }
    }
  }
}
