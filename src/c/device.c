/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "device.h"
#include "service.h"
#include "errorlist.h"
#include "parson.h"
#include "data.h"
#include "metadata.h"
#include "edgex_rest.h"
#include "edgex_time.h"
#include "base64.h"

#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <microhttpd.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <assert.h>

/* NOTES
 *
 * The entry point for the device command is edgex_device_handler_device. This
 * parses the device spec and command name out of the url path and calls either
 * oneCommand or allCommand.
 * Each of these two methods finds the relevant device(s), calls runOne to
 * perform the command(s), uploads any readings and constructs the appropriate
 * JSON response.
 * runOne locates profile resources and calls either runOneGet or runOnePut.
 * runOneGet and runOnePut construct the required parameters, perform the
 * conversions between strings and values, and call the device implementation.
 */

static const char *methStr (edgex_http_method method)
{
  switch (method)
  {
    case GET:
      return "GET";
    case POST:
      return "POST";
    case PUT:
      return "PUT";
    case PATCH:
      return "PATCH";
    case DELETE:
      return "DELETE";
    default:
      return "UNKNOWN";
  }
}

static bool isNumericType (edgex_propertytype type)
{
  return (type != Bool && type != String && type != Binary);
}

static char *checkMapping (char *in, const edgex_nvpairs *map)
{
  const edgex_nvpairs *pair = map;
  while (pair)
  {
    if (strcmp (in, pair->name) == 0)
    {
      free (in);
      return strdup (pair->value);
    }
    pair = pair->next;
  }
  return in;
}

static bool transformResult
  (edgex_device_resultvalue *value, edgex_propertyvalue *props)
{
  if (props->type == Float64 || props->type == Float32)
  {
    long double result =
      (props->type == Float64) ? value->f64_result : value->f32_result;
    if (props->base.enabled) result = powl (props->base.value.dval, result);
    if (props->scale.enabled) result *= props->scale.value.dval;
    if (props->offset.enabled) result += props->offset.value.dval;
    if (props->type == Float64)
    {
      if (result <= DBL_MAX && result >= -DBL_MAX)
      {
        value->f64_result = (double)result;
        return true;
      }
      return false;
    }
    else
    {
      if (result <= FLT_MAX && result >= -FLT_MAX)
      {
        value->f32_result = (float)result;
        return true;
      }
      return false;
    }
  }
  else
  {
    long long int result = 0;
    switch (props->type)
    {
      case Uint8: result = value->ui8_result; break;
      case Uint16: result = value->ui16_result; break;
      case Uint32: result = value->ui32_result; break;
      case Uint64: result = value->ui64_result; break;
      case Int8: result = value->i8_result; break;
      case Int16: result = value->i16_result; break;
      case Int32: result = value->i32_result; break;
      case Int64: result = value->i64_result; break;
      default: assert (0);
    }
    if (props->mask.enabled) result &= props->mask.value.ival;
    if (props->shift.enabled)
    {
      if (props->shift.value.ival < 0)
      {
        result <<= -props->shift.value.ival;
      }
      else
      {
        result >>= props->shift.value.ival;
      }
    }
    if (props->base.enabled) result = powl (props->base.value.ival, result);
    if (props->scale.enabled) result *= props->scale.value.ival;
    if (props->offset.enabled) result += props->offset.value.ival;

    switch (props->type)
    {
      case Uint8:
        if (result >= 0 && result <= UCHAR_MAX)
        {
          value->ui8_result = (uint8_t)result;
          return true;
        }
        break;
      case Uint16:
        if (result >= 0 && result <= USHRT_MAX)
        {
          value->ui16_result = (uint16_t)result;
          return true;
        }
        break;
      case Uint32:
        if (result >= 0 && result <= UINT_MAX)
        {
          value->ui32_result = (uint32_t)result;
          return true;
        }
        break;
      case Uint64:
        if (result >= 0 && result <= ULLONG_MAX)
        {
          value->ui64_result = (uint64_t)result;
          return true;
        }
        break;
      case Int8:
        if (result >= SCHAR_MIN && result <= SCHAR_MAX)
        {
          value->i8_result = (int8_t)result;
          return true;
        }
        break;
      case Int16:
        if (result >= SHRT_MIN && result <= SHRT_MAX)
        {
          value->i16_result = (int16_t)result;
          return true;
        }
        break;
      case Int32:
        if (result >= INT_MIN && result <= INT_MAX)
        {
          value->i32_result = (int32_t)result;
          return true;
        }
        break;
      case Int64:
        if (result >= LLONG_MIN && result <= LLONG_MAX)
        {
          value->i64_result = (int64_t)result;
          return true;
        }
        break;
      default:
        assert (0);
    }
  }
  return false;
}

char *edgex_value_tostring
(
  edgex_device_resultvalue value,
  bool xform,
  edgex_propertyvalue *props,
  edgex_nvpairs *mappings
)
{
  size_t sz;
  char *res = NULL;

  if (isNumericType (props->type))
  {
    if (xform)
    {
      if (props->offset.enabled || props->scale.enabled ||
          props->base.enabled || props->shift.enabled || props->mask.enabled)
      {
        if (!transformResult (&value, props))
        {
          return strdup ("overflow");
        }
      }
    }

    res = malloc (32);
  }

  switch (props->type)
  {
    case Bool:
      res = strdup (value.bool_result ? "true" : "false");
      break;
    case Uint8:
      sprintf (res, "%" PRIu8, value.ui8_result);
      break;
    case Uint16:
      sprintf (res, "%" PRIu16, value.ui16_result);
      break;
    case Uint32:
      sprintf (res, "%" PRIu32, value.ui32_result);
      break;
    case Uint64:
      sprintf (res, "%" PRIu64, value.ui64_result);
      break;
    case Int8:
      sprintf (res, "%" PRIi8, value.i8_result);
      break;
    case Int16:
      sprintf (res, "%" PRIi16, value.i16_result);
      break;
    case Int32:
      sprintf (res, "%" PRIi32, value.i32_result);
      break;
    case Int64:
      sprintf (res, "%" PRIi64, value.i64_result);
      break;
    case Float32:
      sprintf (res, "%.8e", value.f32_result);
      break;
    case Float64:
      sprintf (res, "%.16e", value.f64_result);
      break;
    case String:
      res = xform ?
        checkMapping (value.string_result, mappings) :
        value.string_result;
      break;
    case Binary:
      sz = edgex_b64_encodesize (value.binary_result.size);
      res = malloc (sz);
      edgex_b64_encode
        (value.binary_result.bytes, value.binary_result.size, res, sz);
      free (value.binary_result.bytes);
      break;
  }
  return res;
}

static bool populateValue
  (edgex_device_commandresult *cres, const char *val)
{
  size_t sz;
  switch (cres->type)
  {
    case String:
      cres->value.string_result = strdup (val);
      return true;
    case Bool:
      if (strcasecmp (val, "true") == 0)
      {
        cres->value.bool_result = true;
        return true;
      }
      if (strcasecmp (val, "false") == 0)
      {
        cres->value.bool_result = false;
        return true;
      }
      return false;
    case Uint8:
      return (sscanf (val, "%" SCNu8, &cres->value.ui8_result) == 1);
    case Uint16:
      return (sscanf (val, "%" SCNu16, &cres->value.ui16_result) == 1);
    case Uint32:
      return (sscanf (val, "%" SCNu32, &cres->value.ui32_result) == 1);
    case Uint64:
      return (sscanf (val, "%" SCNu64, &cres->value.ui64_result) == 1);
    case Int8:
      return (sscanf (val, "%" SCNi8, &cres->value.i8_result) == 1);
    case Int16:
      return (sscanf (val, "%" SCNi16, &cres->value.i16_result) == 1);
    case Int32:
      return (sscanf (val, "%" SCNi32, &cres->value.i32_result) == 1);
    case Int64:
      return (sscanf (val, "%" SCNi64, &cres->value.i64_result) == 1);
    case Float32:
      return (sscanf (val, "%e", &cres->value.f32_result) == 1);
    case Float64:
      return (sscanf (val, "%le", &cres->value.f64_result) == 1);
    case Binary:
      sz = edgex_b64_maxdecodesize (val);
      cres->value.binary_result.size = sz;
      cres->value.binary_result.bytes = malloc (sz);
      return edgex_b64_decode
        (val, cres->value.binary_result.bytes, &cres->value.binary_result.size);
  }
  return false;
}

static const edgex_command *findCommand
  (const char *name, const edgex_command *cmd)
{
  while (cmd && strcmp (cmd->name, name))
  {
    cmd = cmd->next;
  }
  return cmd;
}

static edgex_deviceobject *findDevObj
  (edgex_deviceobject *list, const char *name)
{
  while (list && strcmp (list->name, name))
  {
    list = list->next;
  }
  return list;
}

static int runOnePut
(
  edgex_device_service *svc,
  edgex_device *dev,
  uint32_t nops,
  edgex_resourceoperation *ops,
  const char *data,
  JSON_Value **reply
)
{
  const char *value;
  int retcode = MHD_HTTP_OK;

  JSON_Value *jval = json_parse_string (data);
  if (jval == NULL)
  {
    iot_log_error (svc->logger, "Payload did not parse as JSON");
    return MHD_HTTP_BAD_REQUEST;
  }

  JSON_Object *jobj = json_value_get_object (jval);

  edgex_device_commandrequest *reqs =
    malloc (nops * sizeof (edgex_device_commandrequest));
  memset (reqs, 0, nops * sizeof (edgex_device_commandrequest));
  edgex_device_commandresult *results =
    malloc (nops * sizeof (edgex_device_commandresult));
  memset (results, 0, nops * sizeof (edgex_device_commandresult));
  edgex_resourceoperation *op = ops;
  for (int i = 0; i < nops; i++)
  {
    reqs[i].ro = op;
    reqs[i].devobj = findDevObj (dev->profile->device_resources, op->object);
    if (!reqs[i].devobj->properties->value->writable)
    {
      iot_log_error
      (
        svc->logger,
        "Attempt to write unwritable value %s",
        reqs[i].devobj->name
      );
      retcode = MHD_HTTP_METHOD_NOT_ALLOWED;
      break;
    }

    value = json_object_get_string (jobj, op->object);
    if (value == NULL)
    {
      retcode = MHD_HTTP_BAD_REQUEST;
      iot_log_error (svc->logger, "No value supplied for %s", op->object);
      break;
    }
    results[i].type = reqs[i].devobj->properties->value->type;
    if (!populateValue (&results[i], value))
    {
      retcode = MHD_HTTP_BAD_REQUEST;
      iot_log_error
        (svc->logger, "Unable to parse \"%s\" for %s", value, op->object);
      break;
    }
    op = op->next;
  }

  if (retcode == MHD_HTTP_OK)
  {
    if (!svc->userfns.puthandler (svc->userdata, dev->addressable, nops, reqs, results))
    {
      retcode = MHD_HTTP_INTERNAL_SERVER_ERROR;
      iot_log_error (svc->logger, "Driver for %s failed on PUT", dev->name);
    }
  }

  for (int i = 0; i < nops; i++)
  {
    if (results[i].type == String)
    {
      free (results[i].value.string_result);
    }
    else if (results[i].type == Binary)
    {
      free (results[i].value.binary_result.bytes);
    }
  }
  free (reqs);
  free (results);
  json_value_free (jval);

  return retcode;
}

static int runOneGet
(
  edgex_device_service *svc,
  edgex_device *dev,
  uint32_t nops,
  edgex_resourceoperation *ops,
  JSON_Value **reply
)
{
  int retcode = MHD_HTTP_INTERNAL_SERVER_ERROR;
  edgex_device_commandrequest *requests =
    malloc (nops * sizeof (edgex_device_commandrequest));
  memset (requests, 0, nops * sizeof (edgex_device_commandrequest));
  edgex_device_commandresult *results =
    malloc (nops * sizeof (edgex_device_commandresult));
  memset (results, 0, nops * sizeof (edgex_device_commandresult));
  edgex_resourceoperation *op = ops;
  for (int i = 0; i < nops; i++)
  {
    requests[i].ro = op;
    requests[i].devobj =
      findDevObj (dev->profile->device_resources, op->object);
    if (!requests[i].devobj->properties->value->readable)
    {
      iot_log_error
      (
        svc->logger,
        "Attempt to read unreadable value %s",
        requests[i].devobj->name
      );
      free (results);
      free (requests);
      return MHD_HTTP_METHOD_NOT_ALLOWED;
    }
    op = op->next;
  }

  if
  (
    svc->userfns.gethandler
      (svc->userdata, dev->addressable, nops, requests, results)
  )
  {
    *reply = edgex_data_generate_event
      (dev->name, nops, requests, results, svc->config.device.datatransform);

    if (*reply)
    {
      edgex_error err = EDGEX_OK;
      edgex_data_client_add_event
        (svc->logger, &svc->config.endpoints, *reply, &err);
      if (err.code == 0)
      {
        retcode = MHD_HTTP_OK;
      }
    }
    else
    {
      edgex_error err = EDGEX_OK;
      iot_log_error (svc->logger, "Assertion failed for device %s. Disabling.", dev->name);
      edgex_metadata_client_set_device_opstate
        (svc->logger, &svc->config.endpoints, dev->id, DISABLED, &err);
    }
  }
  else
  {
    iot_log_error
      (svc->logger, "Driver for %s failed on GET", dev->name);
  }
  free (results);
  free (requests);
  return retcode;
}

static int runOne
(
  edgex_device_service *svc,
  edgex_device *dev,
  const edgex_command *command,
  edgex_http_method method,
  const char *upload_data,
  size_t upload_data_size,
  JSON_Value **reply
)
{
  if (dev->adminState == LOCKED)
  {
    iot_log_error
    (
      svc->logger,
      "Can't run command %s on device %s as it is locked",
      command->name, dev->name
    );
    return MHD_HTTP_LOCKED;
  }

  if (dev->operatingState == DISABLED)
  {
    iot_log_error
    (
      svc->logger,
      "Can't run command %s on device %s as it is disabled",
      command->name, dev->name
    );
    return MHD_HTTP_LOCKED;
  }

  if
  (
    (method == GET && command->get == NULL) ||
    (method != GET && command->put == NULL)
  )
  {
    iot_log_error
    (
      svc->logger,
      "Command %s found for device %s but no %s version",
      command->name, dev->name, methStr (method)
    );
    return MHD_HTTP_METHOD_NOT_ALLOWED;
  }

  edgex_profileresource *res = dev->profile->resources;
  while (res)
  {
    if (strcmp (res->name, command->name) == 0)
    { break; }
    res = res->next;
  }
  if (res == NULL)
  {
    iot_log_error
    (
      svc->logger,
      "Profile resource %s not found for device %s",
      command->name, dev->name
    );
    return MHD_HTTP_NOT_FOUND;
  }

  edgex_resourceoperation *op;
  uint32_t n = 0;
  for (op = (method == GET) ? res->get : res->set; op; op = op->next)
  {
    if (findDevObj (dev->profile->device_resources, op->object) == NULL)
    {
      iot_log_error
      (
        svc->logger,
        "No device object %s for device %s",
        op->object, dev->name
      );
      return MHD_HTTP_NOT_FOUND;
    }
    n++;
  }
  if (n > svc->config.device.maxcmdops)
  {
    iot_log_error
    (
      svc->logger,
      "MaxCmdOps (%d) exceeded for dev: %s cmd: %s method: %s",
      svc->config.device.maxcmdops, dev->name, command->name,
      methStr (method)
    );
    return MHD_HTTP_INTERNAL_SERVER_ERROR;
  }

  if (method == GET)
  {
    return runOneGet (svc, dev, n, res->get, reply);
  }
  else
  {
    if (upload_data_size == 0)
    {
      iot_log_error (svc->logger, "PUT command recieved with no data");
      return MHD_HTTP_BAD_REQUEST;
    }
    return runOnePut (svc, dev, n, res->set, upload_data, reply);
  }
}

typedef struct devlist
{
   edgex_device *dev;
   const edgex_command *cmd;
   struct devlist *next;
} devlist;

static int allCommand
(
  edgex_device_service *svc,
  const char *cmd,
  edgex_http_method method,
  const char *upload_data,
  size_t upload_data_size,
  char **reply,
  const char **reply_type
)
{
  const char *key;
  edgex_device *dev;
  const edgex_command *command;
  int ret = MHD_HTTP_NOT_FOUND;
  int retOne;
  JSON_Value *jresult;
  JSON_Array *jarray;
  devlist *devs = NULL;
  devlist *d;

  iot_log_debug
    (svc->logger, "Incoming %s command %s for all", methStr (method), cmd);

  jresult = json_value_init_array ();
  jarray = json_value_get_array (jresult);

  pthread_rwlock_rdlock (&svc->deviceslock);
  edgex_map_iter iter = edgex_map_iter (svc->devices);
  while ((key = edgex_map_next (&svc->devices, &iter)))
  {
    dev = *edgex_map_get (&svc->devices, key);
    if (dev->operatingState == ENABLED && dev->adminState == UNLOCKED)
    {
      command = findCommand (cmd, dev->profile->commands);
      if (command)
      {
        d = malloc (sizeof (devlist));
        d->dev = dev;
        d->cmd = command;
        d->next = devs;
        devs = d;
      }
    }
  }
  pthread_rwlock_unlock (&svc->deviceslock);

  uint32_t nret = 0;
  uint32_t maxret = svc->config.service.readmaxlimit;

  for (d = devs; d; d = d->next)
  {
    JSON_Value *jreply = NULL;
    retOne = runOne
      (svc, d->dev, d->cmd, method, upload_data, upload_data_size, &jreply);
    if (jreply && (maxret == 0 || nret++ < maxret))
    {
      json_array_append_value (jarray, jreply);
    }
    if (ret != MHD_HTTP_OK)
    {
      ret = retOne;
    }
  }

  if (ret == MHD_HTTP_OK)
  {
    *reply = json_serialize_to_string (jresult);
    *reply_type = "application/json";
  }
  json_value_free (jresult);
  while (devs)
  {
    d = devs->next;
    free (devs);
    devs = d;
  }
  return ret;
}

static int oneCommand
(
  edgex_device_service *svc,
  const char *id,
  bool byName,
  const char *cmd,
  edgex_http_method method,
  const char *upload_data,
  size_t upload_data_size,
  char **reply,
  const char **reply_type
)
{
  int result = MHD_HTTP_NOT_FOUND;
  edgex_device **dev = NULL;

  iot_log_debug
  (
    svc->logger,
    "Incoming command for device {%s}: %s (%s)",
    id, cmd, methStr (method)
  );

  pthread_rwlock_rdlock (&svc->deviceslock);
  if (byName)
  {
    char **found = edgex_map_get (&svc->name_to_id, id);
    if (found)
    {
      dev = edgex_map_get (&svc->devices, *found);
    }
  }
  else
  {
    dev = edgex_map_get (&svc->devices, id);
  }
  pthread_rwlock_unlock (&svc->deviceslock);
  if (dev)
  {
    const edgex_command *command = findCommand (cmd, (*dev)->profile->commands);
    if (command)
    {
      JSON_Value *jreply = NULL;
      result = runOne
        (svc, *dev, command, method, upload_data, upload_data_size, &jreply);
      if (jreply)
      {
        *reply = json_serialize_to_string (jreply);
        *reply_type = "application/json";
        json_value_free (jreply);
      }
    }
    else
    {
      iot_log_error
        (svc->logger, "Command %s not found for device %s", cmd, (*dev)->name);
    }
  }
  else
  {
    iot_log_error (svc->logger, "No such device {%s}", id);
  }
  return result;
}

int edgex_device_handler_device
(
  void *ctx,
  char *url,
  edgex_http_method method,
  const char *upload_data,
  size_t upload_data_size,
  char **reply,
  const char **reply_type
)
{
  int result = MHD_HTTP_NOT_FOUND;
  char *cmd;
  edgex_device_service *svc = (edgex_device_service *) ctx;

  if (strlen (url) == 0)
  {
    iot_log_error (svc->logger, "No device specified in url");
  }
  else
  {
    if (strncmp (url, "all/", 4) == 0)
    {
      cmd = url + 4;
      if (strlen (cmd))
      {
        result = allCommand
          (svc, cmd, method, upload_data, upload_data_size, reply, reply_type);
      }
      else
      {
        iot_log_error (svc->logger, "No command specified in url");
      }
    }
    else
    {
      bool byName = false;
      if (strncmp (url, "name/", 4) == 0)
      {
        byName = true;
        url += 5;
      }
      cmd = strchr (url, '/');
      if (cmd == NULL || strlen (cmd + 1) == 0)
      {
        iot_log_error (svc->logger, "No command specified in url");
      }
      else
      {
         *cmd = '\0';
         result = oneCommand
         (
           svc,
           url, byName, cmd + 1, method,
           upload_data, upload_data_size,
           reply, reply_type
         );
         *cmd = '/';
      }
    }
  }
  return result;
}
