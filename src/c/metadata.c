/*
 * Copyright (c) 2018-2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <curl/curl.h>
#include <errno.h>

#include "api.h"
#include "metadata.h"
#include "edgex-rest.h"
#include "dto-read.h"
#include "rest.h"
#include "errorlist.h"
#include "config.h"
#include "profiles.h"
#include "iot/time.h"

edgex_deviceprofile *edgex_metadata_client_get_deviceprofile
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  edgex_secret_provider_t * secretprovider,
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
    "http://%s:%u/api/" EDGEX_API_VERSION "/deviceprofile/name/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    ename
  );

  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);  
  ctx.jwt_token = iot_data_string(jwt_data);

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  if (err->code == 0)
  {
    iot_data_t *obj = iot_data_from_json (ctx.buff);
    if (obj)
    {
      result = edgex_profile_read (iot_data_string_map_get (obj, "profile"));
      iot_data_free (obj);
    }
    else
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
  edgex_secret_provider_t * secretprovider,
  const char *devicename,
  edgex_device_operatingstate opstate,
  devsdk_error *err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  char *json = edgex_updateDevOpreq_write (devicename, opstate);

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/" EDGEX_API_VERSION" /device", endpoints->metadata.host, endpoints->metadata.port);

  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);  
  ctx.jwt_token = iot_data_string(jwt_data);

  edgex_http_patch (lc, &ctx, url, json, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  json_free_serialized_string (json);
  free (ctx.buff);
}

void edgex_metadata_client_update_deviceservice
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * name,
  const char * baseaddr,
  devsdk_error * err
)
{ 
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  char *json = edgex_updateDSreq_write (name, baseaddr);

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/" EDGEX_API_VERSION "/deviceservice", endpoints->metadata.host, endpoints->metadata.port);

  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);  
  ctx.jwt_token = iot_data_string(jwt_data);

  edgex_http_patch (lc, &ctx, url, json, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  json_free_serialized_string (json);
  free (ctx.buff);
}

void edgex_metadata_client_update_lastconnected
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * devicename,
  devsdk_error * err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  char *json = edgex_updateDevLCreq_write (devicename, iot_time_msecs ());

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/" EDGEX_API_VERSION" /device", endpoints->metadata.host, endpoints->metadata.port);

  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);  
  ctx.jwt_token = iot_data_string(jwt_data);
  
  edgex_http_patch (lc, &ctx, url, json, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  json_free_serialized_string (json);
  free (ctx.buff);
}

char *edgex_metadata_client_create_deviceprofile_file
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  edgex_secret_provider_t * secretprovider,
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
    "http://%s:%u/api/" EDGEX_API_VERSION"/deviceprofile/uploadfile",
    endpoints->metadata.host,
    endpoints->metadata.port
  );

  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);  
  ctx.jwt_token = iot_data_string(jwt_data);
  
  edgex_http_postfile (lc, &ctx, url, filename, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  return ctx.buff;
}

edgex_deviceservice *edgex_metadata_client_get_deviceservice
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  edgex_secret_provider_t * secretprovider,
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
    "http://%s:%u/api/" EDGEX_API_VERSION "/deviceservice/name/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    ename
  );

  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);  
  ctx.jwt_token = iot_data_string(jwt_data);

  rc = edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

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
  edgex_secret_provider_t * secretprovider,
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
    "http://%s:%u/api/" EDGEX_API_VERSION "/deviceservice",
    endpoints->metadata.host,
    endpoints->metadata.port
  );
  json = edgex_createDSreq_write (newds);

  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);  
  ctx.jwt_token = iot_data_string(jwt_data);

  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  json_free_serialized_string (json);
  free (ctx.buff);
}

edgex_device *edgex_metadata_client_get_devices
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  edgex_secret_provider_t * secretprovider,
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
    "http://%s:%u/api/" EDGEX_API_VERSION "/device/service/name/%s?offset=0&limit=-1",
    endpoints->metadata.host,
    endpoints->metadata.port,
    ename
  );

  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);  
  ctx.jwt_token = iot_data_string(jwt_data);

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

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

static void edgex_metadata_process_device_response (iot_logger_t *lc, devsdk_error *err, const char *buff, const char *devname, char **id_ret)
{
  if (err->code == 0)
  {
    iot_data_t *response = iot_data_from_json (buff);
    if (response)
    {
      const iot_data_t *entry = iot_data_vector_get (response, 0);
      if (entry)
      {
        const char *id = iot_data_string_map_get_string (entry, "id");
        if (id)
        {
          iot_log_info (lc, "Device %s created with id %s", devname, id);
          if (id_ret)
          {
            *id_ret = strdup (id);
          }
        }
        else
        {
          iot_log_error (lc, "Device %s create failed: %s", devname, iot_data_string_map_get_string (entry, "message"));
        }
      }
      iot_data_free (response);
    }
  }
  else
  {
    iot_log_info (lc, "Device %s create failed: %s: %s", devname, err->reason, buff);
  }
}

char *edgex_metadata_client_add_device
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  edgex_secret_provider_t * secretprovider,
  const char *name,
  const char *parent,
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
    "http://%s:%u/api/" EDGEX_API_VERSION "/device",
    endpoints->metadata.host,
    endpoints->metadata.port
  );
  dev->name = (char *)name;
  dev->parent = (char *)parent;
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

  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);  
  ctx.jwt_token = iot_data_string(jwt_data);

  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  edgex_metadata_process_device_response (lc, err, ctx.buff, name, &result);
  free (ctx.buff);
  free (dev->profile);
  json_free_serialized_string (json);
  free (dev);

  return result;
}

void edgex_metadata_client_add_profile_jobj (iot_logger_t *lc, edgex_service_endpoints *endpoints, edgex_secret_provider_t * secretprovider, JSON_Object *jobj, devsdk_error *err)
{
  if (!json_object_get_string (jobj, "apiVersion"))
  {
    json_object_set_string (jobj, "apiVersion", EDGEX_API_VERSION);
  }
  JSON_Value *reqval = edgex_wrap_request ("Profile", json_object_get_wrapping_value (jobj));
  char *json = json_serialize_to_string (reqval);
  edgex_ctx ctx;
  *err = EDGEX_OK;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/" EDGEX_API_VERSION "/deviceprofile", endpoints->metadata.host, endpoints->metadata.port);

  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);  
  ctx.jwt_token = iot_data_string(jwt_data);

  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

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

void edgex_metadata_client_add_device_jobj (iot_logger_t *lc, edgex_service_endpoints *endpoints, edgex_secret_provider_t * secretprovider, JSON_Object *jobj, devsdk_error *err)
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
    json_object_set_string (jobj, "apiVersion", EDGEX_API_VERSION);
  }
  JSON_Value *reqval = edgex_wrap_request ("Device", json_object_get_wrapping_value (jobj));
  char *json = json_serialize_to_string (reqval);
  edgex_ctx ctx;
  *err = EDGEX_OK;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/" EDGEX_API_VERSION "/device", endpoints->metadata.host, endpoints->metadata.port);
  
  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);  
  ctx.jwt_token = iot_data_string(jwt_data);

  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  edgex_metadata_process_device_response (lc, err, ctx.buff, json_object_get_string (jobj, "name"), NULL);
  json_value_free (reqval);
  free (ctx.buff);
  json_free_serialized_string (json);
}

void edgex_metadata_client_add_or_modify_device
(
  iot_logger_t *lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * name,
  const char *parent,
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

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/" EDGEX_API_VERSION "/device", endpoints->metadata.host, endpoints->metadata.port);

  dev->name = (char *)name;
  dev->parent = (char *)parent;
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

  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);  
  ctx.jwt_token = iot_data_string(jwt_data);

  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, &err);

  uint statusCode = 0;
  JSON_Value *val = json_parse_string(ctx.buff);
  JSON_Array *resps = json_value_get_array (val);
  size_t nresps = json_array_get_count (resps);
  if (nresps)
  {
    JSON_Object *obj = json_array_get_object (resps, 0);
    statusCode = json_object_get_uint (obj, "statusCode");

  }
  json_value_free(val);

  if (statusCode == 409)
  {
    if (edgex_metadata_client_check_device (lc, endpoints, secretprovider, name))
    {
      iot_log_info (lc, "edgex_metadata_client_add_or_update_device: updating device %s", name);
      free (ctx.buff);
      memset (&ctx, 0, sizeof (edgex_ctx));
      err = EDGEX_OK;
      edgex_http_patch (lc, &ctx, url, json, edgex_http_write_cb, &err);
    }
  }

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  if (err.code != 0)
  {
    iot_log_error (lc, "edgex_metadata_client_add_or_update_device: %s: %s", err.reason, ctx.buff);
  }

  free (ctx.buff);
  free (dev->profile);
  free (json);
  free (dev);
}

bool edgex_metadata_client_check_device (iot_logger_t *lc, edgex_service_endpoints *endpoints, edgex_secret_provider_t * secretprovider, const char *devicename)
{
  edgex_ctx ctx;
  devsdk_error err;
  char *ename;
  bool result = false;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  ename = curl_easy_escape (NULL, devicename, 0);

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/" EDGEX_API_VERSION "/device/check/name/%s", endpoints->metadata.host, endpoints->metadata.port, ename);

  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);  
  ctx.jwt_token = iot_data_string(jwt_data);

  result = (edgex_http_get (lc, &ctx, url, edgex_http_write_cb, &err) == 200);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  free (ctx.buff);
  curl_free (ename);
  return result;
}

void edgex_metadata_client_update_device
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * name,
  const char *parent,
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
    "http://%s:%u/api/" EDGEX_API_VERSION "/device",
    endpoints->metadata.host,
    endpoints->metadata.port
  );

  json = edgex_device_write_sparse
    (name, parent, description, labels, profile_name);

  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);  
  ctx.jwt_token = iot_data_string(jwt_data);

  edgex_http_patch (lc, &ctx, url, json, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

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
  edgex_secret_provider_t * secretprovider,
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
    "http://%s:%u/api/" EDGEX_API_VERSION "/device/name/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    ename
  );

  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);  
  ctx.jwt_token = iot_data_string(jwt_data);

  edgex_http_delete (lc, &ctx, url, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  curl_free (ename);
  free (ctx.buff);
}

edgex_watcher *edgex_metadata_client_get_watchers
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
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
    "http://%s:%u/api/" EDGEX_API_VERSION "/provisionwatcher/service/name/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    ename
  );

  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);  
  ctx.jwt_token = iot_data_string(jwt_data);

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  curl_free (ename);
  if (err->code)
  {
    free (ctx.buff);
    return 0;
  }

  iot_data_t *p = iot_data_from_json (ctx.buff);
  result = edgex_pws_read (p);
  iot_data_free (p);
  free (ctx.buff);
  *err = EDGEX_OK;
  return result;
}
