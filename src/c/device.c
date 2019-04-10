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
#include "cmdinfo.h"
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
#define BUFSIZE 32

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

    res = malloc (BUFSIZE);
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
      if (props->floatAsBinary)
      {
        edgex_b64_encode (&value.f32_result, sizeof (float), res, BUFSIZE);
      }
      else
      {
        sprintf (res, "%.8e", value.f32_result);
      }
      break;
    case Float64:
      if (props->floatAsBinary)
      {
        edgex_b64_encode (&value.f64_result, sizeof (double), res, BUFSIZE);
      }
      else
      {
        sprintf (res, "%.16e", value.f64_result);
      }
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
      if (strlen (val) == 8 && val[6] == '=' && val[7] == '=')
      {
        size_t sz = sizeof (float);
        return edgex_b64_decode (val, &cres->value.f32_result, &sz);
      }
      else
      {
        return (sscanf (val, "%e", &cres->value.f32_result) == 1);
      }
    case Float64:
      if (strlen (val) == 12 && val[11] == '=')
      {
        size_t sz = sizeof (double);
        return edgex_b64_decode (val, &cres->value.f64_result, &sz);
      }
      else
      {
        return (sscanf (val, "%le", &cres->value.f64_result) == 1);
      }
    case Binary:
      sz = edgex_b64_maxdecodesize (val);
      cres->value.binary_result.size = sz;
      cres->value.binary_result.bytes = malloc (sz);
      return edgex_b64_decode
        (val, cres->value.binary_result.bytes, &cres->value.binary_result.size);
  }
  return false;
}

static edgex_deviceresource *findDevResource
  (edgex_deviceresource *list, const char *name)
{
  while (list && strcmp (list->name, name))
  {
    list = list->next;
  }
  return list;
}

static edgex_cmdinfo *infoForRes
  (edgex_deviceprofile *prof, edgex_profileresource *res, bool forGet)
{
  edgex_cmdinfo *result = malloc (sizeof (edgex_cmdinfo));
  result->name = res->name;
  result->isget = forGet;
  unsigned n = 0;
  edgex_resourceoperation *ro;
  for (ro = forGet ? res->get : res->set; ro; ro = ro->next)
  {
    n++;
  }
  result->nreqs = n;
  result->reqs = malloc (n * sizeof (edgex_device_commandrequest));
  result->pvals = malloc (n * sizeof (edgex_propertyvalue *));
  result->maps = malloc (n * sizeof (edgex_nvpairs *));
  for (n = 0, ro = forGet ? res->get : res->set; ro; n++, ro = ro->next)
  {
    edgex_deviceresource *devres =
      findDevResource (prof->device_resources, ro->object);
    result->reqs[n].resname = devres->name;
    result->reqs[n].attributes = devres->attributes;
    result->reqs[n].type = devres->properties->value->type;
    result->pvals[n] = devres->properties->value;
    result->maps[n] = ro->mappings;
  }
  result->next = NULL;
  return result;
}

static edgex_cmdinfo *infoForDevRes (edgex_deviceresource *devres, bool forGet)
{
  edgex_cmdinfo *result = malloc (sizeof (edgex_cmdinfo));
  result->name = devres->name;
  result->isget = forGet;
  result->nreqs = 1;
  result->reqs = malloc (sizeof (edgex_device_commandrequest));
  result->pvals = malloc (sizeof (edgex_propertyvalue *));
  result->maps = malloc (sizeof (edgex_nvpairs *));
  result->reqs[0].resname = devres->name;;
  result->reqs[0].attributes = devres->attributes;
  result->reqs[0].type = devres->properties->value->type;
  result->pvals[0] = devres->properties->value;
  result->maps[0] = NULL;
  result->next = NULL;
  return result;
}

static void populateCmdInfo (edgex_deviceprofile *prof)
{
  edgex_cmdinfo **head = &prof->cmdinfo;
  edgex_profileresource *res = prof->resources;
  while (res)
  {
    if (res->get)
    {
      *head = infoForRes (prof, res, true);
      head = &((*head)->next);
    }
    if (res->set)
    {
      *head = infoForRes (prof, res, false);
      head = &((*head)->next);
    }
    res = res->next;
  }
  edgex_deviceresource *devres = prof->device_resources;
  while (devres)
  {
    edgex_profileresource *r;
    for (r = prof->resources; r; r = r->next)
    {
      if (strcmp (r->name, devres->name) == 0)
      {
        break;
      }
    }
    if (r == NULL)
    {
      if (devres->properties->value->readable)
      {
        *head = infoForDevRes (devres, true);
        head = &((*head)->next);
      }
      if (devres->properties->value->writable)
      {
        *head = infoForDevRes (devres, false);
        head = &((*head)->next);
      }
    }
    devres = devres->next;
  }
}

const edgex_cmdinfo *edgex_deviceprofile_findcommand
  (const char *name, edgex_deviceprofile *prof, bool forGet)
{
  if (prof->cmdinfo == NULL)
  {
    populateCmdInfo (prof);
  }

  edgex_cmdinfo *result = prof->cmdinfo;
  while (result && (strcmp (result->name, name) || forGet != result->isget))
  {
    result = result->next;
  }
  return result;
}

static bool commandExists (const char *name, edgex_deviceprofile *prof)
{
  if (prof->cmdinfo == NULL)
  {
    populateCmdInfo (prof);
  }

  edgex_cmdinfo *result = prof->cmdinfo;
  while (result && strcmp (result->name, name))
  {
    result = result->next;
  }
  return result != NULL;
}

static int runOnePut
(
  edgex_device_service *svc,
  edgex_device *dev,
  const edgex_cmdinfo *commandinfo,
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

  edgex_device_commandresult *results =
    calloc (commandinfo->nreqs, sizeof (edgex_device_commandresult));
  for (int i = 0; i < commandinfo->nreqs; i++)
  {
    const char *resname = commandinfo->reqs[i].resname;
    if (!commandinfo->pvals[i]->writable)
    {
      iot_log_error
        (svc->logger, "Attempt to write unwritable value %s", resname);
      retcode = MHD_HTTP_METHOD_NOT_ALLOWED;
      break;
    }

    value = json_object_get_string (jobj, resname);
    if (value == NULL)
    {
      retcode = MHD_HTTP_BAD_REQUEST;
      iot_log_error (svc->logger, "No value supplied for %s", resname);
      break;
    }
    results[i].type = commandinfo->pvals[i]->type;
    if (!populateValue (&results[i], value))
    {
      retcode = MHD_HTTP_BAD_REQUEST;
      iot_log_error
        (svc->logger, "Unable to parse \"%s\" for %s", value, resname);
      break;
    }
  }

  if (retcode == MHD_HTTP_OK)
  {
    if (!svc->userfns.puthandler (svc->userdata, dev->name, dev->protocols, commandinfo->nreqs, commandinfo->reqs, results))
    {
      retcode = MHD_HTTP_INTERNAL_SERVER_ERROR;
      iot_log_error (svc->logger, "Driver for %s failed on PUT", dev->name);
    }
  }

  for (int i = 0; i < commandinfo->nreqs; i++)
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
  free (results);
  json_value_free (jval);

  return retcode;
}

static int runOneGet
(
  edgex_device_service *svc,
  edgex_device *dev,
  const edgex_cmdinfo *commandinfo,
  JSON_Value **reply
)
{
  int retcode = MHD_HTTP_INTERNAL_SERVER_ERROR;
  edgex_device_commandresult *results =
    calloc (commandinfo->nreqs, sizeof (edgex_device_commandresult));
  for (int i = 0; i < commandinfo->nreqs; i++)
  {
    if (!commandinfo->pvals[i]->readable)
    {
      iot_log_error
      (
        svc->logger,
        "Attempt to read unreadable value %s",
        commandinfo->reqs[i].resname
      );
      free (results);
      return MHD_HTTP_METHOD_NOT_ALLOWED;
    }
  }

  if
  (
    svc->userfns.gethandler
      (svc->userdata, dev->name, dev->protocols, commandinfo->nreqs, commandinfo->reqs, results)
  )
  {
    *reply = edgex_data_generate_event
      (dev->name, commandinfo, results, svc->config.device.datatransform);

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
  return retcode;
}

static int runOne
(
  edgex_device_service *svc,
  edgex_device *dev,
  const edgex_cmdinfo *command,
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

  if (command->nreqs > svc->config.device.maxcmdops)
  {
    iot_log_error
    (
      svc->logger,
      "MaxCmdOps (%d) exceeded for dev: %s cmd: %s",
      svc->config.device.maxcmdops, dev->name, command->name
    );
    return MHD_HTTP_INTERNAL_SERVER_ERROR;
  }

  if (command->isget)
  {
    return runOneGet (svc, dev, command, reply);
  }
  else
  {
    if (upload_data_size == 0)
    {
      iot_log_error (svc->logger, "PUT command recieved with no data");
      return MHD_HTTP_BAD_REQUEST;
    }
    return runOnePut (svc, dev, command, upload_data, reply);
  }
}

typedef struct devlist
{
   edgex_device *dev;
   const edgex_cmdinfo *cmd;
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
  int ret = MHD_HTTP_NOT_FOUND;
  int retOne;
  JSON_Value *jresult;
  JSON_Array *jarray;
  edgex_cmdqueue_t *cmdq = NULL;
  edgex_cmdqueue_t *iter;

  iot_log_debug
    (svc->logger, "Incoming %s command %s for all", methStr (method), cmd);

  jresult = json_value_init_array ();
  jarray = json_value_get_array (jresult);

  cmdq = edgex_devmap_device_forcmd (svc->devices, cmd, method == GET);

  uint32_t nret = 0;
  uint32_t maxret = svc->config.service.readmaxlimit;

  for (iter = cmdq; iter; iter = iter->next)
  {
    JSON_Value *jreply = NULL;
    retOne = runOne
      (svc, iter->dev, iter->cmd, upload_data, upload_data_size, &jreply);
    edgex_device_release (iter->dev);
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
  while (cmdq)
  {
    iter = cmdq->next;
    free (cmdq);
    cmdq = iter;
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
  edgex_device *dev = NULL;
  const edgex_cmdinfo *command = NULL;

  iot_log_debug
  (
    svc->logger,
    "Incoming command for device {%s}: %s (%s)",
    id, cmd, methStr (method)
  );

  if (byName)
  {
    dev = edgex_devmap_device_byname (svc->devices, id);
  }
  else
  {
    dev = edgex_devmap_device_byid (svc->devices, id);
  }

  if (dev)
  {
    command = edgex_deviceprofile_findcommand
      (cmd, dev->profile, method == GET);
  }

  if (command)
  {
    JSON_Value *jreply = NULL;
    result = runOne
      (svc, dev, command, upload_data, upload_data_size, &jreply);
    edgex_device_release (dev);
    if (jreply)
    {
      *reply = json_serialize_to_string (jreply);
      *reply_type = "application/json";
      json_value_free (jreply);
    }
  }
  else
  {
    if (dev)
    {
      if (commandExists (cmd, dev->profile))
      {
        iot_log_error
        (
          svc->logger,
          "Wrong method for command %s, device %s",
          cmd, dev->name
        );
        result = MHD_HTTP_METHOD_NOT_ALLOWED;
      }
      else
      {
        iot_log_error
          (svc->logger, "No command %s for device %s", cmd, dev->name);
      }
      edgex_device_release (dev);
    }
    else
    {
      iot_log_error (svc->logger, "No such device {%s}", id);
    }
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
