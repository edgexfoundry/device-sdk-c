/*
 * Copyright (c) 2018-2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "devsdk/devsdk-base.h"
#include "consul.h"
#include "edgex-rest.h"
#include "rest.h"
#include "errorlist.h"
#include "parson.h"
#include "api.h"
#include "iot/base64.h"
#include "iot/time.h"

#define CONF_PREFIX "edgex/v3/"

typedef struct consul_impl_t
{
  iot_logger_t *lc;
  iot_threadpool_t *pool;
  edgex_secret_provider_t *sp;
  char *host;
  uint16_t port;
} consul_impl_t;

static bool edgex_consul_client_init (void *impl, iot_logger_t *logger, iot_threadpool_t *pool, edgex_secret_provider_t *sp, const char *url)
{
  bool ok = false;
  consul_impl_t *consul = (consul_impl_t *)impl;
  consul->lc = logger;
  consul->pool = pool;
  consul->sp = sp;
  char *pos = strstr (url, "://");
  if (pos)
  {
    pos += 3;
    char *colon = strchr (pos, ':');
    if (colon && strlen (colon + 1))
    {
      char *end;
      uint16_t port = strtoul (colon + 1, &end, 10);
      if (*end == '\0')
      {
        consul->port = port;
        consul->host = strndup (pos, colon - pos);
        ok = true;
      }
      else
      {
        iot_log_error (logger, "Unable to parse \"%s\" for port number for registry", colon + 1);
      }
    }
  }
  return ok;
}

static void edgex_consul_client_free (void *impl)
{
  consul_impl_t *consul = (consul_impl_t *)impl;
  free (consul->host);
  free (impl);
}

static devsdk_nvpairs *read_pairs (iot_logger_t *lc, const char *json, devsdk_error *err)
{
  const char *key;
  const char *keyindex;
  const char *enc;
  char *kval;
  size_t nconfs;
  size_t rsize;
  JSON_Object *obj;
  devsdk_nvpairs *result = NULL;

  JSON_Value *val = json_parse_string (json);
  JSON_Array *configs = json_value_get_array (val);

  nconfs = json_array_get_count (configs);
  for (size_t i = 0; i < nconfs; i++)
  {
    obj = json_array_get_object (configs, i);
    key = json_object_get_string (obj, "Key");
    if (key)
    {
      // Skip the prefix and device name in the key
      if
      (
        strncmp (key, CONF_PREFIX, sizeof (CONF_PREFIX) - 1) == 0 &&
        (keyindex = strchr (key + sizeof (CONF_PREFIX) - 1, '/')) &&
        *(++keyindex)
      )
      {
        enc = json_object_get_string (obj, "Value");
        if (enc)
        {
          rsize = iot_b64_maxdecodesize (enc);
          kval = malloc (rsize + 1);
          if (iot_b64_decode (enc, kval, &rsize))
          {
            kval[rsize] = '\0';
            result = devsdk_nvpairs_new (keyindex, kval, result);
          }
          else
          {
            iot_log_error (lc, "Unable to decode Value %s (for config key %s)", enc, key);
            *err = EDGEX_CONSUL_RESPONSE;
          }
          free (kval);
        }
        else
        {
          result = devsdk_nvpairs_new (keyindex, "", result);
        }
      }
      else
      {
        iot_log_error (lc, "Unexpected Key %s returned from consul", key);
        *err = EDGEX_CONSUL_RESPONSE;
      }
    }
    else
    {
      iot_log_error (lc, "No Key field in consul response. JSON was %s", json);
      *err = EDGEX_CONSUL_RESPONSE;
    }
  }
  json_value_free (val);
  return result;
}

struct updatejob
{
  char *url;
  edgex_secret_provider_t *sp;
  iot_logger_t *lc;
  devsdk_registry_updatefn updater;
  void *updatectx;
  atomic_bool *updatedone;
};

static void *poll_consul (void *p)
{
  char *urltail;
  edgex_ctx ctx;
  devsdk_nvpairs index;
  devsdk_nvpairs *conf;
  devsdk_error err;

  struct updatejob *job = (struct updatejob *)p;
  urltail = job->url + strlen (job->url);

  index.name = "X-Consul-Index";
  index.value = NULL;
  index.next = NULL;

  while (true)
  {
    memset (&ctx, 0, sizeof (edgex_ctx));
    err = EDGEX_OK;
    if (index.value)
    {
      sprintf (urltail, "&index=%s", index.value);
    }
    free (index.value);
    index.value = NULL;
    edgex_secrets_getregtoken (job->sp, &ctx);
    ctx.rsphdrs = &index;
    ctx.aborter = job->updatedone;
    edgex_secrets_releaseregtoken (job->sp);
    edgex_http_get (job->lc, &ctx, job->url, edgex_http_write_cb, &err);
    if (*job->updatedone)
    {
      free (ctx.buff);
      break;
    }
    if (err.code == 0)
    {
      conf = read_pairs (job->lc, ctx.buff, &err);
      if (err.code == 0)
      {
        job->updater (job->updatectx, conf);
      }
      devsdk_nvpairs_free (conf);
    }
    else
    {
      sleep (5);
    }
    free (ctx.buff);
  }
  free (index.value);
  free (job->url);
  free (job);
  return NULL;
}

static devsdk_nvpairs *edgex_consul_client_get_common_config
(
  void *impl,
  devsdk_registry_updatefn updater,
  void *updatectx,
  atomic_bool *updatedone,
  devsdk_error *err,
  const devsdk_timeout *timeout
)
{
  consul_impl_t *consul = (consul_impl_t *)impl;
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  devsdk_nvpairs *result, *privateConfig, *ccReady = NULL;

  memset (&ctx, 0, sizeof (edgex_ctx));
  edgex_secrets_getregtoken (consul->sp, &ctx);
  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/v1/kv/" CONF_PREFIX "%s", consul->host, consul->port, "core-common-config-bootstrapper/IsCommonConfigReady");

  uint64_t t1, t2;
  while (true)
  {
    t1 = iot_time_msecs ();
    *err = EDGEX_OK;
    edgex_http_get (consul->lc, &ctx, url, edgex_http_write_cb, err);
    edgex_secrets_releaseregtoken (consul->sp);
    if (err->code == 0)
    {
      ccReady = read_pairs (consul->lc, ctx.buff, err);
      const char *isCommonConfigReady = devsdk_nvpairs_value(ccReady, "IsCommonConfigReady");
      if (isCommonConfigReady && strcmp(isCommonConfigReady, "true") == 0)
      {
        devsdk_nvpairs_free (ccReady);
        break;
      }
    }
    t2 = iot_time_msecs ();
    if (t2 > timeout->deadline - timeout->interval)
    {
      *err = EDGEX_REMOTE_SERVER_DOWN;
      devsdk_nvpairs_free (ccReady);
      break;
    }
    if (timeout->interval > t2 - t1)
    {
      // waiting for Common Configuration to be available from config provider
      iot_log_warn(consul->lc, "waiting for Common Configuration to be available from config provider.");
      iot_wait_msecs (timeout->interval - (t2 - t1));
    }
    devsdk_nvpairs_free (ccReady);
  }

  free (ctx.buff);

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/v1/kv/" CONF_PREFIX "%s?recurse=true", consul->host, consul->port, "core-common-config-bootstrapper/all-services");
  edgex_http_get (consul->lc, &ctx, url, edgex_http_write_cb, err);
  edgex_secrets_releaseregtoken (consul->sp);
  if (err->code == 0)
  {
    result = read_pairs (consul->lc, ctx.buff, err);
    if (err->code)
    {
      devsdk_nvpairs_free (result);
      result = NULL;
    }
  }

  free (ctx.buff);

  devsdk_nvpairs *originalResult = result;
  while (result)
  {
    char *name = result->name;
    char *pos = strstr(name, "all-services/");
    if (pos)
    {
      result->name = strdup(pos + strlen("all-services/"));
      free(name);
    }
    result = result->next;
  }
  result = originalResult;

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/v1/kv/" CONF_PREFIX "%s?recurse=true", consul->host, consul->port, "core-common-config-bootstrapper/device-services");
  edgex_http_get (consul->lc, &ctx, url, edgex_http_write_cb, err);
  edgex_secrets_releaseregtoken (consul->sp);
  if (err->code == 0)
  {
      privateConfig = read_pairs (consul->lc, ctx.buff, err);
    free (ctx.buff);
  }

  devsdk_nvpairs *originalPrivateResult = privateConfig;
  while (privateConfig)
  {
    char *name = privateConfig->name;
    char *pos = strstr(name, "device-services/");
    if (pos)
    {
      result = devsdk_nvpairs_new((pos + strlen("device-services/")), privateConfig->value, result);
    }
      privateConfig = privateConfig->next;
  }
  devsdk_nvpairs_free(originalPrivateResult);

  struct updatejob *job_all_service = malloc (sizeof (struct updatejob));
  job_all_service->url = malloc (URL_BUF_SIZE);
  snprintf (job_all_service->url, URL_BUF_SIZE - 1, "http://%s:%u/v1/kv/" CONF_PREFIX "%s/Writable?recurse=true", consul->host, consul->port, "core-common-config-bootstrapper/all-services");
  job_all_service->lc = consul->lc;
  job_all_service->updater = updater;
  job_all_service->updatectx = updatectx;
  job_all_service->updatedone = updatedone;
  job_all_service->sp = consul->sp;
  iot_threadpool_add_work (consul->pool, poll_consul, job_all_service, -1);

  struct updatejob *job_device_service = malloc (sizeof (struct updatejob));
  job_device_service->url = malloc (URL_BUF_SIZE);
  snprintf (job_device_service->url, URL_BUF_SIZE - 1, "http://%s:%u/v1/kv/" CONF_PREFIX "%s/Writable?recurse=true", consul->host, consul->port, "core-common-config-bootstrapper/device-services");
  job_device_service->lc = consul->lc;
  job_device_service->updater = updater;
  job_device_service->updatectx = updatectx;
  job_device_service->updatedone = updatedone;
  job_device_service->sp = consul->sp;
  iot_threadpool_add_work (consul->pool, poll_consul, job_device_service, -1);

  return result;
}

static devsdk_nvpairs *edgex_consul_client_get_config
(
  void *impl,
  const char *servicename,
  devsdk_registry_updatefn updater,
  void *updatectx,
  atomic_bool *updatedone,
  devsdk_error *err
)
{
  consul_impl_t *consul = (consul_impl_t *)impl;
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  devsdk_nvpairs *result = NULL;

  memset (&ctx, 0, sizeof (edgex_ctx));
  edgex_secrets_getregtoken (consul->sp, &ctx);
  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/v1/kv/" CONF_PREFIX "%s?recurse=true", consul->host, consul->port, servicename);
  edgex_http_get (consul->lc, &ctx, url, edgex_http_write_cb, err);
  edgex_secrets_releaseregtoken (consul->sp);
  if (err->code == 0)
  {
    result = read_pairs (consul->lc, ctx.buff, err);
    if (err->code)
    {
      devsdk_nvpairs_free (result);
      result = NULL;
    }
  }

  free (ctx.buff);

  struct updatejob *job = malloc (sizeof (struct updatejob));
  job->url = malloc (URL_BUF_SIZE);
  snprintf (job->url, URL_BUF_SIZE - 1, "http://%s:%u/v1/kv/" CONF_PREFIX "%s/Writable?recurse=true", consul->host, consul->port, servicename);
  job->lc = consul->lc;
  job->updater = updater;
  job->updatectx = updatectx;
  job->updatedone = updatedone;
  job->sp = consul->sp;
  iot_threadpool_add_work (consul->pool, poll_consul, job, -1);

  return result;
}

static char *value_to_b64 (const char *value)
{
  char *result;
  size_t valsz = strlen (value);
  size_t base64sz = iot_b64_encodesize (valsz);
  result = malloc (base64sz);
  iot_b64_encode (value, valsz, result, base64sz);
  return result;
}

static void edgex_consul_client_write_config (void *impl, const char *servicename, const iot_data_t *config, devsdk_error *err)
{
  consul_impl_t *consul = (consul_impl_t *)impl;
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url, URL_BUF_SIZE - 1, "http://%s:%u/v1/txn",
    consul->host, consul->port
  );

  JSON_Value *jresult = json_value_init_array ();
  JSON_Array *jarray = json_value_get_array (jresult);

  iot_data_map_iter_t iter;
  iot_data_map_iter (config, &iter);
  while (iot_data_map_iter_next (&iter))
  {
    char *base64val;
    const iot_data_t *val = iot_data_map_iter_value (&iter);
    if (iot_data_type (val) == IOT_DATA_STRING)
    {
      base64val = value_to_b64 (iot_data_string (val));
    }
    else
    {
      char *json = iot_data_to_json (val);
      base64val = value_to_b64 (json);
      free (json);
    }

    const char *k = iot_data_map_iter_string_key (&iter);
    char *keystr = malloc (strlen (CONF_PREFIX) + strlen (servicename) + strlen (k) + 2);
    sprintf (keystr, "%s%s/%s", CONF_PREFIX, servicename, k);

    JSON_Value *kvfields = json_value_init_object ();
    JSON_Object *obj = json_value_get_object (kvfields);
    json_object_set_string (obj, "Verb", "set");
    json_object_set_string (obj, "Key", keystr);
    json_object_set_string (obj, "Value", base64val);

    JSON_Value *kvcmd = json_value_init_object ();
    json_object_set_value (json_value_get_object (kvcmd), "KV", kvfields);
    json_array_append_value (jarray, kvcmd);

    free (base64val);
    free (keystr);
  }

  char *json = json_serialize_to_string (jresult);
  json_value_free (jresult);

  edgex_secrets_getregtoken (consul->sp, &ctx);
  edgex_http_put (consul->lc, &ctx, url, json, edgex_http_write_cb, err);
  edgex_secrets_releaseregtoken (consul->sp);

  json_free_serialized_string (json);
  free (ctx.buff);
}

static void edgex_consul_client_register_service
(
  void *impl,
  const char *servicename,
  const char *host,
  uint16_t port,
  const char *checkInterval,
  devsdk_error *err
)
{
  consul_impl_t *consul = (consul_impl_t *)impl;
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url, URL_BUF_SIZE - 1, "http://%s:%u/v1/agent/service/register",
    consul->host, consul->port
  );

  JSON_Value *params = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (params);
  json_object_set_string (obj, "Name", servicename);
  json_object_set_string (obj, "Address", host);
  json_object_set_uint (obj, "Port", port);
  if (strlen (checkInterval))
  {
    char myUrl[URL_BUF_SIZE];
    char checkName[URL_BUF_SIZE];
    JSON_Value *checkval = json_value_init_object ();
    snprintf (myUrl, URL_BUF_SIZE - 1, "http://%s:%u" EDGEX_DEV_API3_PING, host, port);
    snprintf (checkName, URL_BUF_SIZE - 1, "Health Check: %s", servicename);
    JSON_Object *checkobj = json_value_get_object (checkval);
    json_object_set_string (checkobj, "Name", checkName);
    json_object_set_string (checkobj, "Interval", checkInterval);
    json_object_set_string (checkobj, "HTTP", myUrl);
    json_object_set_value (obj, "Check", checkval);
  }

  char *json = json_serialize_to_string (params);
  json_value_free (params);

  edgex_secrets_getregtoken (consul->sp, &ctx);
  edgex_http_put (consul->lc, &ctx, url, json, edgex_http_write_cb, err);
  edgex_secrets_releaseregtoken (consul->sp);

  if (err->code)
  {
    iot_log_error (consul->lc, "Register service failed: %s", ctx.buff);
  }
  json_free_serialized_string (json);
  free (ctx.buff);
}

static void edgex_consul_client_deregister_service
(
  void *impl,
  const char *servicename,
  devsdk_error *err
)
{
  consul_impl_t *consul = (consul_impl_t *)impl;
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url, URL_BUF_SIZE - 1, "http://%s:%u/v1/agent/service/deregister/%s",
    consul->host, consul->port, servicename
  );

  edgex_secrets_getregtoken (consul->sp, &ctx);
  edgex_http_put (consul->lc, &ctx, url, NULL, edgex_http_write_cb, err);
  edgex_secrets_releaseregtoken (consul->sp);

  if (err->code)
  {
    iot_log_error (consul->lc, "Deregister service failed: %s", ctx.buff);
  }
  free (ctx.buff);
}

static void edgex_consul_client_query_service
(
  void *impl,
  const char *servicename,
  char **host,
  uint16_t *port,
  devsdk_error *err
)
{
  consul_impl_t *consul = (consul_impl_t *)impl;
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url, URL_BUF_SIZE - 1,
    "http://%s:%u/v1/catalog/service/%s",
    consul->host, consul->port, servicename
  );

  *err = EDGEX_OK;

  edgex_secrets_getregtoken (consul->sp, &ctx);
  edgex_http_get (consul->lc, &ctx, url, edgex_http_write_cb, err);
  edgex_secrets_releaseregtoken (consul->sp);

  if (err->code == 0)
  {
    JSON_Value *val = json_parse_string (ctx.buff);
    JSON_Array *svcs = json_value_get_array (val);

    size_t nsvcs = json_array_get_count (svcs);
    if (nsvcs)
    {
      if (nsvcs != 1)
      {
        iot_log_warn
          (consul->lc, "Multiple instances of %s found, using first.", servicename);
      }
      JSON_Object *obj = json_array_get_object (svcs, 0);
      const char *name = json_object_get_string (obj, "ServiceAddress");
      if (name)
      {
        *host = strdup (name);
        *port = json_object_get_uint (obj, "ServicePort");
      }
      else
      {
        iot_log_error (consul->lc, "consul: no ServiceAddress for %s", servicename);
        *err = EDGEX_BAD_CONFIG;
      }
    }
    else
    {
      iot_log_error (consul->lc, "consul: no service named %s", servicename);
      *err = EDGEX_BAD_CONFIG;
    }

    json_value_free (val);
  }

  free (ctx.buff);
}

static bool edgex_consul_client_ping (void *impl)
{
  consul_impl_t *consul = (consul_impl_t *)impl;
  devsdk_error err = EDGEX_OK;
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/v1/status/leader",
    consul->host,
    consul->port
  );

  edgex_secrets_getregtoken (consul->sp, &ctx);
  edgex_http_get (consul->lc, &ctx, url, NULL, &err);
  edgex_secrets_releaseregtoken (consul->sp);
  return (err.code == 0);
}

void *devsdk_registry_consul_alloc ()
{
  return calloc (1, sizeof (consul_impl_t));
}

const devsdk_registry_impls devsdk_registry_consul_fns =
{
  edgex_consul_client_init,
  edgex_consul_client_ping,
  edgex_consul_client_get_common_config,
  edgex_consul_client_get_config,
  edgex_consul_client_write_config,
  edgex_consul_client_register_service,
  edgex_consul_client_deregister_service,
  edgex_consul_client_query_service,
  edgex_consul_client_free
};
