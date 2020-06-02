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
    "http://%s:%u/api/v1/deviceprofile/name/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    ename
  );

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  if (err->code == 0)
  {
    result = edgex_deviceprofile_read (lc, ctx.buff);
    if (!result)
    {
      *err = EDGEX_PROFILE_PARSE_ERROR;
    }
  }
  curl_free (ename);
  free (ctx.buff);
  return result;
}

JSON_Value *edgex_metadata_client_get_config
  (iot_logger_t *lc, edgex_service_endpoints *endpoints, devsdk_error *err)
{
  edgex_ctx ctx;
  JSON_Value *result = NULL;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/config",
    endpoints->metadata.host,
    endpoints->metadata.port
  );

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  if (err->code == 0)
  {
    result = json_parse_string (ctx.buff);
  }
  free (ctx.buff);
  return result;
}

void edgex_metadata_client_set_device_opstate
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  const char *deviceid,
  edgex_device_operatingstate opstate,
  devsdk_error *err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  char data[sizeof ("{*operatingstate*:*disabled*}")];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/v1/device/%s", endpoints->metadata.host, endpoints->metadata.port, deviceid);
  sprintf (data, "{\"operatingstate\":\"%s\"}", (opstate == ENABLED) ? "enabled" : "disabled");

  edgex_http_put (lc, &ctx, url, data, edgex_http_write_cb, err);
  free (ctx.buff);
}

void edgex_metadata_client_set_device_opstate_byname
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
  char data[sizeof ("{*operatingstate*:*disabled*}")];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/v1/device/name/%s", endpoints->metadata.host, endpoints->metadata.port, devicename);
  sprintf (data, "{\"operatingstate\":\"%s\"}", (opstate == ENABLED) ? "enabled" : "disabled");

  edgex_http_put (lc, &ctx, url, data, edgex_http_write_cb, err);
  free (ctx.buff);
}

void edgex_metadata_client_set_device_adminstate
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  const char *deviceid,
  edgex_device_adminstate adminstate,
  devsdk_error *err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  char data[sizeof ("{*adminstate*:*unlocked*}")];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/v1/device/%s", endpoints->metadata.host, endpoints->metadata.port, deviceid);
  sprintf (data, "{\"adminstate\":\"%s\"}", (adminstate == LOCKED) ? "locked" : "unlocked");

  edgex_http_put (lc, &ctx, url, data, edgex_http_write_cb, err);
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
  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/v1/device/name/%s/lastconnected/%ld", endpoints->metadata.host, endpoints->metadata.port, devicename, iot_time_msecs ());

  edgex_http_put (lc, &ctx, url, NULL, edgex_http_write_cb, err);
  free (ctx.buff);
}

char *edgex_metadata_client_create_deviceprofile
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  const edgex_deviceprofile *newdp,
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
    "http://%s:%u/api/v1/deviceprofile",
    endpoints->metadata.host,
    endpoints->metadata.port
  );
  json = edgex_deviceprofile_write (newdp, true);
  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);
  json_free_serialized_string (json);
  return ctx.buff;
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
    "http://%s:%u/api/v1/deviceprofile/uploadfile",
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
    "http://%s:%u/api/v1/deviceservice/name/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    ename
  );

  rc = edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  if (rc == 404)
  {
    *err = EDGEX_OK;
  }

  if (err->code == 0)
  {
    result = edgex_deviceservice_read (ctx.buff);
  }

  curl_free (ename);
  free (ctx.buff);
  return result;
}

char *edgex_metadata_client_create_deviceservice
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
    "http://%s:%u/api/v1/deviceservice",
    endpoints->metadata.host,
    endpoints->metadata.port
  );
  json = edgex_deviceservice_write (newds, true);
  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);
  json_free_serialized_string (json);
  return ctx.buff;
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
    "http://%s:%u/api/v1/device/servicename/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    ename
  );

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  curl_free (ename);
  if (err->code)
  {
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
    "http://%s:%u/api/v1/device",
    endpoints->metadata.host,
    endpoints->metadata.port
  );
  dev->name = (char *)name;
  dev->description = (char *)description;
  dev->adminState = adminstate;
  dev->operatingState = ENABLED;
  dev->labels = (devsdk_strings *)labels;
  dev->protocols = (devsdk_protocols *)protocols;
  dev->service = malloc (sizeof (edgex_deviceservice));
  memset (dev->service, 0, sizeof (edgex_deviceservice));
  dev->service->name = (char *)service_name;
  dev->profile = malloc (sizeof (edgex_deviceprofile));
  memset (dev->profile, 0, sizeof (edgex_deviceprofile));
  dev->profile->name = (char *)profile_name;
  dev->autos = autos;
  json = edgex_device_write (dev, true);
  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);
  if (err->code == 0)
  {
    result = ctx.buff;
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
    free (ctx.buff);
  }
  free (dev->service);
  free (dev->profile);
  json_free_serialized_string (json);
  free (dev);

  return result;
}

edgex_device *edgex_metadata_client_get_device
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  const char *deviceid,
  devsdk_error *err
)
{
  edgex_ctx ctx;
  edgex_device *result = 0;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/device/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    deviceid
  );

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  if (err->code)
  {
    return 0;
  }

  result = edgex_device_read (lc, ctx.buff);
  free (ctx.buff);
  *err = EDGEX_OK;
  return result;
}

void edgex_metadata_client_update_device
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const char * name,
  const char * id,
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
    "http://%s:%u/api/v1/device",
    endpoints->metadata.host,
    endpoints->metadata.port
  );

  json = edgex_device_write_sparse
    (name, id, description, labels, profile_name);

  edgex_http_put (lc, &ctx, url, json, edgex_http_write_cb, err);
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

void edgex_metadata_client_delete_device
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const char * deviceid,
  devsdk_error * err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/device/id/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    deviceid
  );

  edgex_http_delete (lc, &ctx, url, edgex_http_write_cb, err);

  free (ctx.buff);
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
    "http://%s:%u/api/v1/device/name/%s",
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
    "http://%s:%u/api/v1/provisionwatcher/servicename/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    ename
  );

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  curl_free (ename);
  if (err->code)
  {
    return 0;
  }

  result = edgex_watchers_read (ctx.buff);
  free (ctx.buff);
  *err = EDGEX_OK;
  return result;
}

edgex_watcher *edgex_metadata_client_get_watcher
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  const char * watcherId,
  devsdk_error * err
)
{
  edgex_ctx ctx;
  edgex_watcher *result = 0;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/provisionwatcher/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    watcherId
  );

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  if (err->code)
  {
    return 0;
  }

  result = edgex_watcher_read (ctx.buff);
  free (ctx.buff);
  *err = EDGEX_OK;
  return result;
}

edgex_addressable *edgex_metadata_client_get_addressable
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  const char *name,
  devsdk_error *err
)
{
  edgex_ctx ctx;
  char *ename;
  edgex_addressable *result = 0;
  char url[URL_BUF_SIZE];
  long rc;

  memset (&ctx, 0, sizeof (edgex_ctx));
  ename = curl_easy_escape (NULL, name, 0);
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/addressable/name/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    ename
  );

  rc = edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  curl_free (ename);
  if (err->code)
  {
    if (rc == 404)
    {
      *err = EDGEX_OK;
    }
    free (ctx.buff);
    return 0;
  }

  result = edgex_addressable_read (ctx.buff);
  free (ctx.buff);
  *err = EDGEX_OK;
  return result;
}

char *edgex_metadata_client_create_addressable
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  const edgex_addressable *newadd,
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
    "http://%s:%u/api/v1/addressable",
    endpoints->metadata.host,
    endpoints->metadata.port
  );
  json = edgex_addressable_write (newadd, true);
  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);
  json_free_serialized_string (json);
  return ctx.buff;
}

void edgex_metadata_client_update_addressable
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  const edgex_addressable *addressable,
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
    "http://%s:%u/api/v1/addressable",
    endpoints->metadata.host,
    endpoints->metadata.port
  );
  json = edgex_addressable_write (addressable, false);
  edgex_http_put (lc, &ctx, url, json, edgex_http_write_cb, err);
  json_free_serialized_string (json);
  free (ctx.buff);
}
