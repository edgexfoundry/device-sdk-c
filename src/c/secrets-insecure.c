/*
 * Copyright (c) 2021-2023
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "secrets-insecure.h"
#include "rest.h"

#define SEC_PREFIX "Writable/InsecureSecrets/"
#define SEC_PREFIXLEN (sizeof (SEC_PREFIX) - 1)

#define SEC_PATH "/Secrets/"

typedef struct insecure_impl_t
{
  iot_logger_t *lc;
  iot_data_t *map;
  devsdk_metrics_t *metrics;
  pthread_mutex_t mutex;
} insecure_impl_t;

static iot_data_t *insecure_parse_config (iot_data_t *config)
{
  iot_data_t *result = iot_data_alloc_map (IOT_DATA_STRING);

  iot_data_map_iter_t iter;
  iot_data_map_iter (config, &iter);
  while (iot_data_map_iter_next (&iter))
  {
    const char *key = iot_data_map_iter_string_key (&iter);
    if (strncmp (key, SEC_PREFIX, SEC_PREFIXLEN) == 0)
    {
      size_t l = strlen (key);
      if (strcmp (key + l - 5, "/path") == 0)
      {
        iot_data_map_iter_t iter2;
        char *prefix = malloc (l + sizeof ("Secrets"));
        strcpy (prefix, key);
        strcpy (prefix + l -4, "Secrets/");
        iot_data_t *map = iot_data_alloc_map (IOT_DATA_STRING);
        iot_data_map_iter (config, &iter2);
        while (iot_data_map_iter_next (&iter2))
        {
          const char *key2 = iot_data_map_iter_string_key (&iter2);
          if (strncmp (key2, prefix, strlen (prefix)) == 0)
          {
            iot_data_map_add (map, iot_data_alloc_string (key2 + strlen (prefix), IOT_DATA_COPY), iot_data_copy (iot_data_map_iter_value (&iter2)));
          }
        }
        iot_data_map_add (result, iot_data_copy (iot_data_map_iter_value (&iter)), map);
        free (prefix);
      }
    }
  }
  return result;
}

static bool insecure_init
  (void *impl, iot_logger_t *lc, iot_scheduler_t *sched, iot_threadpool_t *pool, const char *svcname, iot_data_t *config, devsdk_metrics_t *m)
{
  insecure_impl_t *insec = (insecure_impl_t *)impl;
  insec->lc = lc;
  pthread_mutex_init (&insec->mutex, NULL);
  insec->map = insecure_parse_config (config);
  insec->metrics = m;
  return true;
}

static void insecure_reconfigure (void *impl, iot_data_t *config)
{
  insecure_impl_t *insec = (insecure_impl_t *)impl;
  pthread_mutex_lock (&insec->mutex);
  iot_data_free (insec->map);
  insec->map = insecure_parse_config (config);
  pthread_mutex_unlock (&insec->mutex);
}

static iot_data_t *insecure_get (void *impl, const char *path)
{
  iot_data_t *result;
  insecure_impl_t *insec = (insecure_impl_t *)impl;

  pthread_mutex_lock (&insec->mutex);
  const iot_data_t *secrets = iot_data_string_map_get (insec->map, path);
  result = secrets ? iot_data_copy (secrets) : iot_data_alloc_map (IOT_DATA_STRING);
  pthread_mutex_unlock (&insec->mutex);
  atomic_fetch_add (&insec->metrics->secrq, 1);
  return result;
}

static void insecure_set (void *impl, const char *path, const iot_data_t *secrets)
{
  insecure_impl_t *insec = (insecure_impl_t *)impl;
  iot_log_error (insec->lc, "Storing secrets is not supported when running in insecure mode");
}

static void insecure_getregtoken (void *impl, edgex_ctx *ctx)
{
}

static void insecure_releaseregtoken (void *impl)
{
}

static iot_data_t * insecure_requestjwt (void *impl)
{
  // "" will cause no Authorization header to be sent in rest.c
  return iot_data_alloc_string ("", IOT_DATA_REF); 
}

static bool insecure_isjwtvalid (void *impl, const char *jwt)
{
  return true;
}


static void insecure_fini (void *impl)
{
  insecure_impl_t *insec = (insecure_impl_t *)impl;
  iot_data_free (insec->map);
  pthread_mutex_destroy (&insec->mutex);
  free (impl);
}

void *edgex_secrets_insecure_alloc ()
{
  return calloc (1, sizeof (insecure_impl_t));
}

const edgex_secret_impls edgex_secrets_insecure_fns = { insecure_init, insecure_reconfigure, insecure_get, insecure_set, insecure_getregtoken, insecure_releaseregtoken, insecure_requestjwt, insecure_isjwtvalid, insecure_fini };
