/*
 * Copyright (c) 2018-2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "device.h"
#include "api.h"
#include "service.h"
#include "errorlist.h"
#include "parson.h"
#include "data.h"
#include "metadata.h"
#include "edgex-rest.h"
#include "cmdinfo.h"
#include "iot/base64.h"
#include "transform.h"
#include "reqdata.h"

#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <microhttpd.h>

static const char *methStr (devsdk_http_method method)
{
  switch (method)
  {
    case DevSDK_Get:
      return "GET";
    case DevSDK_Post:
      return "POST";
    case DevSDK_Put:
      return "PUT";
    case DevSDK_Patch:
      return "PATCH";
    case DevSDK_Delete:
      return "DELETE";
    default:
      return "UNKNOWN";
  }
}

static bool readFloat32 (const char *val, float *f)
{
  if (strlen (val) == 8 && val[6] == '=' && val[7] == '=')
  {
    size_t sz = sizeof (float);
    return iot_b64_decode (val, f, &sz);
  }
  else
  {
    return (sscanf (val, "%e", f) == 1);
  }
}

static bool readFloat64 (const char *val, double *d)
{
  if (strlen (val) == 12 && val[11] == '=')
  {
    size_t sz = sizeof (double);
    return iot_b64_decode (val, d, &sz);
  }
  else
  {
    return (sscanf (val, "%le", d) == 1);
  }
}

iot_data_t *edgex_data_from_string (iot_data_type_t rtype, const char *val)
{
  switch (rtype)
  {
    case IOT_DATA_UINT8:
    {
      uint8_t i;
      return (sscanf (val, "%" SCNu8, &i) == 1) ? iot_data_alloc_ui8 (i) : NULL;
    }
    case IOT_DATA_INT8:
    {
      int8_t i;
      return (sscanf (val, "%" SCNi8, &i) == 1) ? iot_data_alloc_i8 (i) : NULL;
    }
    case IOT_DATA_UINT16:
    {
      uint16_t i;
      return (sscanf (val, "%" SCNu16, &i) == 1) ? iot_data_alloc_ui16 (i) : NULL;
    }
    case IOT_DATA_INT16:
    {
      int16_t i;
      return (sscanf (val, "%" SCNi16, &i) == 1) ? iot_data_alloc_i16 (i) : NULL;
    }
    case IOT_DATA_UINT32:
    {
      uint32_t i;
      return (sscanf (val, "%" SCNu32, &i) == 1) ? iot_data_alloc_ui32 (i) : NULL;
    }
    case IOT_DATA_INT32:
    {
      int32_t i;
      return (sscanf (val, "%" SCNi32, &i) == 1) ? iot_data_alloc_i32 (i) : NULL;
    }
    case IOT_DATA_UINT64:
    {
      uint64_t i;
      return (sscanf (val, "%" SCNu64, &i) == 1) ? iot_data_alloc_ui64 (i) : NULL;
    }
    case IOT_DATA_INT64:
    {
      int64_t i;
      return (sscanf (val, "%" SCNi64, &i) == 1) ? iot_data_alloc_i64 (i) : NULL;
    }
    case IOT_DATA_FLOAT32:
    {
      float f;
      return readFloat32 (val, &f) ? iot_data_alloc_f32 (f) : NULL;
    }
    case IOT_DATA_FLOAT64:
    {
      double d;
      return readFloat64 (val, &d) ? iot_data_alloc_f64 (d) : NULL;
    }
    case IOT_DATA_STRING:
      return iot_data_alloc_string (val, IOT_DATA_COPY);
    case IOT_DATA_BOOL:
      if (strcasecmp (val, "true") == 0)
      {
        return iot_data_alloc_bool (true);
      } else if (strcasecmp (val, "false") == 0)
      {
        return iot_data_alloc_bool (false);
      }
      else
      {
        return NULL;
      }
    default:
      return NULL;
  }
}

static iot_data_t *populateValue (edgex_propertytype rtype, const char *val)
{
  if (rtype <= Edgex_String)
  {
    return edgex_data_from_string (rtype, val);
  }
  else if (rtype == Edgex_Binary)
  {
    iot_data_t *res = iot_data_alloc_array_from_base64 (val);
    iot_data_t *b = iot_data_alloc_bool (true);
    iot_data_set_metadata (res, b);
    iot_data_free (b);
    return res;
  }

  JSON_Value *jval = NULL;
  JSON_Array *jarr = NULL;
  size_t jsz = 0;
  jval = json_parse_string (val);
  if (jval)
  {
    jarr = json_value_get_array (jval);
    if (jarr == NULL)
    {
      json_value_free (jval);
      return NULL;
    }
    jsz = json_array_get_count (jarr);
  }
  else
  {
    return NULL;
  }

  switch (rtype)
  {
    case Edgex_Int8Array:
    {
      int8_t *arr = malloc (jsz);
      for (size_t i = 0; i < jsz; i++)
      {
        if (sscanf (json_array_get_string (jarr, i), "%" SCNi8, &arr[i]) != 1)
        {
          free (arr);
          json_value_free (jval);
          return NULL;
        }
      }
      json_value_free (jval);
      return iot_data_alloc_array (arr, jsz, IOT_DATA_INT8, IOT_DATA_TAKE);
    }
    case Edgex_Uint8Array:
    {
      uint8_t *arr = malloc (jsz);
      for (size_t i = 0; i < jsz; i++)
      {
        if (sscanf (json_array_get_string (jarr, i), "%" SCNu8, &arr[i]) != 1)
        {
          free (arr);
          json_value_free (jval);
          return NULL;
        }
      }
      json_value_free (jval);
      return iot_data_alloc_array (arr, jsz, IOT_DATA_UINT8, IOT_DATA_TAKE);
    }
    case Edgex_Int16Array:
    {
      int16_t *arr = malloc (2 * jsz);
      for (size_t i = 0; i < jsz; i++)
      {
        if (sscanf (json_array_get_string (jarr, i), "%" SCNi16, &arr[i]) != 1)
        {
          free (arr);
          json_value_free (jval);
          return NULL;
        }
      }
      json_value_free (jval);
      return iot_data_alloc_array (arr, jsz, IOT_DATA_INT16, IOT_DATA_TAKE);
    }
    case Edgex_Uint16Array:
    {
      uint16_t *arr = malloc (2 * jsz);
      for (size_t i = 0; i < jsz; i++)
      {
        if (sscanf (json_array_get_string (jarr, i), "%" SCNu16, &arr[i]) != 1)
        {
          free (arr);
          json_value_free (jval);
          return NULL;
        }
      }
      json_value_free (jval);
      return iot_data_alloc_array (arr, jsz, IOT_DATA_UINT16, IOT_DATA_TAKE);
    }
    case Edgex_Int32Array:
    {
      int32_t *arr = malloc (4 * jsz);
      for (size_t i = 0; i < jsz; i++)
      {
        if (sscanf (json_array_get_string (jarr, i), "%" SCNi32, &arr[i]) != 1)
        {
          free (arr);
          json_value_free (jval);
          return NULL;
        }
      }
      json_value_free (jval);
      return iot_data_alloc_array (arr, jsz, IOT_DATA_INT32, IOT_DATA_TAKE);
    }
    case Edgex_Uint32Array:
    {
      uint32_t *arr = malloc (4 * jsz);
      for (size_t i = 0; i < jsz; i++)
      {
        if (sscanf (json_array_get_string (jarr, i), "%" SCNu32, &arr[i]) != 1)
        {
          free (arr);
          json_value_free (jval);
          return NULL;
        }
      }
      json_value_free (jval);
      return iot_data_alloc_array (arr, jsz, IOT_DATA_UINT32, IOT_DATA_TAKE);
    }
    case Edgex_Int64Array:
    {
      int64_t *arr = malloc (8 * jsz);
      for (size_t i = 0; i < jsz; i++)
      {
        if (sscanf (json_array_get_string (jarr, i), "%" SCNi64, &arr[i]) != 1)
        {
          free (arr);
          json_value_free (jval);
          return NULL;
        }
      }
      json_value_free (jval);
      return iot_data_alloc_array (arr, jsz, IOT_DATA_INT64, IOT_DATA_TAKE);
    }
    case Edgex_Uint64Array:
    {
      uint64_t *arr = malloc (8 * jsz);
      for (size_t i = 0; i < jsz; i++)
      {
        if (sscanf (json_array_get_string (jarr, i), "%" SCNu64, &arr[i]) != 1)
        {
          free (arr);
          json_value_free (jval);
          return NULL;
        }
      }
      json_value_free (jval);
      return iot_data_alloc_array (arr, jsz, IOT_DATA_UINT64, IOT_DATA_TAKE);
    }
    case Edgex_Float32Array:
    {
      float *arr = malloc (jsz * sizeof (float));
      for (size_t i = 0; i < jsz; i++)
      {
        if (!readFloat32 (json_array_get_string (jarr, i), &arr[i]))
        {
          free (arr);
          json_value_free (jval);
          return NULL;
        }
      }
      json_value_free (jval);
      return iot_data_alloc_array (arr, jsz, IOT_DATA_FLOAT32, IOT_DATA_TAKE);
    }
    case Edgex_Float64Array:
    {
      double *arr = malloc (jsz * sizeof (double));
      for (size_t i = 0; i < jsz; i++)
      {
        if (!readFloat64 (json_array_get_string (jarr, i), &arr[i]))
        {
          free (arr);
          json_value_free (jval);
          return NULL;
        }
      }
      json_value_free (jval);
      return iot_data_alloc_array (arr, jsz, IOT_DATA_FLOAT64, IOT_DATA_TAKE);
    }
    case Edgex_BoolArray:
    {
      bool *arr = malloc (jsz * sizeof (bool));
      for (size_t i = 0; i < jsz; i++)
      {
        const char *bval = json_array_get_string (jarr, i);
        if (strcasecmp (bval, "true") == 0)
        {
          arr[i] = true;
        }
        else if (strcasecmp (bval, "false") == 0)
        {
          arr[i] = false;
        }
        else
        {
          free (arr);
          json_value_free (jval);
          return NULL;
        }
      }
      json_value_free (jval);
      return iot_data_alloc_array (arr, jsz, IOT_DATA_BOOL, IOT_DATA_TAKE);
    }
    default:
      return NULL;
  }
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

static iot_typecode_t *typecodeFromType (edgex_propertytype pt)
{
  if (pt <= Edgex_String)
  {
    return iot_typecode_alloc_basic (pt);
  }
  if (pt == Edgex_Binary)
  {
    return iot_typecode_alloc_array (IOT_DATA_UINT8);
  }
  else
  {
    return iot_typecode_alloc_array (pt - (Edgex_Int8Array - Edgex_Int8));
  }
}

static edgex_cmdinfo *infoForRes (edgex_deviceprofile *prof, edgex_devicecommand *cmd, bool forGet)
{
  edgex_cmdinfo *result = malloc (sizeof (edgex_cmdinfo));
  result->name = cmd->name;
  result->profile = prof;
  result->isget = forGet;
  unsigned n = 0;
  edgex_resourceoperation *ro;
  for (ro = cmd->resourceOperations; ro; ro = ro->next)
  {
    n++;
  }
  result->nreqs = n;
  result->reqs = calloc (n, sizeof (devsdk_commandrequest));
  result->pvals = calloc (n, sizeof (edgex_propertyvalue *));
  result->maps = calloc (n, sizeof (devsdk_nvpairs *));
  result->dfls = calloc (n, sizeof (char *));
  for (n = 0, ro = cmd->resourceOperations; ro; n++, ro = ro->next)
  {
    edgex_deviceresource *devres =
      findDevResource (prof->device_resources, ro->deviceResource);
    result->reqs[n].resname = devres->name;
    result->reqs[n].attributes = devres->attributes;
    result->reqs[n].type = typecodeFromType (devres->properties->type);
    if (devres->properties->mask.enabled)
    {
      result->reqs[n].mask = ~devres->properties->mask.value.ival;
    }
    result->pvals[n] = devres->properties;
    result->maps[n] = ro->mappings;
    if (ro->defaultValue && *ro->defaultValue)
    {
      result->dfls[n] = ro->defaultValue;
    }
    else if (devres->properties->defaultvalue && *devres->properties->defaultvalue)
    {
      result->dfls[n] = devres->properties->defaultvalue;
    }
  }
  result->next = NULL;
  return result;
}

static edgex_cmdinfo *infoForDevRes (edgex_deviceprofile *prof, edgex_deviceresource *devres, bool forGet)
{
  edgex_cmdinfo *result = malloc (sizeof (edgex_cmdinfo));
  result->name = devres->name;
  result->profile = prof;
  result->isget = forGet;
  result->nreqs = 1;
  result->reqs = malloc (sizeof (devsdk_commandrequest));
  result->pvals = malloc (sizeof (edgex_propertyvalue *));
  result->maps = malloc (sizeof (devsdk_nvpairs *));
  result->dfls = malloc (sizeof (char *));
  result->reqs[0].resname = devres->name;
  result->reqs[0].attributes = devres->attributes;
  result->reqs[0].type = typecodeFromType (devres->properties->type);
  if (devres->properties->mask.enabled)
  {
    result->reqs[0].mask = ~devres->properties->mask.value.ival;
  }
  result->pvals[0] = devres->properties;
  result->maps[0] = NULL;
  if (devres->properties->defaultvalue && *devres->properties->defaultvalue)
  {
    result->dfls[0] = devres->properties->defaultvalue;
  }
  else
  {
    result->dfls[0] = NULL;
  }
  result->next = NULL;
  return result;
}

static void populateCmdInfo (edgex_deviceprofile *prof)
{
  edgex_cmdinfo **head = &prof->cmdinfo;
  edgex_devicecommand *cmd = prof->device_commands;
  while (cmd)
  {
    if (cmd->readable)
    {
      *head = infoForRes (prof, cmd, true);
      head = &((*head)->next);
    }
    if (cmd->writable)
    {
      *head = infoForRes (prof, cmd, false);
      head = &((*head)->next);
    }
    cmd = cmd->next;
  }
  edgex_deviceresource *devres = prof->device_resources;
  while (devres)
  {
    edgex_devicecommand *dc;
    for (dc = prof->device_commands; dc; dc = dc->next)
    {
      if (strcmp (dc->name, devres->name) == 0)
      {
        break;
      }
    }
    if (dc == NULL)
    {
      if (devres->properties->readable)
      {
        *head = infoForDevRes (prof, devres, true);
        head = &((*head)->next);
      }
      if (devres->properties->writable)
      {
        *head = infoForDevRes (prof, devres, false);
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

static void edgex_device_runput2
  (devsdk_service_t *svc, edgex_device *dev, const edgex_cmdinfo *cmdinfo, const devsdk_nvpairs *params, const edgex_reqdata_t *rdata, devsdk_http_reply *reply)
{
  reply->code = MHD_HTTP_OK;
  iot_data_t **results = calloc (cmdinfo->nreqs, sizeof (iot_data_t *));
  for (int i = 0; i < cmdinfo->nreqs; i++)
  {
    const char *resname = cmdinfo->reqs[i].resname;
    if (!cmdinfo->pvals[i]->writable)
    {
      edgex_error_response (svc->logger, reply, MHD_HTTP_METHOD_NOT_ALLOWED, "Attempt to write unwritable value %s", resname);
      break;
    }

    if (cmdinfo->pvals[i]->type != Edgex_Binary)
    {
      const char *value = edgex_reqdata_get (rdata, resname, cmdinfo->dfls[i]);
      if (value == NULL)
      {
        edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "No value supplied for %s", resname);
        break;
      }

      results[i] = populateValue (cmdinfo->pvals[i]->type, value);
      if (!results[i])
      {
        edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "Unable to parse \"%s\" for %s", value ? value : cmdinfo->dfls[i], resname);
        break;
      }

      if (svc->config.device.datatransform)
      {
        edgex_transform_incoming (&results[i], cmdinfo->pvals[i], cmdinfo->maps[i]);
        if (!results[i])
        {
          edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "Value \"%s\" for %s overflows after transformations", value, resname);
          break;
        }
      }
      if (!edgex_transform_validate (results[i], cmdinfo->pvals[i]))
      {
        edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "Value \"%s\" for %s out of range specified in profile", value, resname);
        break;
      }
    }
    else
    {
      results[i] = edgex_reqdata_get_binary (rdata, resname);
      if (results[i] == NULL)
      {
        edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "No value supplied for %s", resname);
      }
    }
  }

  if (reply->code == MHD_HTTP_OK)
  {
    iot_data_t *e = NULL;
    if (svc->userfns.puthandler (svc->userdata, dev->name, dev->protocols, cmdinfo->nreqs, cmdinfo->reqs, (const iot_data_t **)results, params, &e))
    {
      edgex_baseresponse br;
      edgex_baseresponse_populate (&br, "v2", MHD_HTTP_OK, "Data written successfully");
      edgex_baseresponse_write (&br, reply);
      if (svc->config.device.updatelastconnected)
      {
        devsdk_error err;
        edgex_metadata_client_update_lastconnected (svc->logger, &svc->config.endpoints, dev->name, &err);
      }
    }
    else
    {
      edgex_error_response
        (svc->logger, reply, MHD_HTTP_INTERNAL_SERVER_ERROR, "Driver for %s failed on PUT: %s", dev->name, e ? iot_data_to_json (e) : "(unknown)");
    }
    iot_data_free (e);
  }

  for (int i = 0; i < cmdinfo->nreqs; i++)
  {
    iot_data_free (results[i]);
  }
  free (results);
}

static edgex_event_cooked *edgex_device_runget2
  (devsdk_service_t *svc, edgex_device *dev, const edgex_cmdinfo *cmdinfo, const devsdk_nvpairs *params, devsdk_http_reply *reply)
{
  for (int i = 0; i < cmdinfo->nreqs; i++)
  {
    if (!cmdinfo->pvals[i]->readable)
    {
      edgex_error_response (svc->logger, reply, MHD_HTTP_METHOD_NOT_ALLOWED, "Attempt to read unreadable value %s", cmdinfo->reqs[i].resname);
      return NULL;
    }
  }

  edgex_event_cooked *result = NULL;
  iot_data_t *e = NULL;
  devsdk_commandresult *results = calloc (cmdinfo->nreqs, sizeof (devsdk_commandresult));

  if (svc->userfns.gethandler (svc->userdata, dev->name, dev->protocols, cmdinfo->nreqs, cmdinfo->reqs, results, params, &e))
  {
    devsdk_error err = EDGEX_OK;
    result = edgex_data_process_event (dev->name, cmdinfo, results, svc->config.device.datatransform);

    if (result)
    {
      if (svc->config.device.updatelastconnected)
      {
        edgex_metadata_client_update_lastconnected (svc->logger, &svc->config.endpoints, dev->name, &err);
      }
    }
    else
    {
      edgex_error_response (svc->logger, reply, MHD_HTTP_INTERNAL_SERVER_ERROR, "Assertion failed for device %s. Marking as down.", dev->name);
      edgex_metadata_client_set_device_opstate (svc->logger, &svc->config.endpoints, dev->name, DOWN, &err);
    }
  }
  else
  {
    edgex_error_response
      (svc->logger, reply, MHD_HTTP_INTERNAL_SERVER_ERROR, "Driver for %s failed on GET: ", dev->name, e ? iot_data_to_json (e) : "(unknown)");
  }

  iot_data_free (e);
  devsdk_commandresult_free (results, cmdinfo->nreqs);

  return result;
}

static void edgex_device_v2impl (devsdk_service_t *svc, edgex_device *dev, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  const char *cmdname = devsdk_nvpairs_value (req->params, "cmd");
  const edgex_cmdinfo *cmd = edgex_deviceprofile_findcommand (cmdname, dev->profile, req->method == DevSDK_Get);
  reply->code = MHD_HTTP_OK;

  if (!cmd)
  {
    if (edgex_deviceprofile_findcommand (cmdname, dev->profile, req->method != DevSDK_Get))
    {
      edgex_error_response (svc->logger, reply, MHD_HTTP_METHOD_NOT_ALLOWED, "Wrong method for command %s (operation is %s-only)", cmdname, req->method == DevSDK_Get ? "write" : "read");
    }
    else
    {
      edgex_error_response (svc->logger, reply, MHD_HTTP_NOT_FOUND, "No command %s for device %s", cmdname, dev->name);
    }
  }
  else if (dev->adminState == LOCKED)
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_LOCKED, "Device %s is locked", dev->name);
  }
  else if (dev->operatingState == DOWN)
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_LOCKED, "Device %s is down", dev->name);
  }
  else if (cmd->nreqs > svc->config.device.maxcmdops)
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_INTERNAL_SERVER_ERROR, "MaxCmdOps (%d) exceeded (%d) for command %s", svc->config.device.maxcmdops, cmd->nreqs, cmdname);
  }

  if (reply->code == MHD_HTTP_OK)
  {
    edgex_event_cooked *event = NULL;
    if (req->method == DevSDK_Get)
    {
      event = edgex_device_runget2 (svc, dev, cmd, req->params, reply);
      edgex_device_release (dev);
      if (event)
      {
        edgex_baseresponse br;
        bool pushv = strcmp (devsdk_nvpairs_value_dfl (req->params, DS_PUSH, "no"), "yes") == 0;
        bool retv = strcmp (devsdk_nvpairs_value_dfl (req->params, DS_RETURN, "yes"), "no") != 0;

        if (pushv)
        {
          if (retv)
          {
            edgex_event_cooked_add_ref (event);
            edgex_data_client_add_event_now (svc->dataclient, event);
            edgex_event_cooked_write (event, reply);
          }
          else
          {
            edgex_data_client_add_event (svc->dataclient, event);
            edgex_baseresponse_populate (&br, "v2", MHD_HTTP_OK, "Event generated successfully");
            edgex_baseresponse_write (&br, reply);
          }
        }
        else
        {
          if (retv)
          {
            edgex_event_cooked_write (event, reply);
          }
          else
          {
            edgex_event_cooked_free (event);
            edgex_baseresponse_populate (&br, "v2", MHD_HTTP_OK, "Reading performed successfully");
            edgex_baseresponse_write (&br, reply);
          }
        }
      }
    }
    else
    {
      if (req->data.size)
      {
        edgex_reqdata_t *data = edgex_reqdata_parse (svc->logger, req);
        if (data)
        {
          edgex_device_runput2 (svc, dev, cmd, req->params, data, reply);
          edgex_reqdata_free (data);
        }
        else
        {
          edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "Unable to parse payload for device PUT command");
        }
      }
      else
      {
        edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "PUT command recieved with no data");
      }
      edgex_device_release (dev);
    }
  }
  else
  {
    edgex_device_release (dev);
  }
}

void edgex_device_handler_device_namev2 (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  edgex_device *dev;
  devsdk_service_t *svc = (devsdk_service_t *) ctx;
  const char *name = devsdk_nvpairs_value (req->params, "name");

  iot_log_debug (svc->logger, "Incoming %s command for device name %s", methStr (req->method), name);
  if (svc->adminstate == LOCKED)
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_LOCKED, "device endpoint: service is locked");
    return;
  }

  dev = edgex_devmap_device_byname (svc->devices, name);
  if (dev)
  {
    edgex_device_v2impl (svc, dev, req, reply);
  }
  else
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_NOT_FOUND, "No device named %s", name);
  }
}
