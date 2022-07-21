/*
 * Copyright (c) 2021-2022
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "secrets-vault.h"
#include "rest.h"
#include "edgex-rest.h"
#include "parson.h"
#include "errorlist.h"
#include "iot/scheduler.h"

typedef struct vault_impl_t
{
  iot_logger_t *lc;
  iot_threadpool_t *thpool;
  iot_scheduler_t *scheduler;
  char *token;
  char *baseurl;
  char *regurl;
  char *tokinfourl;
  char *tokrenewurl;
  char *capath;
  devsdk_nvpairs *regtoken;
  bool bearer;
  pthread_mutex_t mtx;
} vault_impl_t;

static void vault_initctx (vault_impl_t *vault, edgex_ctx *ctx)
{
  memset (ctx, 0, sizeof (edgex_ctx));
  if (vault->capath)
  {
    ctx->cacerts_path = vault->capath;
    ctx->verify_peer = 1;
  }
  if (vault->bearer)
  {
    ctx->jwt_token = vault->token;
  }
  else
  {
    ctx->reqhdrs = devsdk_nvpairs_new ("X-Vault-Token", vault->token, NULL);
  }
}

static void vault_freectx (edgex_ctx *ctx)
{
  devsdk_nvpairs_free (ctx->reqhdrs);
  free (ctx->buff);
}

static iot_data_t *vault_rest_get (vault_impl_t *vault, const char *url)
{
  devsdk_error err = EDGEX_OK;
  iot_data_t *result = NULL;
  edgex_ctx ctx;
  vault_initctx (vault, &ctx);
  edgex_http_get (vault->lc, &ctx, url, edgex_http_write_cb, &err);
  if (err.code == 0)
  {
    result = iot_data_from_json (ctx.buff);
  }
  else
  {
    iot_log_error (vault->lc, "vault: GET %s failed", url);
  }
  vault_freectx (&ctx);
  return result;
}

static void vault_schedule_renewal (vault_impl_t *vault);

static void *vault_perform_renewal (void *v)
{
  vault_impl_t *vault = (vault_impl_t *)v;
  devsdk_error err = EDGEX_OK;
  edgex_ctx ctx;
  vault_initctx (vault, &ctx);

  pthread_mutex_lock (&vault->mtx);
  edgex_http_put (vault->lc, &ctx, vault->tokrenewurl, "", edgex_http_write_cb, &err);
  devsdk_nvpairs_free (vault->regtoken);
  vault->regtoken = NULL;
  pthread_mutex_unlock (&vault->mtx);

  if (err.code == 0)
  {
    vault_schedule_renewal (vault);
  }
  else
  {
    iot_log_error (vault->lc, "vault: error renewing token: %s", ctx.buff);
  }

  vault_freectx (&ctx);
  return NULL;
}

void vault_schedule_renewal (vault_impl_t *vault)
{
  const iot_data_t *data = NULL;
  iot_data_t *info = vault_rest_get (vault, vault->tokinfourl);
  if (info)
  {
    data = iot_data_string_map_get (info, "data");
  }
  if (data)
  {
    if (iot_data_bool (iot_data_string_map_get (data, "renewable")))
    {
      int64_t ttl = iot_data_i64 (iot_data_string_map_get (data, "ttl"));
      int64_t c_ttl = iot_data_i64 (iot_data_string_map_get (data, "creation_ttl"));
      int64_t wait = ttl - c_ttl / 10;
      if (wait < 1)
      {
        vault_perform_renewal (vault);
      }
      else
      {
        iot_schedule_t *job = iot_schedule_create (vault->scheduler, vault_perform_renewal, NULL, vault, 0, IOT_SEC_TO_NS(wait), 1, vault->thpool, -1);
        iot_schedule_add (vault->scheduler, job);
        iot_log_info (vault->lc, "vault: scheduled token refresh in %ld seconds", wait);
      }
    }
    else
    {
      iot_log_info (vault->lc, "vault: access token is non-renewable");
    }
  }
  else
  {
    iot_log_error (vault->lc, "vault: could not obtain token renewal information");
  }
  iot_data_free (info);
}

static bool vault_init (void *impl, iot_logger_t *lc, iot_scheduler_t *sched, iot_threadpool_t *pool, const char *svcname, iot_data_t *config)
{
  vault_impl_t *vault = (vault_impl_t *)impl;
  vault->lc = lc;
  vault->scheduler = sched;
  vault->thpool = pool;

  char *host = malloc (URL_BUF_SIZE);
  snprintf
  (
    host, URL_BUF_SIZE,
    "%s://%s:%u",
    iot_data_string_map_get_string (config, "SecretStore/Protocol"),
    iot_data_string_map_get_string (config, "SecretStore/Host"),
    iot_data_ui16 (iot_data_string_map_get (config, "SecretStore/Port"))
  );
  const char *path = iot_data_string_map_get_string (config, "SecretStore/Path");
  vault->baseurl = malloc (URL_BUF_SIZE);
  snprintf (vault->baseurl, URL_BUF_SIZE, "%s/v1/secret/edgex%s%s%s", host, path[0] == '/' ? "" : "/", path, path[strlen (path) - 1] == '/' ? "" : "/");
  vault->regurl = malloc (URL_BUF_SIZE);
  snprintf (vault->regurl, URL_BUF_SIZE, "%s/v1/consul/creds/%s", host, svcname);
  vault->tokinfourl = malloc (URL_BUF_SIZE);
  snprintf (vault->tokinfourl, URL_BUF_SIZE, "%s/v1/auth/token/lookup-self", host);
  vault->tokrenewurl = malloc (URL_BUF_SIZE);
  snprintf (vault->tokrenewurl, URL_BUF_SIZE, "%s/v1/auth/token/renew-self", host);
  free (host);

  const char *fname = iot_data_string_map_get_string (config, "SecretStore/TokenFile");
  JSON_Value *jval = json_parse_file (fname);
  if (jval)
  {
    JSON_Object *jobj = json_value_get_object (jval);
    JSON_Object *aobj = json_object_get_object (jobj, "auth");
    if (aobj)
    {
      const char *ctok = json_object_get_string (aobj, "client_token");
      if (ctok)
      {
        vault->token = strdup (ctok);
      }
    }
    if (vault->token == NULL)
    {
      iot_log_error (vault->lc, "vault: Unable to find client token in file %s", fname);
      return false;
    }
    json_value_free (jval);
  }
  else
  {
    iot_log_error (vault->lc, "vault: Token file %s doesn't parse as JSON", fname);
    return false;
  }

  const char *capath = iot_data_string_map_get_string (config, "SecretStore/RootCaCertPath");
  if (strlen (capath))
  {
    vault->capath = strdup (capath);
  }

  const char *authtype = iot_data_string_map_get_string (config, "SecretStore/Authentication/AuthType");
  if (strcmp (authtype, "Authorization") == 0)
  {
    vault->bearer = true;
  }
  // TODO: SecretStore/ServerName (unsupported in libcurl?)

  vault_schedule_renewal (vault);
  return true;
}

static void vault_reconfigure (void *impl, iot_data_t *config)
{
}

static iot_data_t *vault_get (void *impl, const char *path)
{
  const iot_data_t *d = NULL;
  iot_data_t *result;
  vault_impl_t *vault = (vault_impl_t *)impl;
  char url[URL_BUF_SIZE];
  snprintf (url, URL_BUF_SIZE - 1, "%s%s", vault->baseurl, path);

  iot_data_t *reply = vault_rest_get (vault, url);
  if (reply)
  {
    d = iot_data_string_map_get (reply, "data");
  }
  if (d)
  {
    result = iot_data_copy (d);
  }
  else
  {
    iot_log_error (vault->lc, "vault: get secrets request failed");
    result = iot_data_alloc_map (IOT_DATA_STRING);
  }
  iot_data_free (reply);
  return result;
}

static void vault_set (void *impl, const char *path, const iot_data_t *secrets)
{
  edgex_ctx ctx;
  devsdk_error err = EDGEX_OK;
  char url[URL_BUF_SIZE];
  vault_impl_t *vault = (vault_impl_t *)impl;

  vault_initctx (vault, &ctx);
  snprintf (url, URL_BUF_SIZE - 1, "%s%s", vault->baseurl, path);

  char *json = iot_data_to_json (secrets);
  edgex_http_put (vault->lc, &ctx, url, json, edgex_http_write_cb, &err);
  free (json);

  if (err.code)
  {
    iot_log_error (vault->lc, "vault:error setting secrets: %s", ctx.buff);
  }
  vault_freectx (&ctx);
}

static void vault_fetchregtoken (vault_impl_t *vault)
{
  iot_data_t *reply = vault_rest_get (vault, vault->regurl);
  if (reply)
  {
    const iot_data_t *d = iot_data_string_map_get (reply, "data");
    if (d)
    {
      const iot_data_t *t = iot_data_string_map_get (d, "token");
      if (t)
      {
        vault->regtoken = devsdk_nvpairs_new ("X-Consul-Token", iot_data_string (t), NULL);
      }
    }
    iot_data_free (reply);
  }
  if (vault->regtoken == NULL)
  {
    iot_log_error (vault->lc, "vault: no consul token found");
  }
}

static void vault_getregtoken (void *impl, edgex_ctx *ctx)
{
  vault_impl_t *vault = (vault_impl_t *)impl;
  pthread_mutex_lock (&vault->mtx);
  if (!vault->regtoken)
  {
    vault_fetchregtoken (vault);
  }
  ctx->reqhdrs = vault->regtoken;
}

static void vault_releaseregtoken (void *impl)
{
  vault_impl_t *vault = (vault_impl_t *)impl;
  pthread_mutex_unlock (&vault->mtx);
}

static void vault_fini (void *impl)
{
  vault_impl_t *vault = (vault_impl_t *)impl;
  pthread_mutex_destroy (&vault->mtx);
  free (vault->baseurl);
  free (vault->regurl);
  free (vault->tokinfourl);
  free (vault->tokrenewurl);
  free (vault->token);
  free (vault->capath);
  devsdk_nvpairs_free (vault->regtoken);
  free (impl);
}

void *edgex_secrets_vault_alloc ()
{
  vault_impl_t *vault = calloc (1, sizeof (vault_impl_t));
  pthread_mutex_init (&vault->mtx, NULL);
  return vault;
}

const edgex_secret_impls edgex_secrets_vault_fns = { vault_init, vault_reconfigure, vault_get, vault_set, vault_getregtoken, vault_releaseregtoken, vault_fini };
