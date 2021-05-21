/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <curl/curl.h>
#include <errno.h>

#include "metadata.h"
#include "edgex-rest.h"
#include "rest.h"
#include "errorlist.h"
#include "config.h"
#include "profiles.h"
#include "iot/time.h"

edgex_deviceprofile *edgex_metadata_client_get_deviceprofile
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  const char *name,
  devsdk_error *err
)
{
  edgex_ctx ctx;
  edgex_deviceprofile *result = NULL;
  char *ename;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  ename = curl_easy_escape (NULL, name, 0);

  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v2/deviceprofile/name/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    ename
  );

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  if (err->code == 0)
  {
    result = edgex_getprofileresponse_read (lc, ctx.buff);
    if (!result)
    {
      *err = EDGEX_PROFILE_PARSE_ERROR;
    }
  }
  curl_free (ename);
  free (ctx.buff);
  return result;
}

void edgex_metadata_client_set_device_opstate
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  const char *devicename,
  edgex_device_operatingstate opstate,
  devsdk_error *err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  char *json = edgex_updateDevOpreq_write (devicename, opstate);

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/v2/device", endpoints->metadata.host, endpoints->metadata.port);

  edgex_http_patch (lc, &ctx, url, json, edgex_http_write_cb, err);
  json_free_serialized_string (json);
  free (ctx.buff);
}

void edgex_metadata_client_update_deviceservice
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const char * name,
  const char * baseaddr,
  devsdk_error * err
)
{ 
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  char *json = edgex_updateDSreq_write (name, baseaddr);

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/v2/deviceservice", endpoints->metadata.host, endpoints->metadata.port);

  edgex_http_patch (lc, &ctx, url, json, edgex_http_write_cb, err);
  json_free_serialized_string (json);
  free (ctx.buff);
}

void edgex_metadata_client_update_lastconnected
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const char * devicename,
  devsdk_error * err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  char *json = edgex_updateDevLCreq_write (devicename, iot_time_msecs ());

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/v2/device", endpoints->metadata.host, endpoints->metadata.port);

  edgex_http_patch (lc, &ctx, url, json, edgex_http_write_cb, err);
  json_free_serialized_string (json);
  free (ctx.buff);
}

char *edgex_metadata_client_create_deviceprofile_file
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  const char *filename,
  devsdk_error *err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v2/deviceprofile/uploadfile",
    endpoints->metadata.host,
    endpoints->metadata.port
  );
  edgex_http_postfile (lc, &ctx, url, filename, edgex_http_write_cb, err);
  return ctx.buff;
}

edgex_deviceservice *edgex_metadata_client_get_deviceservice
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  const char *name,
  devsdk_error *err
)
{
  edgex_ctx ctx;
  char *ename;
  edgex_deviceservice *result = NULL;
  char url[URL_BUF_SIZE];
  long rc;

  ename = curl_easy_escape (NULL, name, 0);
  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v2/deviceservice/name/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    ename
  );

  rc = edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  if (rc == 404)
  {
    *err = EDGEX_OK;
  }
  else
  {
    if (err->code == 0)
    {
      result = edgex_getDSresponse_read (ctx.buff);
    }
  }

  curl_free (ename);
  free (ctx.buff);
  return result;
}

void edgex_metadata_client_create_deviceservice
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  const edgex_deviceservice *newds,
  devsdk_error *err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  char *json;

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v2/deviceservice",
    endpoints->metadata.host,
    endpoints->metadata.port
  );
  json = edgex_createDSreq_write (newds);
  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);
  json_free_serialized_string (json);
  free (ctx.buff);
}

edgex_device *edgex_metadata_client_get_devices
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  const char *servicename,
  devsdk_error *err
)
{
  edgex_ctx ctx;
  char *ename;
  edgex_device *result = 0;
  char url[URL_BUF_SIZE];

  ename = curl_easy_escape (NULL, servicename, 0);
  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v2/device/service/name/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    ename
  );

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  curl_free (ename);
  if (err->code)
  {
    free (ctx.buff);
    return 0;
  }

  result = edgex_devices_read (lc, ctx.buff);
  free (ctx.buff);
  return result;
}

char *edgex_metadata_client_add_device
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  const char *name,
  const char *description,
  const devsdk_strings *labels,
  edgex_device_adminstate adminstate,
  devsdk_protocols *protocols,
  edgex_device_autoevents *autos,
  const char *service_name,
  const char *profile_name,
  devsdk_error *err
)
{
  char *result = NULL;
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  char *json;
  edgex_device *dev = malloc (sizeof (edgex_device));

  memset (dev, 0, sizeof (edgex_device));
  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v2/device",
    endpoints->metadata.host,
    endpoints->metadata.port
  );
  dev->name = (char *)name;
  dev->description = (char *)description;
  dev->adminState = adminstate;
  dev->operatingState = UP;
  dev->labels = (devsdk_strings *)labels;
  dev->protocols = (devsdk_protocols *)protocols;
  dev->servicename = (char *)service_name;
  dev->profile = malloc (sizeof (edgex_deviceprofile));
  memset (dev->profile, 0, sizeof (edgex_deviceprofile));
  dev->profile->name = (char *)profile_name;
  dev->autos = autos;
  json = edgex_createdevicereq_write (dev);
  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);
  if (err->code == 0)
  {
    result = edgex_id_from_response (ctx.buff);
  }
  else
  {
    iot_log_info
    (
      lc,
      "edgex_metadata_client_add_device: %s: %s",
      err->reason,
      ctx.buff
    );
  }
  free (ctx.buff);
  free (dev->profile);
  json_free_serialized_string (json);
  free (dev);

  return result;
}

void edgex_metadata_client_add_profile_jobj (iot_logger_t *lc, edgex_service_endpoints *endpoints, JSON_Object *jobj, devsdk_error *err)
{
  if (!json_object_get_string (jobj, "apiVersion"))
  {
    json_object_set_string (jobj, "apiVersion", "2");
  }
  JSON_Value *reqval = edgex_wrap_request ("Profile", json_object_get_wrapping_value (jobj));
  char *json = json_serialize_to_string (reqval);
  edgex_ctx ctx;
  *err = EDGEX_OK;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/v2/deviceprofile", endpoints->metadata.host, endpoints->metadata.port);
  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);
  if (err->code == 0)
  {
    iot_log_info (lc, "Device profile %s created", json_object_get_string (jobj, "name"));
  }
  else
  {
    iot_log_info (lc, "edgex_metadata_client_add_profile_jobj: %s: %s", err->reason, ctx.buff);
  }
  json_value_free (reqval);
  free (ctx.buff);
  json_free_serialized_string (json);
}

void edgex_metadata_client_add_device_jobj (iot_logger_t *lc, edgex_service_endpoints *endpoints, JSON_Object *jobj, devsdk_error *err)
{
  if (!json_object_get_string (jobj, "adminState"))
  {
    json_object_set_string (jobj, "adminState", "UNLOCKED");
  }
  if (!json_object_get_string (jobj, "operatingState"))
  {
    json_object_set_string (jobj, "operatingState", "UP");
  }
  if (!json_object_get_string (jobj, "apiVersion"))
  {
    json_object_set_string (jobj, "apiVersion", "2");
  }
  JSON_Value *reqval = edgex_wrap_request ("Device", json_object_get_wrapping_value (jobj));
  char *json = json_serialize_to_string (reqval);
  edgex_ctx ctx;
  *err = EDGEX_OK;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/v2/device", endpoints->metadata.host, endpoints->metadata.port);
  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);
  if (err->code == 0)
  {
    char *id = edgex_id_from_response (ctx.buff);
    iot_log_info (lc, "Device %s created with id %s", json_object_get_string (jobj, "name"), id);
    free (id);
  }
  else
  {
    iot_log_info (lc, "edgex_metadata_client_add_device_jobj: %s: %s", err->reason, ctx.buff);
  }
  json_value_free (reqval);
  free (ctx.buff);
  json_free_serialized_string (json);
}

void edgex_metadata_client_add_or_modify_device
(
  iot_logger_t *lc,
  edgex_service_endpoints * endpoints,
  const char * name,
  const char * description,
  const devsdk_strings * labels,
  edgex_device_adminstate adminstate,
  devsdk_protocols * protocols,
  edgex_device_autoevents *autos,
  const char * service_name,
  const char * profile_name
)
{
  edgex_ctx ctx;
  devsdk_error err = EDGEX_OK;
  char url[URL_BUF_SIZE];
  char *json;
  edgex_device *dev = malloc (sizeof (edgex_device));

  memset (dev, 0, sizeof (edgex_device));
  memset (&ctx, 0, sizeof (edgex_ctx));

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/v2/device", endpoints->metadata.host, endpoints->metadata.port);

  dev->name = (char *)name;
  dev->description = (char *)description;
  dev->adminState = adminstate;
  dev->operatingState = UP;
  dev->labels = (devsdk_strings *)labels;
  dev->protocols = protocols;
  dev->autos = autos;
  dev->servicename = (char *)service_name;
  dev->profile = calloc (1, sizeof (edgex_deviceprofile));
  dev->profile->name = (char *)profile_name;

  json = edgex_createdevicereq_write (dev);
  if (edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, &err) == 409)
  {
    if (edgex_metadata_client_check_device (lc, endpoints, name))
    {
      iot_log_info (lc, "edgex_metadata_client_add_or_update_device: updating device %s", name);
      free (ctx.buff);
      memset (&ctx, 0, sizeof (edgex_ctx));
      err = EDGEX_OK;
      edgex_http_patch (lc, &ctx, url, json, edgex_http_write_cb, &err);
    }
  }

  if (err.code != 0)
  {
    iot_log_error (lc, "edgex_metadata_client_add_or_update_device: %s: %s", err.reason, ctx.buff);
  }

  free (ctx.buff);
  free (dev->profile);
  free (json);
  free (dev);
}

bool edgex_metadata_client_check_device (iot_logger_t *lc, edgex_service_endpoints *endpoints, const char *devicename)
{
  edgex_ctx ctx;
  devsdk_error err;
  char *ename;
  bool result = false;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  ename = curl_easy_escape (NULL, devicename, 0);

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/v2/device/check/name/%s", endpoints->metadata.host, endpoints->metadata.port, ename);

  result = (edgex_http_get (lc, &ctx, url, edgex_http_write_cb, &err) == 200);

  free (ctx.buff);
  curl_free (ename);
  return result;
}

void edgex_metadata_client_update_device
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const char * name,
  const char * description,
  const devsdk_strings * labels,
  const char * profile_name,
  devsdk_error * err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  char *json;

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v2/device",
    endpoints->metadata.host,
    endpoints->metadata.port
  );

  json = edgex_device_write_sparse
    (name, description, labels, profile_name);

  edgex_http_patch (lc, &ctx, url, json, edgex_http_write_cb, err);
  if (err->code != 0)
  {
    iot_log_info
    (
      lc,
      "edgex_metadata_client_update_device: %s: %s",
      err->reason,
      ctx.buff
    );
  }
  free (ctx.buff);
  json_free_serialized_string (json);
}

void edgex_metadata_client_delete_device_byname
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const char * devicename,
  devsdk_error * err
)
{
  edgex_ctx ctx;
  char *ename;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  ename = curl_easy_escape (NULL, devicename, 0);
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v2/device/name/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    ename
  );

  edgex_http_delete (lc, &ctx, url, edgex_http_write_cb, err);

  curl_free (ename);
  free (ctx.buff);
}

edgex_watcher *edgex_metadata_client_get_watchers
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const char * servicename,
  devsdk_error * err
)
{
  edgex_ctx ctx;
  char *ename;
  edgex_watcher *result = 0;
  char url[URL_BUF_SIZE];

  ename = curl_easy_escape (NULL, servicename, 0);
  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v2/provisionwatcher/service/name/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    ename
  );

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  curl_free (ename);
  if (err->code)
  {
    free (ctx.buff);
    return 0;
  }

  result = edgex_watchers_read (ctx.buff);
  free (ctx.buff);
  *err = EDGEX_OK;
  return result;
}
