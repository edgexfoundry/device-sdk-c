/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "edgex/device-mgmt.h"
#include "profiles.h"
#include "service.h"
#include "metadata.h"
#include "data.h"
#include "edgex_rest.h"
#include "edgex_time.h"
#include "errorlist.h"

#include <dirent.h>
#include <errno.h>
#include <yaml.h>

#define MAX_PATH_SIZE 256

static int yamlselect (const struct dirent *d)
{
  return strcasecmp (d->d_name + strlen (d->d_name) - 5, ".yaml") == 0 ? 1 : 0;
}

static void generate_value_descriptors
(
  edgex_device_service *svc,
  const edgex_deviceprofile *dp
)
{
  uint64_t timenow = edgex_device_millitime ();

  for (edgex_deviceresource *res = dp->device_resources; res; res = res->next)
  {
    edgex_propertyvalue *pv = res->properties->value;
    edgex_units *units = res->properties->units;
    char type[2];
    edgex_valuedescriptor *vd;
    edgex_error err;
    iot_logger_t *lc = svc->logger;

    type[0] = edgex_propertytype_tostring (pv->type)[0];
    type[1] = '\0';
    vd = edgex_data_client_add_valuedescriptor
    (
      lc,
      &svc->config.endpoints,
      res->name,
      timenow,
      pv->minimum,
      pv->maximum,
      type,
      units->defaultvalue,
      pv->defaultvalue,
      "%s",
      res->description,
      pv->mediaType,
      pv->floatAsBinary ? "base64" : "eNotation",
      &err
    );
    if (err.code)
    {
      iot_log_error (lc, "Unable to create ValueDescriptor for %s", res->name);
    }
    edgex_valuedescriptor_free (vd);
  }
}

static const edgex_deviceprofile *edgex_deviceprofile_get_internal
(
  edgex_device_service *svc,
  const char *name,
  edgex_error *err
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

void edgex_device_profiles_upload
(
  edgex_device_service *svc,
  edgex_error *err
)
{
  struct dirent **filenames = NULL;
  int n;
  char *fname;
  char pathname[MAX_PATH_SIZE];
  char *profname;
  FILE *handle;
  yaml_parser_t parser;
  yaml_event_t event;
  bool lastWasName;
  const edgex_deviceprofile *dp;
  const char *profileDir = svc->config.device.profilesdir;
  edgex_service_endpoints *endpoints = &svc->config.endpoints;
  iot_logger_t *lc = svc->logger;

  n = scandir (profileDir, &filenames, yamlselect, NULL);
  if (n < 0)
  {
    if (errno == ENOENT || errno == ENOTDIR)
    {
       iot_log_error (lc, "No profiles directory found at %s", profileDir);
    }
    else
    {
      iot_log_error
        (lc, "Error scanning profiles directory: %s", strerror (errno));
    }
    *err = EDGEX_PROFILES_DIRECTORY;
    return;
  }

  while (n--)
  {
    profname = NULL;

    if (!yaml_parser_initialize (&parser))
    {
      iot_log_error
        (lc, "YAML parser did not initialize - DeviceProfile upload disabled");
      *err = EDGEX_PROFILE_PARSE_ERROR;
      break;
    }

    fname = filenames[n]->d_name;
    if (snprintf (pathname, MAX_PATH_SIZE, "%s/%s", profileDir, fname) >=
        MAX_PATH_SIZE)
    {
      iot_log_error
        (lc, "%s: Pathname too long (max %d chars)", fname, MAX_PATH_SIZE - 1);
      *err = EDGEX_PROFILE_PARSE_ERROR;
      break;
    }

    handle = fopen (pathname, "r");
    if (handle)
    {
      lastWasName = false;
      yaml_parser_set_input_file (&parser, handle);
      do
      {
        if (!yaml_parser_parse (&parser, &event))
        {
          iot_log_error
            (lc, "Parser error %d for file %s", parser.error, fname);
          *err = EDGEX_PROFILE_PARSE_ERROR;
          break;
        }
        if (event.type == YAML_SCALAR_EVENT)
        {
          if (lastWasName)
          {
            profname = strdup ((char *) event.data.scalar.value);
            break;
          }
          else
          {
            lastWasName =
              (strcasecmp ((char *) event.data.scalar.value, "name") == 0);
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
      if (err->code)
      {
        break;
      }

      if (profname)
      {
        iot_log_debug
          (lc, "Checking existence of DeviceProfile %s", profname);
        if (edgex_deviceprofile_get_internal (svc, profname, err))
        {
          iot_log_debug
            (lc, "DeviceProfile %s already exists: skipped", profname);
        }
        else
        {
          *err = EDGEX_OK;
          iot_log_debug (lc, "Uploading deviceprofile from %s", pathname);
          free (edgex_metadata_client_create_deviceprofile_file
                  (lc, endpoints, pathname, err));
          if (err->code)
          {
            iot_log_error (lc, "Error uploading device profile");
            break;
          }
          else
          {
            iot_log_debug
              (lc, "Device profile upload successful, will now retrieve it");
            dp = edgex_deviceprofile_get_internal (svc, profname, err);
            if (dp)
            {
              iot_log_debug
                (lc, "Generating value descriptors DeviceProfile %s", profname);
              generate_value_descriptors (svc, dp);
            }
            else
            {
              iot_log_error
                (lc, "Failed to retrieve DeviceProfile %s", profname);
              break;
            }
          }
        }
        free (profname);
      }
      else
      {
        iot_log_error (lc, "No device profile name found in %s", fname);
        *err = EDGEX_PROFILE_PARSE_ERROR;
        break;
      }
    }
    else
    {
      iot_log_error (lc, "Unable to open %s for reading", fname);
      *err = EDGEX_PROFILE_PARSE_ERROR;
      break;
    }
    free (filenames[n]);
    yaml_parser_delete (&parser);
  }
  free (filenames);
}

edgex_deviceprofile *edgex_deviceprofile_get
(
  edgex_device_service *svc,
  const char *name,
  edgex_error *err
)
{
  *err = EDGEX_OK;
  return edgex_deviceprofile_dup
    (edgex_deviceprofile_get_internal (svc, name, err));
}

uint32_t edgex_device_service_getprofiles
(
  edgex_device_service *svc,
  edgex_deviceprofile **profiles
)
{
  return edgex_devmap_copyprofiles (svc->devices, profiles);
}
