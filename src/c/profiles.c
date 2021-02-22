/*
 * Copyright (c) 2018-2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "edgex/profiles.h"
#include "profiles.h"
#include "service.h"
#include "metadata.h"
#include "data.h"
#include "edgex-rest.h"
#include "iot/time.h"
#include "errorlist.h"

#include <dirent.h>
#include <errno.h>
#include <yaml.h>

#define MAX_PATH_SIZE 256

static int yamlselect (const struct dirent *d)
{
  return strcasecmp (d->d_name + strlen (d->d_name) - 5, ".yaml") == 0 ? 1 : 0;
}

const edgex_deviceprofile *edgex_deviceprofile_get_internal
(
  devsdk_service_t *svc,
  const char *name,
  devsdk_error *err
)
{
  const edgex_deviceprofile *dp;

  dp = edgex_devmap_profile (svc->devices, name);
  if (dp == NULL)
  {
    edgex_deviceprofile *newdp = edgex_metadata_client_get_deviceprofile
      (svc->logger, &svc->config.endpoints, name, err);
    if (newdp)
    {
      edgex_devmap_add_profile (svc->devices, newdp);
      dp = newdp;
    }
  }
  return dp;
}

void edgex_device_profiles_upload (devsdk_service_t *svc, devsdk_error *err)
{
  struct dirent **filenames = NULL;
  int n;
  char *fname;
  char pathname[MAX_PATH_SIZE];
  const char *profileDir = svc->config.device.profilesdir;
  iot_logger_t *lc = svc->logger;

  *err = EDGEX_OK;
  n = scandir (profileDir, &filenames, yamlselect, NULL);
  if (n < 0)
  {
    if (errno == ENOENT || errno == ENOTDIR)
    {
       iot_log_error (lc, "No profiles directory found at %s", profileDir);
    }
    else
    {
      iot_log_error (lc, "Error scanning profiles directory: %s", strerror (errno));
    }
    *err = EDGEX_PROFILES_DIRECTORY;
    return;
  }

  iot_log_info (lc, "Processing Device Profiles from %s", profileDir);

  while (n--)
  {
    if (err->code == 0)
    {
      fname = filenames[n]->d_name;
      if (snprintf (pathname, MAX_PATH_SIZE, "%s/%s", profileDir, fname) < MAX_PATH_SIZE)
      {
        edgex_add_profile (svc, pathname, err);
      }
      else
      {
        iot_log_error (lc, "%s: Pathname too long (max %d chars)", fname, MAX_PATH_SIZE - 1);
        *err = EDGEX_PROFILE_PARSE_ERROR;
      }
    }
    free (filenames[n]);
  }
  free (filenames);
}

static char *getProfName (iot_logger_t *lc, const char *fname, devsdk_error *err)
{
  FILE *handle;
  yaml_parser_t parser;
  yaml_event_t event;
  bool lastWasName;
  char *result = NULL;

  if (yaml_parser_initialize (&parser))
  {
    handle = fopen (fname, "r");
    if (handle)
    {
      lastWasName = false;
      yaml_parser_set_input_file (&parser, handle);
      do
      {
        if (!yaml_parser_parse (&parser, &event))
        {
          iot_log_error (lc, "Parser error %d for file %s", parser.error, fname);
          *err = EDGEX_PROFILE_PARSE_ERROR;
          break;
        }
        if (event.type == YAML_SCALAR_EVENT)
        {
          if (lastWasName)
          {
            result = strdup ((char *) event.data.scalar.value);
            break;
          }
          else
          {
            lastWasName = (strcasecmp ((char *) event.data.scalar.value, "name") == 0);
          }
        }
        else
        {
          lastWasName = false;
        }
        if (event.type != YAML_STREAM_END_EVENT)
        {
          yaml_event_delete (&event);
        }
      } while (event.type != YAML_STREAM_END_EVENT);
      yaml_event_delete (&event);

      fclose (handle);
      if (result == NULL && err->code == 0)
      {
        iot_log_error (lc, "No device profile name found in %s", fname);
        *err = EDGEX_PROFILE_PARSE_ERROR;
      }
    }
    else
    {
      iot_log_error (lc, "Unable to open %s for reading", fname);
      *err = EDGEX_PROFILE_PARSE_ERROR;
    }
    yaml_parser_delete (&parser);
  }
  else
  {
    iot_log_error (lc, "YAML parser did not initialize - DeviceProfile upload disabled");
    *err = EDGEX_PROFILE_PARSE_ERROR;
  }
  return result;
}

void edgex_add_profile (devsdk_service_t *svc, const char *fname, devsdk_error *err)
{
  const edgex_deviceprofile *dp;
  edgex_service_endpoints *endpoints = &svc->config.endpoints;
  char *profname;
  iot_logger_t *lc = svc->logger;

  *err = EDGEX_OK;
  profname = getProfName (lc, fname, err);
  if (profname)
  {
    iot_log_debug (lc, "Checking existence of DeviceProfile %s", profname);
    if (edgex_deviceprofile_get_internal (svc, profname, err))
    {
      iot_log_info (lc, "DeviceProfile %s already exists: skipped", profname);
    }
    else
    {
      iot_log_info (lc, "Uploading deviceprofile from %s", fname);
      free (edgex_metadata_client_create_deviceprofile_file (lc, endpoints, fname, err));
      if (err->code)
      {
        iot_log_error (lc, "Error uploading device profile");
      }
      else
      {
        iot_log_debug (lc, "Device profile upload successful, will now retrieve it");
        dp = edgex_deviceprofile_get_internal (svc, profname, err);
        if (!dp)
        {
          iot_log_error (lc, "Failed to retrieve DeviceProfile %s", profname);
        }
      }
    }
    free (profname);
  }
}

edgex_deviceprofile *edgex_get_deviceprofile_byname
  (devsdk_service_t *svc, const char *name)
{
  devsdk_error err = EDGEX_OK;
  return edgex_deviceprofile_dup (edgex_deviceprofile_get_internal (svc, name, &err));
}

edgex_deviceprofile *edgex_profiles (devsdk_service_t *svc)
{
  return edgex_devmap_copyprofiles (svc->devices);
}

void edgex_free_deviceprofile (edgex_deviceprofile *dp)
{
  edgex_deviceprofile_free (dp);
}
