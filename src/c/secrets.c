/*
 * Copyright (c) 2021-2022
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
#include "iot/iot.h"
#include <microhttpd.h>

typedef struct edgex_secret_provider_t
{
  void *impl;
  edgex_secret_impls fns;
} edgex_secret_provider_t;

static uint64_t edgex_secrets_from_file (edgex_secret_provider_t *sp, const char *filename, bool scrub);

bool edgex_secrets_init
  (edgex_secret_provider_t *sp, iot_logger_t *lc, iot_scheduler_t *sched, iot_threadpool_t *pool, const char *svcname, iot_data_t *config, devsdk_metrics_t *m)
{
  bool result = sp->fns.init (sp->impl, lc, sched, pool, svcname, config);
  if (result)
  {
    const char *secfile = iot_data_string_map_get_string (config, "SecretStore/SecretsFile");
    if (strlen (secfile))
    {
      uint64_t count = edgex_secrets_from_file (sp, secfile, !iot_data_string_map_get_bool (config, "SecretStore/DisableScrubSecretsFile", false));
      atomic_fetch_add (&m->secsto, count);
    }
  }
  return result;
}

void edgex_secrets_reconfigure (edgex_secret_provider_t *sp, iot_data_t *config)
{
  sp->fns.reconfigure (sp->impl, config);
}

iot_data_t *edgex_secrets_get (edgex_secret_provider_t *sp, const char *path)
{
  return sp->fns.get (sp->impl, path);
}

static void edgex_secrets_set (edgex_secret_provider_t *sp, const char *path, const iot_data_t *secrets)
{
  sp->fns.set (sp->impl, path, secrets);
}

void edgex_secrets_getregtoken (edgex_secret_provider_t *sp, edgex_ctx *ctx)
{
  return sp->fns.getregtoken (sp->impl, ctx);
}

void edgex_secrets_releaseregtoken (edgex_secret_provider_t *sp)
{
  return sp->fns.releaseregtoken (sp->impl);
}

void edgex_secrets_fini (edgex_secret_provider_t *sp)
{
  if (sp)
  {
    sp->fns.fini (sp->impl);
    free (sp);
  }
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

static iot_data_t *edgex_secrets_process_secretdata (const iot_data_t *sd)
{
  iot_data_t *secrets = iot_data_alloc_map (IOT_DATA_STRING);
  if (sd && iot_data_type (sd) == IOT_DATA_VECTOR)
  {
    iot_data_vector_iter_t iter;
    iot_data_vector_iter (sd, &iter);
    while (iot_data_vector_iter_next (&iter))
    {
      const iot_data_t *entry = iot_data_vector_iter_value (&iter);
      const iot_data_t *k = iot_data_string_map_get (entry, "key");
      const iot_data_t *v = iot_data_string_map_get (entry, "value");
      if (k && v)
      {
        iot_data_map_add (secrets, iot_data_add_ref (k), iot_data_add_ref (v));
      }
    }
  }
  return secrets;
}

void edgex_device_handler_secret (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *) ctx;
  edgex_baseresponse br;
  iot_data_t *data = iot_data_from_json (req->data.bytes);

  if (data)
  {
    iot_data_t *secrets = edgex_secrets_process_secretdata (iot_data_string_map_get (data, "secretData"));
    edgex_secrets_set (svc->secretstore, iot_data_string_map_get_string (data, "path"), secrets);
    atomic_fetch_add (&svc->metrics.secsto, 1);
    iot_data_free (secrets);
    iot_data_free (data);
    edgex_baseresponse_populate (&br, "v2", MHD_HTTP_CREATED, "Secrets populated successfully");
  }
  else
  {
    edgex_baseresponse_populate (&br, "v2", MHD_HTTP_BAD_REQUEST, "Unable to parse secrets");
  }

  edgex_baseresponse_write (&br, reply);
}

static uint64_t edgex_secrets_from_file (edgex_secret_provider_t *sp, const char *filename, bool scrub)
{
  uint64_t result = 0;
  char *json = iot_file_read (filename);
  iot_data_t *src = iot_data_from_json (json);
  if (iot_data_type (src) == IOT_DATA_MAP)
  {
    const iot_data_t *s = iot_data_string_map_get (src, "secrets");
    if (s && iot_data_type (s) == IOT_DATA_VECTOR)
    {
      iot_data_vector_iter_t iter;
      iot_data_vector_iter (s, &iter);
      while  (iot_data_vector_iter_next (&iter))
      {
        const iot_data_t *element = iot_data_vector_iter_value (&iter);
        if (iot_data_string_map_get_bool (element, "imported", false))
        {
          continue;
        }
        iot_data_t *secrets = edgex_secrets_process_secretdata (iot_data_string_map_get (element, "secretData"));
        edgex_secrets_set (sp, iot_data_string_map_get_string (element, "path"), secrets);
        result++;
        iot_data_free (secrets);
        iot_data_string_map_add ((iot_data_t *)element, "imported", iot_data_alloc_bool (true));
        if (scrub)
        {
          iot_data_string_map_add ((iot_data_t *)element, "secretData", iot_data_alloc_vector (0));
        }
      }
    }
    free (json);
    json = iot_data_to_json (src);
    iot_file_write (filename, json);
  }
  iot_data_free (src);
  free (json);
  return result;
}
