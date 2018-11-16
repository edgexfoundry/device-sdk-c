/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

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

static int yamlselect (const struct dirent *d);

int yamlselect (const struct dirent *d)
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

  for (edgex_deviceobject *res = dp->device_resources; res; res = res->next)
  {
    edgex_propertyvalue *pv = res->properties->value;
    edgex_units *units = res->properties->units;
    char type[2];
    edgex_valuedescriptor *vd;
    edgex_error err;
    iot_logging_client *lc = svc->logger;

    type[0] = pv->type[0];
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
      &err
    );
    if (err.code)
    {
      iot_log_error (lc, "Unable to create ValueDescriptor for %s", res->name);
    }
    edgex_valuedescriptor_free (vd);
  }
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
  edgex_deviceprofile *dp;
  const char *profileDir = svc->config.device.profilesdir;
  edgex_service_endpoints *endpoints = &svc->config.endpoints;
  iot_logging_client *lc = svc->logger;

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
    *err = EDGEX_NO_CONF_FILE;
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
        dp = edgex_metadata_client_get_deviceprofile
          (lc, endpoints, profname, err);
        if (dp)
        {
          iot_log_debug
            (lc, "DeviceProfile %s already exists: skipped", profname);
          if (edgex_map_get (&svc->profiles, profname))
          {
            edgex_deviceprofile_free (dp);
          }
          else
          {
            edgex_map_set (&svc->profiles, profname, dp);
          }
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
            iot_log_debug (lc, "Device profile upload successful");
            dp = edgex_metadata_client_get_deviceprofile
              (lc, endpoints, profname, err);
            if (dp)
            {
              iot_log_debug
                (lc, "Generating value descriptors DeviceProfile %s", profname);
              generate_value_descriptors (svc, dp);
              edgex_map_set (&svc->profiles, profname, dp);
            }
            else
            {
              iot_log_error
                (lc, "Failed to retrieve DeviceProfile %s", profname);
              if (err->code == 0)
              {
                *err = EDGEX_PROFILE_PARSE_ERROR;
              }
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
  edgex_deviceprofile **dpp;
  edgex_deviceprofile *dp;

  pthread_mutex_lock (&svc->profileslock);
  dpp = edgex_map_get (&svc->profiles, name);
  if (dpp == NULL)
  {
    dp = edgex_metadata_client_get_deviceprofile
      (svc->logger, &svc->config.endpoints, name, err);
    if (dp)
    {
      edgex_map_set (&svc->profiles, name, edgex_deviceprofile_dup (dp));
    }
  }
  else
  {
    dp = edgex_deviceprofile_dup (*dpp);
  }
  pthread_mutex_unlock (&svc->profileslock);
  return dp;
}

void edgex_device_service_getprofiles
(
  edgex_device_service *svc,
  uint32_t *count,
  edgex_deviceprofile **profiles
)
{
  edgex_map_iter iter = edgex_map_iter (svc->profiles);
  uint32_t i = 0;
  pthread_mutex_lock (&svc->profileslock);
  while (edgex_map_next (&svc->profiles, &iter))
  {
    i++;
  }
  *count = i;
  *profiles = malloc (i * sizeof (edgex_deviceprofile));

  const char *key;
  i = 0;
  iter = edgex_map_iter (svc->profiles);
  while ((key = edgex_map_next (&svc->profiles, &iter)))
  {
    (*profiles)[i++] = **edgex_map_get (&svc->profiles, key);
  }
  pthread_mutex_unlock (&svc->profileslock);
}
