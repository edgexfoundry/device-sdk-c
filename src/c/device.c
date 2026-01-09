/*
 * Copyright (c) 2018-2025
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
#include "iot/config.h"
#include "transform.h"
#include "reqdata.h"
#include "request_auth.h"
#include "opstate.h"

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
          char *exstr = iot_data_to_json (exception);
          iot_log_error (svc->logger, "%s", exstr ? exstr : "Error: exstr reported NULL");
          free (exstr);
          iot_data_free (exception);
          iot_log_error (svc->logger, "Unable to parse attributes for device resource %s: device command %s will not be available", devres->name, cmd->name);
          return NULL;
        }
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
  result->maps = calloc (n, sizeof (iot_data_t *));
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
    result->maps[n] = iot_data_add_ref (ro->mappings);
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
    if (exception)
    {
      char *exstr = iot_data_to_json (exception);
      iot_log_error (svc->logger, "%s", exstr ? exstr : "Error: exstr reported NULL");
      free (exstr);
      iot_data_free (exception);
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
        if (!edgex_transform_validate (results[i], cmdinfo->pvals[i]))
        {
          edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "Value \"%s\" for %s out of range specified in profile", value, resname);
          break;
        }
        edgex_transform_incoming (&results[i], cmdinfo->pvals[i], cmdinfo->maps[i]);
        if (!results[i])
        {
          edgex_error_response (svc->logger, reply, MHD_HTTP_BAD_REQUEST, "Value \"%s\" for %s overflows after transformations", value, resname);
          break;
        }
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
        edgex_baseresponse_populate (&br, EDGEX_API_VERSION, MHD_HTTP_OK, "Data written successfully");
        edgex_baseresponse_write (&br, reply);
        if (svc->config.device.updatelastconnected)
        {
          devsdk_error err;
          edgex_metadata_client_update_lastconnected (svc->logger, &svc->config.endpoints, svc->secretstore, dev->name, &err);
        }
      }
      else
      {
        char *exstr = e ? iot_data_to_json (e) : NULL;
        edgex_error_response
                (svc->logger, reply, MHD_HTTP_INTERNAL_SERVER_ERROR, "Driver for %s failed on PUT: %s", dev->name, e ? exstr : "(unknown)");
        free (exstr);
      }
    }
    else
    {
      char *exstr = e ? iot_data_to_json (e) : NULL;
      edgex_error_response
              (svc->logger, reply, MHD_HTTP_INTERNAL_SERVER_ERROR, "Address parsing failed for device %s: %s", dev->name, e ? exstr : "(unknown)");
      free (exstr);
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
      result = edgex_data_process_event (dev->name, cmdinfo, results, svc->config.device.datatransform, svc->reduced_events);

      if (result)
      {
        if (svc->config.device.updatelastconnected)
        {
          edgex_metadata_client_update_lastconnected (svc->logger, &svc->config.endpoints, svc->secretstore, dev->name, &err);
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
        edgex_metadata_client_set_device_opstate (svc->logger, &svc->config.endpoints, svc->secretstore, dev->name, DOWN, &err);
      }
    }
    else
    {
      char *exstr = e ? iot_data_to_json (e) : NULL;
      edgex_error_response
              (svc->logger, reply, MHD_HTTP_INTERNAL_SERVER_ERROR, "Driver for %s failed on GET: ", dev->name, e ? exstr : "(unknown)");
      free(exstr);
    }
    atomic_fetch_add (&svc->metrics.rcexe, 1);
  }
  else
  {
    char *exstr = e ? iot_data_to_json (e) : NULL;
    edgex_error_response
            (svc->logger, reply, MHD_HTTP_INTERNAL_SERVER_ERROR, "Address parsing failed for device %s: %s", dev->name, e ? exstr : "(unknown)");
    free(exstr);
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
            edgex_data_client_add_event (svc->msgbus, event, &svc->metrics);
            edgex_event_cooked_write (event, reply);
          }
          else
          {
            edgex_data_client_add_event (svc->msgbus, event, &svc->metrics);
            edgex_baseresponse_populate (&br, EDGEX_API_VERSION, MHD_HTTP_OK, "Event generated successfully");
            edgex_baseresponse_write (&br, reply);
          }
          edgex_event_cooked_free (event);
        }
        else
        {
          if (retv)
          {
            edgex_event_cooked_write (event, reply);
          }
          else
          {
            edgex_baseresponse_populate (&br, EDGEX_API_VERSION, MHD_HTTP_OK, "Reading performed successfully");
            edgex_baseresponse_write (&br, reply);
          }
          edgex_event_cooked_free (event);
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

static int32_t edgex_device_runput3
  (devsdk_service_t *svc, edgex_device *dev, const edgex_cmdinfo *cmdinfo, const iot_data_t *request, const iot_data_t *params, iot_data_t **reply)
{
  int32_t result = 0;
  iot_data_t *e = NULL;
  iot_data_t **results = calloc (cmdinfo->nreqs, sizeof (iot_data_t *));
  for (int i = 0; i < cmdinfo->nreqs; i++)
  {
    const char *resname = cmdinfo->reqs[i].resource->name;
    if (!cmdinfo->pvals[i]->writable)
    {
      *reply = edgex_v3_error_response (svc->logger, "Attempt to write unwritable value %s", resname);
      result = MHD_HTTP_METHOD_NOT_ALLOWED;
      break;
    }

    const char *value = iot_config_string_default (request, resname, cmdinfo->dfls[i], false);
    if (value == NULL)
    {
      *reply = edgex_v3_error_response (svc->logger, "No value supplied for %s", resname);
      result = MHD_HTTP_BAD_REQUEST;
      break;
    }

    results[i] = populateValue (cmdinfo->pvals[i]->type, value);
    if (!results[i])
    {
      *reply = edgex_v3_error_response (svc->logger, "Unable to parse \"%s\" for %s", value, resname);
      result = MHD_HTTP_BAD_REQUEST;
      break;
    }

    if (svc->config.device.datatransform)
    {
      if (!edgex_transform_validate (results[i], cmdinfo->pvals[i]))
      {
        *reply = edgex_v3_error_response (svc->logger, "Value \"%s\" for %s out of range specified in profile", value, resname);
        result = MHD_HTTP_BAD_REQUEST;
        break;
      }
      edgex_transform_incoming (&results[i], cmdinfo->pvals[i], cmdinfo->maps[i]);
      if (!results[i])
      {
        *reply = edgex_v3_error_response (svc->logger, "Value \"%s\" for %s overflows after transformations", value, resname);
        result = MHD_HTTP_BAD_REQUEST;
        break;
      }
    }
  }

  if (result)
  {
    return result;
  }

  if (dev->devimpl->address == NULL)
  {
    dev->devimpl->address = svc->userfns.create_addr (svc->userdata, dev->protocols, &e);
  }
  if (dev->devimpl->address)
  {
    if (svc->userfns.puthandler (svc->userdata, dev->devimpl, cmdinfo->nreqs, cmdinfo->reqs, (const iot_data_t **)results, params, &e))
    {
      *reply = edgex_v3_base_response ("Data written successfully");
      if (svc->config.device.updatelastconnected)
      {
        devsdk_error err;
        edgex_metadata_client_update_lastconnected (svc->logger, &svc->config.endpoints, svc->secretstore, dev->name, &err);
      }
      devsdk_device_request_succeeded (svc, dev);
    }
    else
    {
      char *exstr = e ? iot_data_to_json (e) : NULL;
      *reply = edgex_v3_error_response (svc->logger, "Driver for %s failed on PUT: %s", dev->name, exstr ? exstr : "(unknown)");
      free (exstr);
      result = MHD_HTTP_INTERNAL_SERVER_ERROR;
      devsdk_device_request_failed (svc, dev);
    }
  }
  else
  {
    char *exstr = e ? iot_data_to_json (e) : NULL;
    *reply = edgex_v3_error_response (svc->logger, "Address parsing failed for device %s: %s", dev->name, exstr ? exstr : "(unknown)");
    free (exstr);
    result = MHD_HTTP_INTERNAL_SERVER_ERROR;
  }
  iot_data_free (e);

  for (int i = 0; i < cmdinfo->nreqs; i++)
  {
    iot_data_free (results[i]);
  }
  free (results);
  return result;
}

static edgex_event_cooked *edgex_device_runget3 (devsdk_service_t *svc, edgex_device *dev, const edgex_cmdinfo *cmdinfo, const iot_data_t *params, iot_data_t **reply)
{
  for (int i = 0; i < cmdinfo->nreqs; i++)
  {
    if (!cmdinfo->pvals[i]->readable)
    {
      *reply = edgex_v3_error_response (svc->logger, "Attempt to read unreadable value %s", cmdinfo->reqs[i].resource->name);
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
      result = edgex_data_process_event (dev->name, cmdinfo, results, svc->config.device.datatransform, svc->reduced_events);
      if (result)
      {
        if (svc->config.device.updatelastconnected)
        {
          edgex_metadata_client_update_lastconnected (svc->logger, &svc->config.endpoints, svc->secretstore, dev->name, &err);
        }
        if (svc->config.device.maxeventsize && edgex_event_cooked_size (result) > svc->config.device.maxeventsize * 1024)
        {
          *reply = edgex_v3_error_response (svc->logger, "Event size (%d KiB) exceeds configured MaxEventSize", edgex_event_cooked_size (result) / 1024);
          edgex_event_cooked_free (result);
          result = NULL;
        }
        devsdk_device_request_succeeded (svc, dev);
      }
      else
      {
        *reply = edgex_v3_error_response (svc->logger, "Assertion failed for device %s. Marking as down.", dev->name);
        edgex_metadata_client_set_device_opstate (svc->logger, &svc->config.endpoints, svc->secretstore, dev->name, DOWN, &err);
      }
    }
    else
    {
      char *exstr = e ? iot_data_to_json (e) : NULL;
      *reply = edgex_v3_error_response (svc->logger, "Driver for %s failed on GET: %s", dev->name, exstr ? exstr : "(unknown)");
      free (exstr);
      devsdk_device_request_failed (svc, dev);
    }
    atomic_fetch_add (&svc->metrics.rcexe, 1);
  }
  else
  {
    char *exstr = e ? iot_data_to_json (e) : NULL;
    *reply = edgex_v3_error_response (svc->logger, "Address parsing failed for device %s: %s", dev->name, exstr ? exstr : "(unknown)");
    free (exstr);
  }
  iot_data_free (e);
  devsdk_commandresult_free (results, cmdinfo->nreqs);

  return result;
}

static int32_t edgex_device_v3impl (devsdk_service_t *svc, edgex_device *dev, const char *cmdname, bool isGet, const iot_data_t *req, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor)
{
  int32_t result = 0;
  const edgex_cmdinfo *cmd = edgex_deviceprofile_findcommand (svc, cmdname, dev->profile, isGet);
  if (!cmd)
  {
    if (edgex_deviceprofile_findcommand (svc, cmdname, dev->profile, !isGet))
    {
      *reply = edgex_v3_error_response (svc->logger, "Wrong method for command %s (operation is %s-only)", cmdname, isGet ? "write" : "read");
      result = MHD_HTTP_METHOD_NOT_ALLOWED;
    }
    else
    {
      *reply = edgex_v3_error_response (svc->logger, "No command %s for device %s", cmdname, dev->name);
      result = MHD_HTTP_NOT_FOUND;
    }
  }
  else if (dev->adminState == LOCKED)
  {
    *reply = edgex_v3_error_response (svc->logger, "Device %s is locked", dev->name);
    result = MHD_HTTP_LOCKED;
  }
  else if (dev->operatingState == DOWN)
  {
    *reply = edgex_v3_error_response (svc->logger, "Device %s is down", dev->name);
    result = MHD_HTTP_LOCKED;
  }
  else if (svc->config.device.maxcmdops && (cmd->nreqs > svc->config.device.maxcmdops))
  {
    *reply = edgex_v3_error_response (svc->logger, "MaxCmdOps (%d) exceeded (%d) for command %s", svc->config.device.maxcmdops, cmd->nreqs, cmdname);
    result = MHD_HTTP_INTERNAL_SERVER_ERROR;
  }
  if (result)
  {
    edgex_device_release (svc, dev);
    return result;
  }

  if (isGet)
  {
    edgex_event_cooked *event = edgex_device_runget3 (svc, dev, cmd, params, reply);
    edgex_device_release (svc, dev);
    if (event)
    {
      if (event_is_cbor)
      {
        *event_is_cbor = (event->encoding == CBOR);
      }
      bool pushv = params ? iot_data_string_map_get_bool (params, DS_PUSH, false) : false;
      bool retv = params ? iot_data_string_map_get_bool (params, DS_RETURN, true) : true;
      if (pushv)
      {
        edgex_data_client_add_event (svc->msgbus, event, &svc->metrics);
      }
      if (retv)
      {
        *reply = iot_data_add_ref (event->value);
      }
      else
      {
        *reply = edgex_v3_base_response (pushv ? "Event generated successfully" : "Reading performed successfully");
      }
      edgex_event_cooked_free (event);
    }
    else
    {
      result = 1;
    }
  }
  else
  {
    result = edgex_device_runput3 (svc, dev, cmd, req, params, reply);
    edgex_device_release (svc, dev);
  }
  return result;
}

int32_t edgex_device_handler_devicev3 (void *ctx, const iot_data_t *req, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;
  edgex_device *device;

  const char *op = iot_data_string_map_get_string (pathparams, "op");
  const char *cmd = iot_data_string_map_get_string (pathparams, "cmd");
  const char *dev = iot_data_string_map_get_string (pathparams, "device");

  iot_log_debug (svc->logger, "Incoming %s command for device name %s", op, dev);
  if (svc->adminstate == LOCKED)
  {
    *reply = edgex_v3_error_response (svc->logger, "device endpoint: service is locked");
    return MHD_HTTP_LOCKED;
  }

  device = edgex_devmap_device_byname (svc->devices, dev);
  if (device)
  {
    bool isGet = false;
    devsdk_http_request hreq;
    devsdk_http_reply hreply;
    memset (&hreply, 0, sizeof (hreply));
    memset (&hreq, 0, sizeof (hreq));

    if (strcmp (op, "get") == 0)
    {
      isGet = true;
    }
    else if (strcmp (op, "set") != 0)
    {
      *reply = edgex_v3_error_response (svc->logger, "device: only get and set operations allowed");
      return MHD_HTTP_METHOD_NOT_ALLOWED;
    }
    return edgex_device_v3impl (svc, device, cmd, isGet, req, params, reply, event_is_cbor);
  }
  else
  {
    *reply = edgex_v3_error_response (svc->logger, "No device named %s", dev);
    return MHD_HTTP_NOT_FOUND;
  }
}
