/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "consul.h"
#include "edgex_rest.h"
#include "rest.h"
#include "errorlist.h"
#include "config.h"
#include "parson.h"
#include "base64.h"

#define URL_BUF_SIZE 512

static size_t write_cb (void *contents, size_t size, size_t nmemb, void *userp)
{
  edgex_ctx *ctx = (edgex_ctx *) userp;
  size *= nmemb;
  ctx->buff = realloc (ctx->buff, ctx->size + size + 1);
  memcpy (&(ctx->buff[ctx->size]), contents, size);
  ctx->size += size;
  ctx->buff[ctx->size] = 0;

  return size;
}

static char *keyvalue_read (const char *json)
{
  char *result = NULL;
  JSON_Value *val = json_parse_string (json);
  JSON_Array *aval = json_value_get_array (val);
  JSON_Object *obj = json_array_get_object (aval, 0);

  if (obj)
  {
    const char *enc = json_object_get_string (obj, "Value");
    if (enc)
    {
      size_t rsize = edgex_b64_maxdecodesize (enc);
      result = malloc (rsize + 1);
      if (edgex_b64_decode (enc, result, &rsize))
      {
        result[rsize] = '\0';
      }
      else
      {
        free (result);
        result = NULL;
      }
    }
  }

  json_value_free (val);

  return result;
}

static size_t keylist_read (const char *json, char ***results)
{
  size_t nkeys = 0;

  JSON_Value *val = json_parse_string (json);
  JSON_Array *keys = json_value_get_array (val);
  if (keys)
  {
    nkeys = json_array_get_count (keys);
    *results = malloc (nkeys * sizeof (char *));
    for (size_t i = 0; i < nkeys; i++)
    {
      (*results)[i] = strdup (json_array_get_string (keys, i));
    }
  }
  json_value_free (val);
  return nkeys;
}

const char *edgex_consul_client_get_value
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const char *servicename,
  const char *config,
  const char *key,
  edgex_error *err
)
{
  const char *result;
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  if (config)
  {
    snprintf
    (
      url, URL_BUF_SIZE - 1, "http://%s:%u/v1/kv/config/%s;%s/%s",
      endpoints->consul.host, (uint16_t) endpoints->consul.port,
      servicename, config, key
    );
  }
  else
  {
    snprintf
    (
      url, URL_BUF_SIZE - 1, "http://%s:%u/v1/kv/config/%s/%s",
      endpoints->consul.host, (uint16_t) endpoints->consul.port,
      servicename, key
    );
  }
  edgex_http_get (lc, &ctx, url, write_cb, err);

  if (err->code)
  {
    return 0;
  }

  result = keyvalue_read (ctx.buff);
  free (ctx.buff);
  *err = EDGEX_OK;
  return result;
}

int edgex_consul_client_get_keys
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const char *servicename,
  const char *config,
  const char *base,
  char ***results,
  edgex_error *err
)
{
  int nkeys = 0;
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  if (config)
  {
    snprintf
    (
      url, URL_BUF_SIZE - 1, "http://%s:%u/v1/kv/config/%s;%s/%s?keys",
      endpoints->consul.host, (uint16_t) endpoints->consul.port,
      servicename, config, base
    );
  }
  else
  {
    snprintf
    (
      url, URL_BUF_SIZE - 1, "http://%s:%u/v1/kv/config/%s/%s?keys",
      endpoints->consul.host, (uint16_t) endpoints->consul.port,
      servicename, base
    );
  }
  edgex_http_get (lc, &ctx, url, write_cb, err);

  if (err->code)
  {
    return 0;
  }

  nkeys = keylist_read (ctx.buff, results);
  free (ctx.buff);
  *err = EDGEX_OK;
  return nkeys;
}
