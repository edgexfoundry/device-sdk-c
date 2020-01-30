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
#include "edgex-rest.h"
#include "cmdinfo.h"
#include "iot/base64.h"
#include "transform.h"

#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <microhttpd.h>

/* NOTES
 *
 * The entry point for the device command is edgex_device_handler_device. This
 * parses the device spec and command name out of the url path and calls either
 * oneCommand or allCommand.
 * Each of these two methods finds the relevant device(s), calls runOne to
 * perform the command(s), uploads any readings and constructs the appropriate
 * JSON response.
 * runOne locates profile resources and calls either edgex_device_runget or
 * edgex_device_runput.
 * edgex_device_runget and edgex_device_runput construct the required
 * parameters, perform the conversions between strings and values, and call
 * the device implementation.
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

static bool isNumericType (iot_data_type_t type)
{
  return (type <= IOT_DATA_FLOAT64);
}

char *edgex_value_tostring (const iot_data_t *value, bool binfloat)
{
#define BUFSIZE 32

  char *res = NULL;

  if (isNumericType (iot_data_type (value)))
  {
    res = malloc (BUFSIZE);
  }

  switch (iot_data_type (value))
  {
    case IOT_DATA_INT8:
      sprintf (res, "%" PRIi8, iot_data_i8 (value));
      break;
    case IOT_DATA_UINT8:
      sprintf (res, "%" PRIu8, iot_data_ui8 (value));
      break;
    case IOT_DATA_INT16:
      sprintf (res, "%" PRIi16, iot_data_i16 (value));
      break;
    case IOT_DATA_UINT16:
      sprintf (res, "%" PRIu16, iot_data_ui16 (value));
      break;
    case IOT_DATA_INT32:
      sprintf (res, "%" PRIi32, iot_data_i32 (value));
      break;
    case IOT_DATA_UINT32:
      sprintf (res, "%" PRIu32, iot_data_ui32 (value));
      break;
    case IOT_DATA_INT64:
      sprintf (res, "%" PRIi64, iot_data_i64 (value));
      break;
    case IOT_DATA_UINT64:
      sprintf (res, "%" PRIu64, iot_data_ui64 (value));
      break;
    case IOT_DATA_FLOAT32:
      if (binfloat)
      {
        float f = iot_data_f32 (value);
        iot_b64_encode (&f, sizeof (float), res, BUFSIZE);
      }
      else
      {
        sprintf (res, "%.8e", iot_data_f32 (value));
      }
      break;
    case IOT_DATA_FLOAT64:
      if (binfloat)
      {
        double d = iot_data_f64 (value);
        iot_b64_encode (&d, sizeof (double), res, BUFSIZE);
      }
      else
      {
        sprintf (res, "%.16e", iot_data_f64 (value));
      }
      break;
    case IOT_DATA_BOOL:
      res = strdup (iot_data_bool (value) ? "true" : "false");
      break;
    case IOT_DATA_STRING:
      res = strdup (iot_data_string (value));
      break;
    case IOT_DATA_BLOB:
    {
      uint32_t sz, rsz;
      const uint8_t *data = iot_data_blob (value, &rsz);
      sz = iot_b64_encodesize (rsz);
      res = malloc (sz);
      iot_b64_encode (data, rsz, res, sz);
      break;
    }
    case IOT_DATA_MAP:
      res = strdup ("Error: MAP datatype unsupported");
      break;
    case IOT_DATA_ARRAY:
      res = strdup ("Error: ARRAY datatype unsupported");
      break;
    default:
      res = NULL;
      break;
  }
  return res;
}

static iot_data_t *populateValue (iot_data_type_t rtype, const char *val)
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
      if (strlen (val) == 8 && val[6] == '=' && val[7] == '=')
      {
        size_t sz = sizeof (float);
        return iot_b64_decode (val, &f, &sz) ? iot_data_alloc_f32 (f) : NULL;
      }
      else
      {
        return (sscanf (val, "%e", &f) == 1) ? iot_data_alloc_f32 (f) : NULL;
      }
    }
    case IOT_DATA_FLOAT64:
    {
      double d;
      if (strlen (val) == 12 && val[11] == '=')
      {
        size_t sz = sizeof (double);
        return iot_b64_decode (val, &d, &sz) ? iot_data_alloc_f64 (d) : NULL;
      }
      else
      {
        return (sscanf (val, "%le", &d) == 1) ? iot_data_alloc_f64 (d) : NULL;
      }
    }
    case IOT_DATA_STRING:
      return iot_data_alloc_string (val, IOT_DATA_COPY);
    case IOT_DATA_BOOL:
      return iot_data_alloc_bool (strcasecmp (val, "true") == 0);
    case IOT_DATA_BLOB:
      return iot_data_alloc_blob_from_base64 (val);
    case IOT_DATA_MAP:
    case IOT_DATA_ARRAY:
      return NULL;
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
  (edgex_deviceprofile *prof, edgex_devicecommand *cmd, bool forGet)
{
  edgex_cmdinfo *result = malloc (sizeof (edgex_cmdinfo));
  result->name = cmd->name;
  result->isget = forGet;
  unsigned n = 0;
  edgex_resourceoperation *ro;
  for (ro = forGet ? cmd->get : cmd->set; ro; ro = ro->next)
  {
    n++;
  }
  result->nreqs = n;
  result->reqs = calloc (n, sizeof (devsdk_commandrequest));
  result->pvals = calloc (n, sizeof (edgex_propertyvalue *));
  result->maps = calloc (n, sizeof (edgex_nvpairs *));
  result->dfls = calloc (n, sizeof (char *));
  for (n = 0, ro = forGet ? cmd->get : cmd->set; ro; n++, ro = ro->next)
  {
    edgex_deviceresource *devres =
      findDevResource (prof->device_resources, ro->deviceResource);
    result->reqs[n].resname = devres->name;
    result->reqs[n].attributes = (devsdk_nvpairs *)devres->attributes;
    result->reqs[n].type = devres->properties->value->type;
    result->pvals[n] = devres->properties->value;
    result->maps[n] = (devsdk_nvpairs *)ro->mappings;
    if (ro->parameter && *ro->parameter)
    {
      result->dfls[n] = ro->parameter;
    }
    else if (devres->properties->value->defaultvalue && *devres->properties->value->defaultvalue)
    {
      result->dfls[n] = devres->properties->value->defaultvalue;
    }
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
  result->reqs = malloc (sizeof (devsdk_commandrequest));
  result->pvals = malloc (sizeof (edgex_propertyvalue *));
  result->maps = malloc (sizeof (edgex_nvpairs *));
  result->dfls = malloc (sizeof (char *));
  result->reqs[0].resname = devres->name;
  result->reqs[0].attributes = (devsdk_nvpairs *)devres->attributes;
  result->reqs[0].type = devres->properties->value->type;
  result->pvals[0] = devres->properties->value;
  result->maps[0] = NULL;
  if (devres->properties->value->defaultvalue && *devres->properties->value->defaultvalue)
  {
    result->dfls[0] = devres->properties->value->defaultvalue;
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
    if (cmd->get)
    {
      *head = infoForRes (prof, cmd, true);
      head = &((*head)->next);
    }
    if (cmd->set)
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

static int edgex_device_runput
(
  devsdk_service_t *svc,
  edgex_device *dev,
  const edgex_cmdinfo *commandinfo,
  const char *data,
  char **exc
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

  iot_data_t **results = calloc (commandinfo->nreqs, sizeof (iot_data_t *));
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
    if (value == NULL && commandinfo->dfls[i] == NULL)
    {
      retcode = MHD_HTTP_BAD_REQUEST;
      iot_log_error (svc->logger, "No value supplied for %s", resname);
      break;
    }
    results[i] = populateValue (commandinfo->pvals[i]->type, value ? value : commandinfo->dfls[i]);
    if (!results[i])
    {
      retcode = MHD_HTTP_BAD_REQUEST;
      iot_log_error
        (svc->logger, "Unable to parse \"%s\" for %s", value ? value : commandinfo->dfls[i], resname);
      break;
    }
    if (svc->config.device.datatransform && value)
    {
      edgex_transform_incoming (&results[i], commandinfo->pvals[i], commandinfo->maps[i]);
      if (!results[i])
      {
        retcode = MHD_HTTP_BAD_REQUEST;
        iot_log_error (svc->logger, "Value \"%s\" for %s overflows after transformations", value, resname);
        break;
      }
    }
  }

  if (retcode == MHD_HTTP_OK)
  {
    iot_data_t *e = NULL;
    if
    (
      !svc->userfns.puthandler
        (svc->userdata, dev->name, (devsdk_protocols *)dev->protocols, commandinfo->nreqs, commandinfo->reqs, (const iot_data_t **)results, &e)
    )
    {
      retcode = MHD_HTTP_INTERNAL_SERVER_ERROR;
      if (e)
      {
        *exc = iot_data_to_json (e, false);
      }
      iot_log_error (svc->logger, "Driver for %s failed on PUT%s%s", dev->name, e ? ": " : "", e ? *exc : "");
    }
    iot_data_free (e);
  }

  for (int i = 0; i < commandinfo->nreqs; i++)
  {
    iot_data_free (results[i]);
  }
  free (results);
  json_value_free (jval);

  return retcode;
}

static int edgex_device_runget
(
  devsdk_service_t *svc,
  edgex_device *dev,
  const edgex_cmdinfo *cmdinfo,
  const devsdk_nvpairs *qparams,
  edgex_event_cooked **reply,
  char **exc
)
{
  devsdk_commandresult *results;

  int retcode = MHD_HTTP_INTERNAL_SERVER_ERROR;
  iot_data_t *e = NULL;

  for (int i = 0; i < cmdinfo->nreqs; i++)
  {
    if (!cmdinfo->pvals[i]->readable)
    {
      iot_log_error (svc->logger, "Attempt to read unreadable value %s", cmdinfo->reqs[i].resname);
      return MHD_HTTP_METHOD_NOT_ALLOWED;
    }
  }

  results = calloc (cmdinfo->nreqs, sizeof (devsdk_commandresult));

  if
  (
    svc->userfns.gethandler (svc->userdata, dev->name, (devsdk_protocols *)dev->protocols, cmdinfo->nreqs, cmdinfo->reqs, results, qparams, &e)
  )
  {
    devsdk_error err = EDGEX_OK;
    *reply = edgex_data_process_event (dev->name, cmdinfo, results, svc->config.device.datatransform);

    if (*reply)
    {
      retcode = MHD_HTTP_OK;
      edgex_data_client_add_event (svc->logger, &svc->config.endpoints, *reply, &err);
    }
    else
    {
      iot_log_error (svc->logger, "Assertion failed for device %s. Disabling.", dev->name);
      edgex_metadata_client_set_device_opstate (svc->logger, &svc->config.endpoints, dev->id, DISABLED, &err);
    }
  }
  else
  {
    if (e)
    {
      *exc = iot_data_to_json (e, false);
    }
    iot_log_error (svc->logger, "Driver for %s failed on GET%s%s", dev->name, e ? ": " : "", e ? *exc : "");
  }

  iot_data_free (e);
  devsdk_commandresult_free (results, cmdinfo->nreqs);

  return retcode;
}

static int runOne
(
  devsdk_service_t *svc,
  edgex_device *dev,
  const edgex_cmdinfo *command,
  const devsdk_nvpairs *qparams,
  const char *upload_data,
  size_t upload_data_size,
  edgex_event_cooked **reply,
  char **exc
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
    return edgex_device_runget (svc, dev, command, qparams, reply, exc);
  }
  else
  {
    if (upload_data_size == 0)
    {
      iot_log_error (svc->logger, "PUT command recieved with no data");
      return MHD_HTTP_BAD_REQUEST;
    }
    return edgex_device_runput (svc, dev, command, upload_data, exc);
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
  devsdk_service_t *svc,
  const char *cmd,
  edgex_http_method method,
  const devsdk_nvpairs *qparams,
  const char *upload_data,
  size_t upload_data_size,
  void **reply,
  size_t *reply_size,
  const char **reply_type
)
{
  int ret = MHD_HTTP_NOT_FOUND;
  int retOne;
  edgex_cmdqueue_t *cmdq = NULL;
  edgex_cmdqueue_t *iter;
  uint32_t nret = 0;
  char *buff;
  edgex_event_encoding enc;
  size_t bsize;

  iot_log_debug
    (svc->logger, "Incoming %s command %s for all", methStr (method), cmd);

  enc = JSON;
  bsize = 3; // start-array byte + end-array byte + NUL character
  buff = malloc (bsize);
  strcpy (buff, "[");

  cmdq = edgex_devmap_device_forcmd (svc->devices, cmd, method == GET);

  for (iter = cmdq; iter; iter = iter->next)
  {
    edgex_event_cooked *ereply = NULL;
    char *exc = NULL;
    retOne = runOne (svc, iter->dev, iter->cmd, qparams, upload_data, upload_data_size, &ereply, &exc);
    edgex_device_release (iter->dev);
    free (exc);
    if (ereply)
    {
      enc = ereply->encoding;
      switch (enc)
      {
        case JSON:
          bsize += (strlen (ereply->value.json) + (nret ? 1 : 0));
          buff = realloc (buff, bsize);
          if (nret)
          {
            strcat (buff, ",");
          }
          strcat (buff, ereply->value.json);
          break;
        case CBOR:
          buff = realloc (buff, bsize + ereply->value.cbor.length);
          if (nret == 0)
          {
            buff[0] = '\374';
          }
          memcpy (buff + bsize - 2, ereply->value.cbor.data, ereply->value.cbor.length);
          bsize += ereply->value.cbor.length;
          break;
      }
      nret++;
    }
    edgex_event_cooked_free (ereply);
    if (ret != MHD_HTTP_OK)
    {
      ret = retOne;
    }
  }

  if (ret == MHD_HTTP_OK)
  {
    switch (enc)
    {
      case JSON:
        strcat (buff, "]");
        *reply = buff;
        *reply_size = strlen (buff);
        *reply_type = "application/json";
        break;
      case CBOR:
        buff[bsize - 2] = '\377';
        buff[bsize - 1] = '\0';
        *reply = buff;
        *reply_size = bsize - 1;
        *reply_type = "application/cbor";
        break;
    }
  }
  else
  {
    free (buff);
  }

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
  devsdk_service_t *svc,
  const char *id,
  bool byName,
  const char *cmd,
  edgex_http_method method,
  const devsdk_nvpairs *qparams,
  const char *upload_data,
  size_t upload_data_size,
  void **reply,
  size_t *reply_size,
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
    edgex_event_cooked *ereply = NULL;
    char *exc = NULL;
    result = runOne (svc, dev, command, qparams, upload_data, upload_data_size, &ereply, &exc);
    edgex_device_release (dev);
    if (ereply)
    {
      switch (ereply->encoding)
      {
        case JSON:
          *reply = ereply->value.json;
          *reply_size = strlen (ereply->value.json);
          *reply_type = "application/json";
          break;
        case CBOR:
          *reply = ereply->value.cbor.data;
          *reply_size = ereply->value.cbor.length;
          *reply_type = "application/cbor";
          break;
      }
      free (ereply);
    }
    else if (exc)
    {
      *reply = exc;
      *reply_size = strlen (exc);
      *reply_type = "text/plain";
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
  const devsdk_nvpairs *qparams,
  edgex_http_method method,
  const char *upload_data,
  size_t upload_data_size,
  void **reply,
  size_t *reply_size,
  const char **reply_type
)
{
  int result = MHD_HTTP_NOT_FOUND;
  char *cmd;
  devsdk_service_t *svc = (devsdk_service_t *) ctx;

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
        result = allCommand (svc, cmd, method, qparams, upload_data, upload_data_size, reply, reply_size, reply_type);
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
           (svc, url, byName, cmd + 1, method, qparams, upload_data, upload_data_size, reply, reply_size, reply_type);
         *cmd = '/';
      }
    }
  }
  return result;
}
