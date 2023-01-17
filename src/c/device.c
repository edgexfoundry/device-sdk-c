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

static iot_data_t *populateValue (iot_typecode_t rtype, const char *val)
{
  if (rtype.type == IOT_DATA_ARRAY)
  {
    iot_data_t *vec = iot_data_from_json (val);
    if (iot_data_type (vec) != IOT_DATA_VECTOR)
    {
      iot_data_free (vec);
      return NULL;
    }
    uint32_t length = iot_data_vector_size (vec);
    uint32_t esize = iot_data_type_size (rtype.element_type);
    uint8_t *arr = malloc (length * esize);
    iot_data_vector_iter_t iter;
    iot_data_vector_iter (vec, &iter);
    for (int i = 0; i < length; i++)
    {
      iot_data_vector_iter_next (&iter);
      if (!iot_data_cast (iot_data_vector_iter_value (&iter), rtype.element_type, arr + i * esize))
      {
        free (arr);
        iot_data_free (vec);
        return NULL;
      }
    }
    iot_data_free (vec);
    return iot_data_alloc_array (arr, length, rtype.element_type, IOT_DATA_TAKE);
  }
  else if (rtype.type == IOT_DATA_BINARY)
  {
    iot_data_t *res = iot_data_alloc_array_from_base64 (val);
    if (res)
    {
      iot_data_array_to_binary (res);
    }
    return res;
  }
  else if (rtype.type == IOT_DATA_MAP)
  {
    return iot_data_from_json (val);
  }
  else
  {
    return iot_data_alloc_from_string (rtype.type, val);
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

static edgex_cmdinfo *infoForRes (devsdk_service_t *svc, edgex_deviceprofile *prof, edgex_devicecommand *cmd, bool forGet)
{
  iot_data_t *exception = NULL;
  edgex_cmdinfo *result;
  unsigned n = 0;
  edgex_resourceoperation *ro;
  edgex_deviceresource *devres;
  for (ro = cmd->resourceOperations; ro; ro = ro->next)
  {
    n++;
    devres = findDevResource (prof->device_resources, ro->deviceResource);
    if (devres->parsed_attrs == NULL)
    {
      devres->parsed_attrs = svc->userfns.create_res (svc->userdata, devres->attributes, &exception);
      if (devres->parsed_attrs == NULL)
      {
        if (exception)
        {
          iot_log_error (svc->logger, iot_data_to_json (exception));
          iot_data_free (exception);
        }
        iot_log_error (svc->logger, "Unable to parse attributes for device resource %s: device command %s will not be available", devres->name, cmd->name);
        return NULL;
      }
    }
  }
  result = malloc (sizeof (edgex_cmdinfo));
  result->name = cmd->name;
  result->profile = prof;
  result->isget = forGet;
  result->nreqs = n;
  result->reqs = calloc (n, sizeof (devsdk_commandrequest));
  result->pvals = calloc (n, sizeof (edgex_propertyvalue *));
  result->maps = calloc (n, sizeof (devsdk_nvpairs *));
  result->dfls = calloc (n, sizeof (char *));
  for (n = 0, ro = cmd->resourceOperations; ro; n++, ro = ro->next)
  {
    result->reqs[n].resource = malloc (sizeof (devsdk_resource_t));
    edgex_deviceresource *devres =
      findDevResource (prof->device_resources, ro->deviceResource);
    result->reqs[n].resource->name = devres->name;
    result->reqs[n].resource->attrs = devres->parsed_attrs;
    result->reqs[n].resource->type = devres->properties->type;
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

static edgex_cmdinfo *infoForDevRes (devsdk_service_t *svc, edgex_deviceprofile *prof, edgex_deviceresource *devres, bool forGet)
{
  edgex_cmdinfo *result;
  iot_data_t *exception = NULL;
  if (devres->parsed_attrs == NULL)
  {
    devres->parsed_attrs = svc->userfns.create_res (svc->userdata, devres->attributes, &exception);
    if (devres->parsed_attrs == NULL)
    {
      if (exception)
      {
        iot_log_error (svc->logger, iot_data_to_json (exception));
        iot_data_free (exception);
      }
      iot_log_error (svc->logger, "Unable to parse attributes for device resource %s: it will not be available", devres->name);
      return NULL;
    }
  }
  result = malloc (sizeof (edgex_cmdinfo));
  result->name = devres->name;
  result->profile = prof;
  result->isget = forGet;
  result->nreqs = 1;
  result->reqs = malloc (sizeof (devsdk_commandrequest));
  result->pvals = malloc (sizeof (edgex_propertyvalue *));
  result->maps = malloc (sizeof (devsdk_nvpairs *));
  result->dfls = malloc (sizeof (char *));
  result->reqs[0].resource = malloc (sizeof (devsdk_resource_t));
  result->reqs[0].resource->name = devres->name;
  result->reqs[0].resource->attrs = devres->parsed_attrs;
  result->reqs[0].resource->type = devres->properties->type;
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

static void insertCmdInfo (edgex_cmdinfo ***head, edgex_cmdinfo *entry)
{
  if (entry)
  {
    **head = entry;
    *head = &((**head)->next);
  }
}

static void populateCmdInfo (devsdk_service_t *svc, edgex_deviceprofile *prof)
{
  edgex_cmdinfo **head = &prof->cmdinfo;
  edgex_devicecommand *cmd = prof->device_commands;
  while (cmd)
  {
    if (cmd->readable)
    {
      insertCmdInfo (&head, infoForRes (svc, prof, cmd, true));
    }
    if (cmd->writable)
    {
      insertCmdInfo (&head, infoForRes (svc, prof, cmd, false));
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
        insertCmdInfo (&head, infoForDevRes (svc, prof, devres, true));
      }
      if (devres->properties->writable)
      {
        insertCmdInfo (&head, infoForDevRes (svc, prof, devres, false));
      }
    }
    devres = devres->next;
  }
}

const edgex_cmdinfo *edgex_deviceprofile_findcommand
  (devsdk_service_t *svc, const char *name, edgex_deviceprofile *prof, bool forGet)
{
  if (prof->cmdinfo == NULL)
  {
    populateCmdInfo (svc, prof);
  }

  edgex_cmdinfo *result = prof->cmdinfo;
  while (result && (strcmp (result->name, name) || forGet != result->isget))
  {
    result = result->next;
  }
  return result;
}

static void edgex_device_runput2
  (devsdk_service_t *svc, edgex_device *dev, const edgex_cmdinfo *cmdinfo, const iot_data_t *params, const edgex_reqdata_t *rdata, devsdk_http_reply *reply)
{
  reply->code = MHD_HTTP_OK;
  iot_data_t **results = calloc (cmdinfo->nreqs, sizeof (iot_data_t *));
  for (int i = 0; i < cmdinfo->nreqs; i++)
  {
    const char *resname = cmdinfo->reqs[i].resource->name;
    if (!cmdinfo->pvals[i]->writable)
    {
      edgex_error_response (svc->logger, reply, MHD_HTTP_METHOD_NOT_ALLOWED, "Attempt to write unwritable value %s", resname);
      break;
    }

    if (cmdinfo->pvals[i]->type.type != IOT_DATA_BINARY)
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
    if (dev->devimpl->address == NULL)
    {
      dev->devimpl->address = svc->userfns.create_addr (svc->userdata, dev->protocols, &e);
    }
    if (dev->devimpl->address)
    {
      if (svc->userfns.puthandler (svc->userdata, dev->devimpl, cmdinfo->nreqs, cmdinfo->reqs, (const iot_data_t **)results, params, &e))
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
    }
    else
    {
      edgex_error_response
        (svc->logger, reply, MHD_HTTP_INTERNAL_SERVER_ERROR, "Address parsing failed for device %s: %s", dev->name, e ? iot_data_to_json (e) : "(unknown)");
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
  (devsdk_service_t *svc, edgex_device *dev, const edgex_cmdinfo *cmdinfo, const iot_data_t *params, devsdk_http_reply *reply)
{
  for (int i = 0; i < cmdinfo->nreqs; i++)
  {
    if (!cmdinfo->pvals[i]->readable)
    {
      edgex_error_response (svc->logger, reply, MHD_HTTP_METHOD_NOT_ALLOWED, "Attempt to read unreadable value %s", cmdinfo->reqs[i].resource->name);
      return NULL;
    }
  }

  edgex_event_cooked *result = NULL;
  iot_data_t *e = NULL;
  devsdk_commandresult *results = calloc (cmdinfo->nreqs, sizeof (devsdk_commandresult));

  if (dev->devimpl->address == NULL)
  {
    dev->devimpl->address = svc->userfns.create_addr (svc->userdata, dev->protocols, &e);
  }
  if (dev->devimpl->address)
  {
    if (svc->userfns.gethandler (svc->userdata, dev->devimpl, cmdinfo->nreqs, cmdinfo->reqs, results, params, &e))
    {
      devsdk_error err = EDGEX_OK;
      result = edgex_data_process_event (dev->name, cmdinfo, results, svc->config.device.datatransform);

      if (result)
      {
        if (svc->config.device.updatelastconnected)
        {
          edgex_metadata_client_update_lastconnected (svc->logger, &svc->config.endpoints, dev->name, &err);
        }
        if (svc->config.device.maxeventsize && edgex_event_cooked_size (result) > svc->config.device.maxeventsize * 1024)
        {
          edgex_error_response (svc->logger, reply, MHD_HTTP_INTERNAL_SERVER_ERROR, "Event size (%d KiB) exceeds configured MaxEventSize", edgex_event_cooked_size (result) / 1024);
          edgex_event_cooked_free (result);
          result = NULL;
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
    atomic_fetch_add (&svc->metrics.rcexe, 1);
  }
  else
  {
    edgex_error_response
      (svc->logger, reply, MHD_HTTP_INTERNAL_SERVER_ERROR, "Address parsing failed for device %s: %s", dev->name, e ? iot_data_to_json (e) : "(unknown)");
  }
  iot_data_free (e);
  devsdk_commandresult_free (results, cmdinfo->nreqs);

  return result;
}

static void edgex_device_v2impl (devsdk_service_t *svc, edgex_device *dev, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  const char *cmdname = devsdk_nvpairs_value (req->params, "cmd");
  const edgex_cmdinfo *cmd = edgex_deviceprofile_findcommand (svc, cmdname, dev->profile, req->method == DevSDK_Get);
  reply->code = MHD_HTTP_OK;

  if (!cmd)
  {
    if (edgex_deviceprofile_findcommand (svc, cmdname, dev->profile, req->method != DevSDK_Get))
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
  else if (svc->config.device.maxcmdops && (cmd->nreqs > svc->config.device.maxcmdops))
  {
    edgex_error_response (svc->logger, reply, MHD_HTTP_INTERNAL_SERVER_ERROR, "MaxCmdOps (%d) exceeded (%d) for command %s", svc->config.device.maxcmdops, cmd->nreqs, cmdname);
  }

  if (reply->code == MHD_HTTP_OK)
  {
    edgex_event_cooked *event = NULL;
    if (req->method == DevSDK_Get)
    {
      event = edgex_device_runget2 (svc, dev, cmd, req->qparams, reply);
      edgex_device_release (svc, dev);
      if (event)
      {
        edgex_baseresponse br;
        bool pushv = false;
        bool retv = true;
        if (req->qparams && iot_data_string_map_get_string(req->qparams, DS_PUSH) &&
            (strcmp(iot_data_string_map_get_string(req->qparams, DS_PUSH), "true") == 0))
        {
          pushv = true;
        }
        if (req->qparams && iot_data_string_map_get_string(req->qparams, DS_RETURN) &&
            (strcmp(iot_data_string_map_get_string(req->qparams, DS_RETURN), "false") == 0))
        {
          retv = false;
        }

        if (pushv)
        {
          if (retv)
          {
            edgex_event_cooked_add_ref (event);
            edgex_data_client_add_event_now (svc->dataclient, event, &svc->metrics);
            edgex_event_cooked_write (event, reply);
          }
          else
          {
            edgex_data_client_add_event (svc->dataclient, event, &svc->metrics);
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
          edgex_device_runput2 (svc, dev, cmd, req->qparams, data, reply);
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
      edgex_device_release (svc, dev);
    }
  }
  else
  {
    edgex_device_release (svc, dev);
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
