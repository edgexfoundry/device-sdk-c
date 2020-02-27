/*
 * Copyright (c) 2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "devsdk/devsdk.h"
#undef DEVSDKV2
#include "edgex/devsdk.h"
#include "errorlist.h"
#include "edgex-rest.h"
#include "edgex/device-mgmt.h"
#include "edgex/devices.h"
#include "edgex/profiles.h"

#define EX_ERROR(X) (edgex_error){ X.code, X.reason }

struct edgex_device_service
{
  devsdk_service_t *impl;
  iot_logger_t *lc;
  const char *name;
  const char *version;
  void *impldata;
  edgex_device_callbacks implfns;
  edgex_device_add_device_callback add_device;
  edgex_device_update_device_callback update_device;
  edgex_device_remove_device_callback remove_device;
  bool overwrite;
  edgex_nvpairs *config;
};

static bool compat_init (void *impl, struct iot_logger_t *lc, const iot_data_t *config)
{
  edgex_device_service *v1 = (edgex_device_service *)impl;
  v1->lc = lc;

  if (config)
  {
    iot_data_map_iter_t iter;
    iot_data_map_iter (config, &iter);
    while (iot_data_map_iter_next (&iter))
    {
      v1->config = edgex_nvpairs_new (iot_data_map_iter_string_key (&iter), iot_data_map_iter_string_value (&iter), v1->config);
    }
  }

  return v1->implfns.init (v1->impldata, lc, v1->config);
}

static void compat_discover (void *impl)
{
  edgex_device_service *v1 = (edgex_device_service *)impl;
  v1->implfns.discover (v1->impldata);
}

static char *nvpsToStr (const devsdk_nvpairs *nvps)
{
  size_t size = 256;
  size_t used = 1;
  char *result = malloc (size);
  *result = '\0';

  for (const devsdk_nvpairs *p = nvps; p; p = p->next)
  {
    size_t nsize = used + strlen (p->name) + strlen (p->value) + 2;
    if (nsize > size)
    {
      size = nsize + 256;
      result = realloc (result, size);
    }
    if (used != 1)
    {
      strcat (result, "&");
      used++;
    }
    strcat (result, p->name);
    used += strlen (p->name);
    if (p->value)
    {
      strcat (result, "=");
      strcat (result, p->value);
      used += (strlen (p->value) + 1);
    }
  }
  return result;
}

static bool compat_get_handler
(
  void *impl,
  const char *devname,
  const devsdk_protocols *protocols,
  uint32_t nreadings,
  const devsdk_commandrequest *requests,
  devsdk_commandresult *readings,
  const devsdk_nvpairs *qparams,
  iot_data_t **exception
)
{
  edgex_device_commandrequest *erequests;
  edgex_device_commandresult *ereadings;
  edgex_nvpairs *epairs = NULL;
  char *qstr = NULL;
  bool result;
  edgex_device_service *v1 = (edgex_device_service *)impl;

  if (qparams)
  {
    qstr = nvpsToStr (qparams);
    erequests = calloc (sizeof (edgex_device_commandrequest), nreadings);
    epairs = calloc (sizeof (edgex_nvpairs), nreadings);
    for (uint32_t i = 0; i < nreadings; i++)
    {
      erequests[i].resname = requests[i].resname;
      epairs[i].name = "urlRawQuery";
      epairs[i].value = qstr;
      epairs[i].next = (edgex_nvpairs *)requests[i].attributes;
      erequests[i].attributes = &epairs[i];
      erequests[i].type = requests[i].type;
    }
  }
  else
  {
    erequests = (edgex_device_commandrequest *)requests;
  }
  ereadings = calloc (sizeof (edgex_device_commandresult), nreadings);
  result = v1->implfns.gethandler (v1->impldata, devname, (const edgex_protocols *)protocols, nreadings, erequests, ereadings);
  for (uint32_t i = 0; result && i < nreadings; i++)
  {
    readings[i].origin = ereadings[i].origin;
    switch (ereadings[i].type)
    {
      case Int8:
        readings[i].value = iot_data_alloc_i8 (ereadings[i].value.i8_result);
        break;
      case Uint8:
        readings[i].value = iot_data_alloc_ui8 (ereadings[i].value.ui8_result);
        break;
      case Int16:
        readings[i].value = iot_data_alloc_i16 (ereadings[i].value.i16_result);
        break;
      case Uint16:
        readings[i].value = iot_data_alloc_ui16 (ereadings[i].value.ui16_result);
        break;
      case Int32:
        readings[i].value = iot_data_alloc_i32 (ereadings[i].value.i32_result);
        break;
      case Uint32:
        readings[i].value = iot_data_alloc_ui32 (ereadings[i].value.ui32_result);
        break;
      case Int64:
        readings[i].value = iot_data_alloc_i64 (ereadings[i].value.i64_result);
        break;
      case Uint64:
        readings[i].value = iot_data_alloc_ui64 (ereadings[i].value.ui64_result);
        break;
      case Float32:
        readings[i].value = iot_data_alloc_f32 (ereadings[i].value.f32_result);
        break;
      case Float64:
        readings[i].value = iot_data_alloc_f64 (ereadings[i].value.f64_result);
        break;
      case Bool:
        readings[i].value = iot_data_alloc_bool (ereadings[i].value.bool_result);
        break;
      case String:
        readings[i].value = iot_data_alloc_string (ereadings[i].value.string_result, IOT_DATA_TAKE);
        break;
      case Binary:
        readings[i].value = iot_data_alloc_array (ereadings[i].value.binary_result.bytes, ereadings[i].value.binary_result.size, IOT_DATA_UINT8, IOT_DATA_TAKE);
        break;
      default:
        result = false;
        *exception = iot_data_alloc_string ("Unsupported data type (map/array) returned by driver", IOT_DATA_REF);
    }
  }
  if (qparams)
  {
    free (erequests);
    free (epairs);
    free (qstr);
  }
  free (ereadings);
  return result;
}

static bool compat_put_handler
(
  void *impl,
  const char *devname,
  const devsdk_protocols *protocols,
  uint32_t nvalues,
  const devsdk_commandrequest *requests,
  const iot_data_t *values[],
  iot_data_t **exception
)
{
  bool result = true;
  edgex_device_service *v1 = (edgex_device_service *)impl;
  edgex_device_commandresult *evalues = calloc (sizeof (edgex_device_commandresult), nvalues);

  for (uint32_t i = 0; result && i < nvalues; i++)
  {
    evalues[i].type = iot_data_type (values[i]);
    switch (evalues[i].type)
    {
      case Int8:
        evalues[i].value.i8_result = iot_data_i8 (values[i]);
        break;
      case Uint8:
        evalues[i].value.ui8_result = iot_data_ui8 (values[i]);
        break;
      case Int16:
        evalues[i].value.i16_result = iot_data_i16 (values[i]);
        break;
      case Uint16:
        evalues[i].value.ui16_result = iot_data_ui16 (values[i]);
        break;
      case Int32:
        evalues[i].value.i32_result = iot_data_i32 (values[i]);
        break;
      case Uint32:
        evalues[i].value.ui32_result = iot_data_ui32 (values[i]);
        break;
      case Int64:
        evalues[i].value.i64_result = iot_data_i64 (values[i]);
        break;
      case Uint64:
        evalues[i].value.ui64_result = iot_data_ui64 (values[i]);
        break;
      case Float32:
        evalues[i].value.f32_result = iot_data_f32 (values[i]);
        break;
      case Float64:
        evalues[i].value.f64_result = iot_data_f64 (values[i]);
        break;
      case Bool:
        evalues[i].value.bool_result = iot_data_bool (values[i]);
        break;
      case String:
        evalues[i].value.string_result = (char *)iot_data_string (values[i]);
        break;
      case Binary:
        evalues[i].value.binary_result.bytes = iot_data_address (values[i]);
        evalues[i].value.binary_result.size = iot_data_array_size (values[i]);
        break;
      default:
        result = false;
        *exception = iot_data_alloc_string ("Unsupported data type (map/array) generated in SDK", IOT_DATA_REF);
    }
  }
  if (result)
  {
    result = v1->implfns.puthandler
      (v1->impldata, devname, (const edgex_protocols *)protocols, nvalues, (const edgex_device_commandrequest *)requests, evalues);
  }

  free (evalues);
  return result;
}

static void compat_stop (void *impl, bool force)
{
  edgex_device_service *v1 = (edgex_device_service *)impl;
  v1->implfns.stop (v1->impldata, force);
}

static void compat_add_device (void *impl, const char *devname, const devsdk_protocols *protocols, bool adminEnabled)
{
  edgex_device_service *v1 = (edgex_device_service *)impl;
  if (v1->add_device)
  {
    (v1->add_device)(v1->impldata, devname, (const edgex_protocols *)protocols, adminEnabled ? ENABLED : DISABLED);
  }
}

static void compat_remove_device (void *impl, const char *devname, const devsdk_protocols *protocols)
{
  edgex_device_service *v1 = (edgex_device_service *)impl;
  if (v1->remove_device)
  {
    (v1->remove_device)(v1->impldata, devname, (const edgex_protocols *)protocols);
  }
}

static void compat_update_device (void *impl, const char *devname, const devsdk_protocols *protocols, bool adminEnabled)
{
  edgex_device_service *v1 = (edgex_device_service *)impl;
  if (v1->update_device)
  {
    (v1->update_device)(v1->impldata, devname, (const edgex_protocols *)protocols, adminEnabled ? ENABLED : DISABLED);
  }
}

void edgex_device_service_usage ()
{
  printf ("  -n, --name=<name>\t: Set the device service name\n");
  printf ("  -r, --registry=<url>\t: Use the registry service\n");
  printf ("  -p, --profile=<name>\t: Set the profile name\n");
  printf ("  -c, --confdir=<dir>\t: Set the configuration directory\n");
}

static bool testArgOpt (char *arg, char *val, const char *pshort, const char *plong, const char **var, bool *result)
{
  if (strcmp (arg, pshort) == 0 || strcmp (arg, plong) == 0)
  {
    if (val && *val && *val != '-')
    {
      *var = val;
    }
    else
    {
      if (*var == NULL)
      {
        *var = "";
      }
      *result = false;
    }
    return true;
  }
  else
  {
    return false;
  }
}
static bool testArg (char *arg, char *val, const char *pshort, const char *plong, const char **var, bool *result)
{
  if (strcmp (arg, pshort) == 0 || strcmp (arg, plong) == 0)
  {
    if (val && *val)
    {
      *var = val;
    }
    else
    {
      printf ("Option \"%s\" requires a parameter\n", arg);
      *result = false;
    }
    return true;
  }
  else
  {
    return false;
  }
}

static bool testBool (char *arg, char *val, const char *pshort, const char *plong, bool *var, bool *result)
{
  if (strcmp (arg, pshort) == 0 || strcmp (arg, plong) == 0)
  {
    *var = true;
    return true;
  }
  return false;
}

static void consumeArgs (int *argc_p, char **argv, int start, int nargs)
{
  for (int n = start + nargs; n < *argc_p; n++)
  {
    argv[n - nargs] = argv[n];
  }
  *argc_p -= nargs;
}

bool edgex_device_service_processparams
  (int *argc_p, char **argv, edgex_device_svcparams *params)
{
  bool result = true;
  char *eq;
  char *arg;
  char *val;
  int argc = *argc_p;

  val = getenv ("edgex_registry");
  if (val)
  {
    params->regURL = val;
  }

  int n = 1;
  while (result && n < argc)
  {
    arg = argv[n];
    val = NULL;
    eq = strchr (arg, '=');
    if (eq)
    {
      *eq = '\0';
      val = eq + 1;
    }
    else if (n + 1 < argc)
    {
      val = argv[n + 1];
    }
    if (testArgOpt (arg, val, "-r", "--registry", &params->regURL, &result))
    {
      consumeArgs (&argc, argv, n, result ? 2 : 1);
      result = true;
    } else if
    (
      testArg (arg, val, "-n", "--name", &params->svcname, &result) ||
      testArg (arg, val, "-p", "--profile", &params->profile, &result) ||
      testArg (arg, val, "-c", "--confdir", &params->confdir, &result)
    )
    {
      consumeArgs (&argc, argv, n, eq ? 1 : 2);
    }
    else if
    (
      testBool (arg, val, "-o", "--overwrite", &params->overwrite, &result)
    )
    {
      consumeArgs (&argc, argv, n, 1);
    }
    else
    {
      n++;
    }
    if (eq)
    {
      *eq = '=';
    }
  }
  *argc_p = argc;
  return result;
}

edgex_device_service *edgex_device_service_new
(
  const char *name,
  const char *version,
  void *impldata,
  edgex_device_callbacks implfns,
  edgex_error *err
)
{
  devsdk_error de = EDGEX_OK;
  if (impldata == NULL)
  {
    iot_log_error
      (iot_logger_default (), "edgex_device_service_new: no implementation object");
    de = EDGEX_NO_DEVICE_IMPL;
    *err = EX_ERROR(de);
    return NULL;
  }
  if (name == NULL || strlen (name) == 0)
  {
    iot_log_error
      (iot_logger_default (), "edgex_device_service_new: no name specified");
    de = EDGEX_NO_DEVICE_NAME;
    *err = EX_ERROR(de);
    return NULL;
  }
  if (version == NULL || strlen (version) == 0)
  {
    iot_log_error
      (iot_logger_default (), "edgex_device_service_new: no version specified");
    de = EDGEX_NO_DEVICE_VERSION;
    *err = EX_ERROR(de);
    return NULL;
  }

  *err = EX_ERROR(de);
  edgex_device_service *res = calloc (sizeof (edgex_device_service), 1);
  res->name = name;
  res->version = version;
  res->impldata = impldata;
  res->implfns = implfns;
  return res;
}

void edgex_device_service_set_overwrite (edgex_device_service *svc, bool overwrite)
{
  svc->overwrite = overwrite;
}

void edgex_device_service_start
(
  edgex_device_service *svc,
  const char *registryURL,
  const char *profile,
  const char *confDir,
  edgex_error *err
)
{
  devsdk_error de = EDGEX_OK;
  devsdk_callbacks callbacks =
  {
    compat_init,
    compat_discover,
    compat_get_handler,
    compat_put_handler,
    compat_stop,
    compat_add_device,
    compat_update_device,
    compat_remove_device
  };
  char *argv[8];
  int argc = 1;

  if (registryURL)
  {
    argv[argc++] = "--registry";
    argv[argc++] = (char *)registryURL;
  }
  if (profile)
  {
    argv[argc++] = "--profile";
    argv[argc++] = (char *)profile;
  }
  if (confDir)
  {
    argv[argc++] = "--confdir";
    argv[argc++] = (char *)confDir;
  }
  if (svc->overwrite)
  {
    argv[argc++] = "--overwrite";
  }
  svc->impl = devsdk_service_new (svc->name, svc->version, svc, callbacks, &argc, argv, &de);
  if (svc->impl)
  {
    devsdk_service_start (svc->impl, &de);
  }
  if (de.code)
  {
    *err = EX_ERROR (de);
  }
}

void edgex_device_service_stop (edgex_device_service *svc, bool force, edgex_error *err)
{
  devsdk_error de = EDGEX_OK;
  devsdk_service_stop (svc->impl, force, &de);
  if (de.code)
  {
    *err = EX_ERROR (de);
  }
}

void edgex_device_service_free (edgex_device_service *svc)
{
  devsdk_service_free (svc->impl);
  edgex_nvpairs_free (svc->config);
  free (svc);
}

char * edgex_device_add_device
(
  edgex_device_service *svc,
  const char *name,
  const char *description,
  const edgex_strings *labels,
  const char *profile_name,
  edgex_protocols *protocols,
  edgex_device_autoevents *autos,
  edgex_error *err
);

void edgex_device_remove_device (edgex_device_service *svc, const char *id, edgex_error *err);

void edgex_device_remove_device_byname (edgex_device_service *svc, const char *name, edgex_error *err);

void edgex_device_update_device
(
  edgex_device_service *svc,
  const char *id,
  const char *name,
  const char *description,
  const edgex_strings *labels,
  const char *profilename,
  edgex_error *err
);

edgex_device * edgex_device_devices (edgex_device_service *svc)
{
  return edgex_devices (svc->impl);
}

edgex_device * edgex_device_get_device (edgex_device_service *svc, const char *id)
{
  return edgex_get_device (svc->impl, id);
}

edgex_device * edgex_device_get_device_byname (edgex_device_service *svc, const char *name)
{
  return edgex_get_device_byname (svc->impl, name);
}

void edgex_device_free_device (edgex_device *d)
{
  edgex_free_device (d);
}

edgex_deviceprofile *edgex_device_profiles (edgex_device_service *svc)
{
  return edgex_profiles (svc->impl);
}

edgex_deviceprofile *edgex_device_get_deviceprofile_byname (edgex_device_service *svc, const char *name)
{
  return edgex_get_deviceprofile_byname (svc->impl, name);
}

void edgex_device_free_deviceprofile (edgex_deviceprofile *p)
{
  edgex_free_deviceprofile (p);
}

void edgex_device_add_profile (edgex_device_service *svc, const char *fname, edgex_error *err)
{
  edgex_add_profile (svc->impl, fname, (devsdk_error *)err);
}

void edgex_device_register_devicelist_callbacks
(
  edgex_device_service *svc,
  edgex_device_add_device_callback add_device,
  edgex_device_update_device_callback update_device,
  edgex_device_remove_device_callback remove_device
)
{
  if (svc->impl)
  {
    iot_log_error (svc->lc, "Devicelist: must register callbacks before service start.");
    return;
  }
  svc->add_device = add_device;
  svc->update_device = update_device;
  svc->remove_device = remove_device;
}
