/*
 * Copyright (c) 2021
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

typedef struct vault_impl_t
{
  iot_logger_t *lc;
  char *token;
  char *baseurl;
  char *capath;
  bool bearer;
} vault_impl_t;

static bool vault_init (void *impl, iot_logger_t *lc, iot_data_t *config)
{
  vault_impl_t *vault = (vault_impl_t *)impl;
  vault->lc = lc;

  const char *path = iot_data_string_map_get_string (config, "SecretStore/Path");
  vault->baseurl = malloc (URL_BUF_SIZE);
  snprintf
  (
    vault->baseurl, URL_BUF_SIZE,
    "%s://%s:%u/v1/secret/edgex%s%s%s",
    iot_data_string_map_get_string (config, "SecretStore/Protocol"),
    iot_data_string_map_get_string (config, "SecretStore/Host"),
    iot_data_ui16 (iot_data_string_map_get (config, "SecretStore/Port")),
    path[0] == '/' ? "" : "/",
    path,
    path[strlen (path) - 1] == '/' ? "" : "/"
  );

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
  return true;
}

static void vault_reconfigure (void *impl, iot_data_t *config)
{
}

static iot_data_t *vault_get (void *impl, const char *path)
{
  edgex_ctx ctx;
  devsdk_error err = EDGEX_OK;
  char url[URL_BUF_SIZE];
  vault_impl_t *vault = (vault_impl_t *)impl;
  iot_data_t *result = iot_data_alloc_map (IOT_DATA_STRING);

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf (url, URL_BUF_SIZE - 1, "%s%s", vault->baseurl, path);
  if (vault->capath)
  {
    ctx.cacerts_path = vault->capath;
    ctx.verify_peer = 1;
  }
  if (vault->bearer)
  {
    ctx.jwt_token = vault->token;
  }
  else
  {
    ctx.reqhdrs = devsdk_nvpairs_new ("X-Vault-Token", vault->token, NULL);
  }
  edgex_http_get (vault->lc, &ctx, url, edgex_http_write_cb, &err);
  if (err.code == 0)
  {
    JSON_Value *jval = json_parse_string (ctx.buff);
    JSON_Object *jobj = json_value_get_object (jval);
    JSON_Object *data = json_object_get_object (jobj, "data");
    size_t count = json_object_get_count (data);
    for (size_t i = 0; i < count; i++)
    {
      iot_data_map_add (result, iot_data_alloc_string (json_object_get_name (data, i), IOT_DATA_COPY), iot_data_alloc_string (json_string (json_object_get_value_at (data, i)), IOT_DATA_COPY));
    }

    json_value_free (jval);
  }
  else
  {
    iot_log_error (vault->lc, "vault: get secrets request failed");
  }
  devsdk_nvpairs_free (ctx.reqhdrs);
  free (ctx.buff);

  return result;
}

static void vault_set (void *impl, const char *path, const iot_data_t *secrets)
{
  edgex_ctx ctx;
  devsdk_error err = EDGEX_OK;
  char url[URL_BUF_SIZE];
  vault_impl_t *vault = (vault_impl_t *)impl;

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf (url, URL_BUF_SIZE - 1, "%s%s", vault->baseurl, path);
  ctx.jwt_token = vault->token;

  char *json = iot_data_to_json (secrets);

  edgex_http_put (vault->lc, &ctx, url, json, edgex_http_write_cb, &err);
  free (json);

  if (err.code)
  {
    iot_log_error (vault->lc, "vault:error setting secrets: %s", ctx.buff);
  }
  free (ctx.buff);
}

static void vault_fini (void *impl)
{
  vault_impl_t *vault = (vault_impl_t *)impl;
  free (vault->baseurl);
  free (vault->token);
  free (vault->capath);
  free (impl);
}

void *edgex_secrets_vault_alloc ()
{
  return calloc (1, sizeof (vault_impl_t));
}

const edgex_secret_impls edgex_secrets_vault_fns = { vault_init, vault_reconfigure, vault_get, vault_set, vault_fini };
