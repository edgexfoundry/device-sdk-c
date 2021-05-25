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
#include "filesys.h"

#include <yaml.h>

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

static void edgex_add_profile_json (devsdk_service_t *svc, const char *fname, devsdk_error *err)
{
  JSON_Value *jval = json_parse_file (fname);
  if (jval)
  {
    JSON_Object *jobj = json_value_get_object (jval);
    if (json_object_get_string (jobj, "profileName"))
    {
      // it's a device, ignore it
      json_value_free (jval);
      return;
    }
    const char *name = json_object_get_string (jobj, "name");
    if (name)
    {
      iot_log_debug (svc->logger, "Checking existence of DeviceProfile %s", name);
      if (edgex_deviceprofile_get_internal (svc, name, err))
      {
        iot_log_info (svc->logger, "DeviceProfile %s already exists: skipped", name);
      }
      else
      {
        JSON_Value *copy = json_value_deep_copy (jval);
        JSON_Object *profobj = json_value_get_object (copy);
        edgex_metadata_client_add_profile_jobj (svc->logger, &svc->config.endpoints, profobj, err);
      }
    }
    else
    {
      iot_log_warn (svc->logger, "Device Profile upload: Missing deviceprofile name definition");
    }
    json_value_free (jval);
  }
  else
  {
    iot_log_error (svc->logger, "File does not parse as JSON");
    *err = EDGEX_CONF_PARSE_ERROR;
  }
}

void edgex_device_profiles_upload (devsdk_service_t *svc, devsdk_error *err)
{
  iot_log_info (svc->logger, "Processing Device Profiles from %s", svc->config.device.profilesdir);

  devsdk_strings *filenames = devsdk_scandir (svc->logger, svc->config.device.profilesdir, "yaml");
  for (devsdk_strings *f = filenames; f; f = f->next)
  {
    edgex_add_profile (svc, f->str, err);
  }
  devsdk_strings_free (filenames);

  filenames = devsdk_scandir (svc->logger, svc->config.device.profilesdir, "json");
  for (devsdk_strings *f = filenames; f; f = f->next)
  {
    edgex_add_profile_json (svc, f->str, err);
  }
  devsdk_strings_free (filenames);
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

void edgex_free_deviceprofile (devsdk_service_t *svc, edgex_deviceprofile *dp)
{
  edgex_deviceprofile_free (svc, dp);
}
