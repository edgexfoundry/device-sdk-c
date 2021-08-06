/*
 * Copyright (c) 2021
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "secrets.h"
#include "secrets-insecure.h"
#include "secrets-vault.h"
#include "edgex2.h"
#include "edgex-rest.h"
#include "service.h"
#include <microhttpd.h>

typedef struct edgex_secret_provider_t
{
  void *impl;
  edgex_secret_impls fns;
} edgex_secret_provider_t;

bool edgex_secrets_init (edgex_secret_provider_t *sp, iot_logger_t *lc, iot_data_t *config)
{
  return sp->fns.init (sp->impl, lc, config);
}

void edgex_secrets_reconfigure (edgex_secret_provider_t *sp, iot_data_t *config)
{
  sp->fns.reconfigure (sp->impl, config);
}

iot_data_t *edgex_secrets_get (edgex_secret_provider_t *sp, const char *path)
{
  return sp->fns.get (sp->impl, path);
}

void edgex_secrets_set (edgex_secret_provider_t *sp, const char *path, const devsdk_nvpairs *secrets)
{
  sp->fns.set (sp->impl, path, secrets);
}

void edgex_secrets_fini (edgex_secret_provider_t *sp)
{
  sp->fns.fini (sp->impl);
  free (sp);
}

edgex_secret_provider_t *edgex_secrets_get_insecure ()
{
  edgex_secret_provider_t *res = malloc (sizeof (edgex_secret_provider_t));
  res->impl = edgex_secrets_insecure_alloc ();
  res->fns = edgex_secrets_insecure_fns;
  return res;
}

edgex_secret_provider_t *edgex_secrets_get_vault ()
{
  edgex_secret_provider_t *res = malloc (sizeof (edgex_secret_provider_t));
  res->impl = edgex_secrets_vault_alloc ();
  res->fns = edgex_secrets_vault_fns;
  return res;
}

void edgex_device_handler_secret (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;
  JSON_Value *val = json_parse_string (req->data.bytes);
  JSON_Object *obj = json_value_get_object (val);

  if (obj)
  {
    devsdk_nvpairs *secrets = NULL;
    const char *path = json_object_get_string (obj, "path");
    JSON_Array *arr = json_object_get_array (obj, "secretData");
    size_t count = json_array_get_count (arr);
    for (size_t i = 0; i < count; i++)
    {
      JSON_Object *entry = json_array_get_object (arr, i);
      secrets = devsdk_nvpairs_new (json_object_get_string (entry, "key"), json_object_get_string (entry, "value"), secrets);
    }
    edgex_secrets_set (svc->secretstore, path, secrets);
    devsdk_nvpairs_free (secrets);
  }

  json_value_free (val);

  edgex_baseresponse br;
  edgex_baseresponse_populate (&br, "v2", MHD_HTTP_CREATED, "Secrets populated successfully");
  edgex_baseresponse_write (&br, reply);
}
