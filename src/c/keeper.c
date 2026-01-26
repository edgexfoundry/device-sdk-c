#define _COMMON_CONFIG_KEY_ROOT "edgex/v4/core-common-config-bootstrapper/"
#define _COMMON_CONFIG_TOPIC_ROOT KEEPER_PUBLISH_PREFIX "edgex/v4/core-common-config-bootstrapper/"

#include "keeper.h"
#include "service.h"
#include "errorlist.h"
#include "bus.h"
#include "devsdk/devsdk.h"
#include "iot/time.h"
#include "api.h"
#include "iot/base64.h"


/* Our "impl" structure for global state. */
typedef struct keeper_impl_t
{
    devsdk_service_t *service;
    iot_threadpool_t *pool;
    iot_logger_t *lc;
    char *host;
    uint16_t port;
    char *key_root;
    char *topic_root;
    devsdk_registry_updatefn private_config_updater;
    devsdk_registry_updatefn common_config_updater;
    void *updatectx;
} keeper_impl_t;

static int32_t edgex_keeper_client_notify(void *impl, const iot_data_t *request, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor);

void *devsdk_registry_keeper_alloc(devsdk_service_t *service)
{
    keeper_impl_t *rv;
    rv = calloc(sizeof(keeper_impl_t), 1);
    rv->service = service;
    return rv;
}

static void edgex_keeper_client_free (void *impl)
{
  keeper_impl_t *keeper = (keeper_impl_t *)impl;

  if (keeper)
  {
    free (keeper->host);
    free (keeper->key_root);
    free (keeper->topic_root);
    free (impl);
  }
}

static void *delayed_message_bus_connect(void *impl)
{
  keeper_impl_t *keeper = (keeper_impl_t *)impl;
  /* If the service stops before we connect, the 'impl' structure might be freed, 
     so stash a copy of 'stopconfig' so we know if we need to exit.
     'stopconfig' is allocated before our init, and freed after the iot_threadpool 
     is waited on, so this is safe.
  */
  atomic_bool *stopconfig = NULL;
  if (keeper && keeper->service && keeper->service->stopconfig)
  {
    stopconfig = keeper->service->stopconfig;
  }
  else if (keeper && keeper->lc)
  {
    iot_log_error (keeper->lc, "Internal error: Keeper delayed bus connect called too early, we will not listen for config changes");
    return NULL;
  }
  else
  {
    return NULL;
  }
  
  iot_log_info (keeper->lc, "Keeper message bus wait thread starting");
  while (!(*stopconfig))
  {
    if (keeper->service && keeper->service->msgbus)
    {
      char *tree = malloc (strlen (keeper->topic_root) + 3);
      strcpy (tree, keeper->topic_root);
      if (tree [strlen (tree) - 1] == '/')
      {
        tree [strlen (tree) - 1] = '\0';
      }
      strcat (tree, "/#");
      iot_log_info (keeper->lc, "Subscribing to Keeper config changes on topic %s", tree);
      edgex_bus_register_handler (keeper->service->msgbus, tree, impl, edgex_keeper_client_notify);
      edgex_bus_register_handler (keeper->service->msgbus, _COMMON_CONFIG_TOPIC_ROOT ALL_SVCS_NODE "/#", impl, edgex_keeper_client_notify);
      edgex_bus_register_handler (keeper->service->msgbus, _COMMON_CONFIG_TOPIC_ROOT DEV_SVCS_NODE "/#", impl, edgex_keeper_client_notify);
      free(tree);
      break;
    }
    sleep (1);
  }
  return NULL;
}

static bool edgex_keeper_client_init (void *impl, iot_logger_t *logger, iot_threadpool_t *pool, edgex_secret_provider_t *sp, const char *url)
{
    /* Secret provider not used, only present for function prototype compatibility */
    (void) sp;
    if ((!impl) || (!logger) || (!url) || (!pool))
    {
        return false;
    }
    keeper_impl_t *keeper = (keeper_impl_t *)(impl);
    keeper->lc = logger;
    if (!keeper->service)
    {
        iot_log_error(logger, "Internal error: devsdk_service_t not set at Keeper init");
        return false;
    }
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
                keeper->port = port;
                keeper->host = strndup (pos, colon - pos);
            }
        }
        else
        {
            iot_log_error (logger, "Unable to parse \"%s\" for port number for registry", colon + 1);
            return false;
        }
    }
    else
    {
        iot_log_error(logger, "Could not parse URL \"%s\" as a core-keeper config/registry URL", url);
        return false;
    }

    keeper->key_root = calloc(URL_BUF_SIZE, 1);
    snprintf(keeper->key_root, URL_BUF_SIZE-1, "edgex/v4/%s", keeper->service->name);
    keeper->key_root[URL_BUF_SIZE-1] = '\0';
    keeper->topic_root = calloc(URL_BUF_SIZE, 1);
    snprintf(keeper->topic_root, URL_BUF_SIZE-1, KEEPER_PUBLISH_PREFIX "%s", keeper->key_root);
    keeper->topic_root[URL_BUF_SIZE-1] = '\0';

    // Can't yet subscribe to the message bus because it's not set up yet, because we
    // don't have its config yet, because we might be reading config from Keeper.
    // So start a background thread to wait until the message bus is available,
    // then subscribe for notification of changes.
    iot_threadpool_add_work(pool, delayed_message_bus_connect, keeper, -1);

    return true;
}

static bool edgex_keeper_client_ping (void *impl)
{
  keeper_impl_t *keeper = (keeper_impl_t *)impl;
  devsdk_error err = EDGEX_OK;
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v3/ping",
    keeper->host,
    keeper->port
  );

  edgex_http_get (keeper->lc, &ctx, url, NULL, &err);
  return (err.code == 0);
}

static devsdk_nvpairs *edgex_keeper_get_tree(void *impl, const char *keyroot, devsdk_error *err)
{
  keeper_impl_t *keeper = (keeper_impl_t *)impl;
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  devsdk_nvpairs *result = NULL;

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/v3/kvs/key/%s?plaintext=true&keyOnly=false", keeper->host, keeper->port, keyroot);
  url[URL_BUF_SIZE-1] = '\0';

  iot_data_t *jwt_data = edgex_secrets_request_jwt (keeper->service->secretstore);
  ctx.jwt_token = iot_data_string(jwt_data);

  edgex_http_get (keeper->lc, &ctx, url, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  if (err->code == 0)
  {
    // Unlike the event, this will give us a list of single keys and their values.
    JSON_Value *resp_val = json_parse_string(ctx.buff);
    iot_log_trace(keeper->lc, "Got response from Keeper for key %s", keyroot);
    char *err_msg = NULL;
    if (resp_val)
    {
        JSON_Object *resp_obj = json_value_get_object(resp_val);
        if (resp_obj)
        {
            const char *apiVer = json_object_get_string(resp_obj, "apiVersion");
            if ((!apiVer) || (strcmp(apiVer, "v3")!=0))
            {
                iot_log_warn(keeper->lc, "Keeper response apiVersion (%s) missing or wrong", apiVer ? apiVer : "(null)");
            }
            uint64_t code = json_object_get_uint(resp_obj, "statusCode");
            if (code == 200)
            {
                JSON_Array *kv_array = json_object_get_array(resp_obj, "response");
                if (kv_array)
                {
                    size_t i;
                    for (i=0; i<json_array_get_count(kv_array); i++)
                    {
                        JSON_Object *this_obj = json_value_get_object(json_array_get_value(kv_array, i));
                        if (this_obj)
                        {
                            const char *this_key = json_object_get_string(this_obj, "key");
                            const char *this_val = json_object_get_string(this_obj, "value");
                            if (this_key && this_val)
                            {
                                char *prefix = strstr(this_key, keyroot);
                                if (prefix != this_key)
                                {
                                    iot_log_warn(keeper->lc, "Received key %s does not begin with our prefix %s, ignoring", this_key, keyroot);
                                }
                                else
                                {
                                    const char *key_suffix = this_key + strlen(keyroot);
                                    if (*key_suffix == '/')
                                    {
                                      key_suffix++;
                                    }
                                    iot_log_trace(keeper->lc, "Got key %s = value %s", key_suffix, this_val);
                                    result = devsdk_nvpairs_new(key_suffix, this_val, result);
                                }
                            }
                            else
                            {
                                err_msg = "'key' or 'value' member not found in object";
                                break;
                            }
                        }
                        else
                        {
                            err_msg = "An element of 'response' is not a JSON object";
                            break;
                        }
                    } /* End loop over responses */
                }
                else
                {
                    err_msg = "'response' missing or not an array";
                }
            }
            else
            {
                err_msg = "'statusCode is missing or not 200";
            }
        }
        else
        {
            err_msg = "Response was valid JSON but not a JSON object";
        }
        json_value_free(resp_val);
    }
    else
    {
        err_msg = "Response could not be parsed as JSON";
    }
    if (err_msg)
    {
        iot_log_error(keeper->lc, "Error processing response(%s): %s", ctx.buff ? ctx.buff : "(null)", err_msg);
        *err = EDGEX_REGISTRY_RESPONSE;
    }
  }
  else
  {
    iot_log_info(keeper->lc, "Error (%d) response from Keeper, it probably does not have our config", err->code);
  }
  free (ctx.buff);

  return result;
}

static devsdk_nvpairs *edgex_keeper_client_get_config
(
  void *impl,
  const char *servicename,
  devsdk_registry_updatefn updater,
  void *updatectx,
  atomic_bool *updatedone,
  devsdk_error *err
)
{
  keeper_impl_t *keeper = (keeper_impl_t *)impl;
  /* updatedone not used, only kept for prototype compatibility */
  (void) updatedone;
  keeper->private_config_updater = updater;
  keeper->updatectx = updatectx;
  return edgex_keeper_get_tree(impl, keeper->key_root, err);
}

static devsdk_nvpairs *edgex_keeper_client_get_common_config
(
  void *impl,
  devsdk_registry_updatefn updater,
  void *updatectx,
  atomic_bool *updatedone,
  devsdk_error *err,
  const devsdk_timeout *timeout
)
{
  keeper_impl_t *keeper = (keeper_impl_t *)impl;
  devsdk_nvpairs *result = NULL;
  devsdk_nvpairs *ccReady = NULL;
  /* updatedone not used, only kept for prototype compatibility */
  (void) updatedone;
  keeper->common_config_updater = updater;
  keeper->updatectx = updatectx;

  uint64_t t1, t2;
  while (true)
  {
    t1 = iot_time_msecs ();
    *err = EDGEX_OK;
    ccReady = edgex_keeper_get_tree(impl, "edgex/v4/core-common-config-bootstrapper", err);
    if (err->code == 0)
    {
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
      iot_log_warn(keeper->lc, "waiting for Common Configuration to be available from config provider.");
      iot_wait_msecs (timeout->interval - (t2 - t1));
    }
    devsdk_nvpairs_free (ccReady);
  }

  result = edgex_keeper_get_tree(impl, "edgex/v4/core-common-config-bootstrapper/all-services", err);
  if (err->code)
  {
    devsdk_nvpairs_free (result);
    result = NULL;
  }
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

  devsdk_nvpairs *privateConfig = NULL;
  privateConfig = edgex_keeper_get_tree(impl, "edgex/v4/core-common-config-bootstrapper/device-services", err);

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

  return result;
}

static void process_notification(keeper_impl_t *keeper, const iot_data_t *request, const char *key, const char *key_root, devsdk_registry_updatefn updater)
{
  const char *key_suffix = key + strlen (key_root);
  if (*key_suffix == '/')
  {
    key_suffix++;
  }
  const char *str_val = iot_data_string_map_get_string (request, "value");
  if (!str_val)
  {
    iot_log_warn (keeper->lc, "Notified of change but object missing 'value' member");
    return;
  }
  iot_log_info (keeper->lc, "Notified of config change at key '%s' to value '%s'", key_suffix, str_val);
  const char *prefix_all_svcs = "all-services/";
  const char *prefix_dev_svcs = "device-services/";
  if (strncmp (key_suffix, prefix_all_svcs, strlen (prefix_all_svcs)) == 0)
  {
    key_suffix += strlen(prefix_all_svcs);
  }
  else if (strncmp(key_suffix, prefix_dev_svcs, strlen (prefix_dev_svcs)) == 0)
  {
    key_suffix += strlen (prefix_dev_svcs);
  }
  devsdk_nvpairs *result = devsdk_nvpairs_new (key_suffix, str_val, NULL);
  if (result)
  {
    updater (keeper->updatectx, result);
    devsdk_nvpairs_free (result);
  }
}

static int32_t edgex_keeper_client_notify(void *impl, const iot_data_t *request, const iot_data_t *pathparams, const iot_data_t *params, iot_data_t **reply, bool *event_is_cbor)
{
  keeper_impl_t *keeper = (keeper_impl_t *)impl;
  if ((!keeper) || (!request) || (iot_data_type(request) != IOT_DATA_MAP))
  {
    iot_log_warn (keeper->lc, "Received notification from Keeper but request is not a map, ignoring");
    return 0;
  }
  if (!keeper->private_config_updater || !keeper->common_config_updater)
  {
    iot_log_info (keeper->lc, "Notified of config change but this service has not registered for these, ignoring");
    return 0;
  }
  const iot_data_t *raw_key = iot_data_string_map_get (request, "key");
  if (!raw_key)
  {
    iot_log_warn (keeper->lc, "Notified of change but object missing 'key' member");
    return 0;
  }
  const char *key = iot_data_string(raw_key);
  if (strstr (key, keeper->key_root) == key)
  {
    process_notification (keeper, request, key, keeper->key_root, keeper->private_config_updater);
  }
  else if (strstr (key, _COMMON_CONFIG_KEY_ROOT) == key)
  {
    process_notification (keeper, request, key, _COMMON_CONFIG_KEY_ROOT, keeper->common_config_updater);
  }
  else
  {
    iot_log_warn (keeper->lc, "Received key %s does not begin with our prefix %s or common config prefix %s, ignoring",
                  key, keeper->key_root, _COMMON_CONFIG_KEY_ROOT);
  }
  return 0;
}

static JSON_Value *cfg_item_to_json(const iot_data_t *item)
{
  if (!item)
  {
    return NULL;
  }
  switch (iot_data_type(item))
  {
    case IOT_DATA_INT8:
      return json_value_init_number((double)iot_data_i8(item));
    case IOT_DATA_UINT8:
      return json_value_init_uint((uint64_t)iot_data_ui8(item));
    case IOT_DATA_INT16:
      return json_value_init_number((double)iot_data_i16(item));
    case IOT_DATA_UINT16:
      return json_value_init_uint((uint64_t)iot_data_ui16(item));
    case IOT_DATA_INT32:
      return json_value_init_number((double)iot_data_i32(item));
    case IOT_DATA_UINT32:
      return json_value_init_uint((uint64_t)iot_data_ui32(item));
    case IOT_DATA_INT64:
      return json_value_init_number((double)iot_data_i64(item));
    case IOT_DATA_UINT64:
      return json_value_init_uint(iot_data_ui64(item));
    case IOT_DATA_FLOAT32:
      return json_value_init_number((double)iot_data_f32(item));
    case IOT_DATA_FLOAT64:
      return json_value_init_number(iot_data_f64(item));
    case IOT_DATA_BOOL:
      return json_value_init_boolean(iot_data_bool(item));
    case IOT_DATA_STRING:
      return json_value_init_string(iot_data_string(item));
    case IOT_DATA_ARRAY:
    case IOT_DATA_MAP:
    case IOT_DATA_VECTOR:
    default:
      /* Do not expect to see these in config map */
      return NULL;
  }
}

static void edgex_keeper_client_write_config (void *impl, const char *servicename, const iot_data_t *config, devsdk_error *err)
{
  keeper_impl_t *keeper = (keeper_impl_t *)impl;
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  if ((!keeper) || (!servicename))
  {
    return;
  }

  if ( (config == NULL) || ( iot_data_type(config) != IOT_DATA_MAP ) )
  {
    iot_log_error(keeper->lc, "edgex_keeper_client_write_config : %s error avoid_assert", (config ? "iot_data_t type" : "null conf" ));
    return;
  }

  /* The map comes to us flat, in the form: {"Device/UseMessageBus": true, "Device/ProfilesDir": "res/profiles"} etc.
     We use Parson's dotset functions to convert this into a hierarchical object, which we can PUT to Keeper
     in one go with flatten=true.
     Keys that contain dots get written separately, one at a time.
  */
  devsdk_nvpairs *dotted_keys = NULL;
  iot_data_map_iter_t iter;
  JSON_Value *flat_conf = json_value_init_object();
  JSON_Object *flat_conf_obj = json_value_get_object(flat_conf);
  iot_data_map_iter (config, &iter);
  while (iot_data_map_iter_next (&iter))
  {
    const char *k = iot_data_map_iter_string_key (&iter);
    char *value_string = iot_data_to_json (config); // freed by json_value_free()
    if (k && value_string)
    {
      if (strchr(k, '.'))
      {
        dotted_keys = devsdk_nvpairs_new(k, iot_data_to_json(config), dotted_keys);
      }
      else
      {
        char *dotkey = strdup(k);
        char *this_slash;
        while ((this_slash = strchr(dotkey, '/')) != NULL)
        {
          *this_slash = '.';
        }
        JSON_Value *this_value = cfg_item_to_json(iot_data_map_iter_value(&iter));
        if ((!this_value) || json_object_dotset_value(flat_conf_obj, dotkey, this_value))
        {
          iot_log_error(keeper->lc, "Could not add key %s to Keeper", k);
        }
        free(dotkey);
      }
    }
  }

  JSON_Value *put_request = json_value_init_object();
  JSON_Object *put_request_obj = json_value_get_object(put_request);
  json_object_set_value(put_request_obj, "value", flat_conf);
  char *put_request_string = json_serialize_to_string(put_request);
  json_value_free(put_request); // also frees flat_conf since that is now part of put_request
  snprintf(url, URL_BUF_SIZE - 1, "http://%s:%u/api/v3/kvs/key/%s?flatten=true",
    keeper->host, keeper->port, keeper->key_root);
  url[URL_BUF_SIZE-1] = '\0';

  iot_log_trace(keeper->lc, "PUT '%s' to Keeper at key %s", put_request_string, keeper->key_root);
  memset (&ctx, 0, sizeof (edgex_ctx));
  devsdk_error e;

  iot_data_t *jwt_data = edgex_secrets_request_jwt (keeper->service->secretstore);
  ctx.jwt_token = iot_data_string(jwt_data);

  edgex_http_put (keeper->lc, &ctx, url, put_request_string, edgex_http_write_cb, &e);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  json_free_serialized_string (put_request_string);
  free (ctx.buff);
  if (err && (e.code != 0))
  {
      *err = e;
  }

  const char *format = "{\"value\":%s}";
  devsdk_nvpairs *this_pair = dotted_keys;
  while (this_pair)
  {
    if (this_pair->name && this_pair->value)
    {
      iot_log_trace(keeper->lc, "Posting key %s value %s individually to Keeper", this_pair->name, this_pair->value);
      char *post_url = malloc(URL_BUF_SIZE);
      size_t val_size = strlen(this_pair->value) + strlen(format); // technically a couple more but that's OK
      char *req = malloc(val_size+1);
      snprintf(post_url, URL_BUF_SIZE, "http://%s:%u/api/v3/kvs/key/%s/%s", keeper->host, keeper->port, keeper->key_root, this_pair->name);
      post_url[URL_BUF_SIZE-1] = '\0';
      snprintf(req, val_size, format, this_pair->value);
      req[val_size] = '\0';
      memset (&ctx, 0, sizeof (edgex_ctx));
      devsdk_error e;

      iot_data_t *jwt_data = edgex_secrets_request_jwt (keeper->service->secretstore);
      ctx.jwt_token = iot_data_string(jwt_data);

      edgex_http_put (keeper->lc, &ctx, post_url, req, edgex_http_write_cb, &e);

      iot_data_free(jwt_data);
      ctx.jwt_token = NULL;

      free(post_url);
      free(req);
      free (ctx.buff);
      if (err && (e.code != 0))
      {
        *err = e;
      }
    }
    this_pair = this_pair->next;
  }
  if (dotted_keys)
  {
    devsdk_nvpairs_free(dotted_keys);
  }
}

static void edgex_keeper_client_register_service
(
  void *impl,
  const char *servicename,
  const char *host,
  uint16_t port,
  const char *checkInterval,
  devsdk_error *err
)
{
  keeper_impl_t *keeper = (keeper_impl_t *)impl;
  edgex_ctx postput_ctx, get_ctx;
  char post_url[URL_BUF_SIZE];
  char get_url[URL_BUF_SIZE];
  bool exists = false;

  memset (&postput_ctx, 0, sizeof (edgex_ctx));
  memset (&get_ctx, 0, sizeof (edgex_ctx));

  iot_data_t *jwt_data = edgex_secrets_request_jwt (keeper->service->secretstore);
  const char *jwt_str = iot_data_string(jwt_data);
  postput_ctx.jwt_token = jwt_str;
  get_ctx.jwt_token = jwt_str;

  snprintf
  (
    post_url, URL_BUF_SIZE - 1, "http://%s:%u/api/v3/registry",
    keeper->host, keeper->port
  );
  post_url[URL_BUF_SIZE - 1] = '\0';
  snprintf
  (
    get_url, URL_BUF_SIZE - 1, "http://%s:%u/api/v3/registry/serviceId/%s",
    keeper->host, keeper->port, servicename
  );
  get_url[URL_BUF_SIZE - 1] = '\0';

  JSON_Value *top_params = json_value_init_object ();
  JSON_Object *top_obj = json_value_get_object (top_params);
  json_object_set_string (top_obj, "apiVersion", "v3");
  JSON_Value *reg_params = json_value_init_object();
  JSON_Object *reg_obj = json_value_get_object(reg_params);
  json_object_set_string(reg_obj, "serviceId", servicename);
  json_object_set_string(reg_obj, "host", host);
  json_object_set_uint(reg_obj, "port", port);
  JSON_Value *check_params = json_value_init_object();
  JSON_Object *check_obj = json_value_get_object(check_params);
  json_object_set_string(check_obj, "interval", checkInterval);
  json_object_set_string(check_obj, "type", "http");
  json_object_set_string(check_obj, "path", "/api/v3/ping");
  json_object_set_value(reg_obj, "healthCheck", check_params);
  json_object_set_value(top_obj, "registration", reg_params);
  char *json = json_serialize_to_string (top_params);
  json_value_free (top_params);

  /* Now we have the create or update request body.
   * Check to see if the registration already exists, and if so
   * use PUT to update it instead of POST to create it (that would return 409).
   * Don't call query_service() because we need to treat the "exists but invalid"
   * case the same as the "exists, valid" case.
   */

  long http_code = edgex_http_get (keeper->lc, &get_ctx, get_url, edgex_http_write_cb, err);
  if ((err->code == 0) && (http_code == 200))
  {
    exists = true;
  }
  *err = EDGEX_OK;  // Whether the above failed does not matter to our caller
  if (exists)
  {
    edgex_http_put (keeper->lc, &postput_ctx, post_url, json, edgex_http_write_cb, err);
  }
  else
  {
    edgex_http_post (keeper->lc, &postput_ctx, post_url, json, edgex_http_write_cb, err);
  }
  if (err->code)
  {
    iot_log_error (keeper->lc, "Register service failed: %s", postput_ctx.buff);
  }
  else
  {
    iot_log_info(keeper->lc, "Registered service %s at %s:%u to Keeper with check interval %s", servicename, host, port, checkInterval);
  }

  iot_data_free(jwt_data);
  get_ctx.jwt_token = NULL;
  postput_ctx.jwt_token = NULL;

  json_free_serialized_string (json);
  free (get_ctx.buff);
  free (postput_ctx.buff);
}

static void edgex_keeper_client_deregister_service
(
  void *impl,
  const char *servicename,
  devsdk_error *err
)
{
  keeper_impl_t *keeper = (keeper_impl_t *)impl;
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url, URL_BUF_SIZE - 1, "http://%s:%u/api/v3/registry/serviceId/%s",
    keeper->host, keeper->port, servicename
  );
  url[URL_BUF_SIZE-1] = '\0';

  iot_data_t *jwt_data = edgex_secrets_request_jwt (keeper->service->secretstore);
  ctx.jwt_token = iot_data_string(jwt_data);

  edgex_http_delete (keeper->lc, &ctx, url, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  if (err->code)
  {
    iot_log_error (keeper->lc, "Deregister service failed: %s", ctx.buff);
  }
  else
  {
    iot_log_info(keeper->lc, "Unregistered service %s from Keeper", servicename);
  }
  free (ctx.buff);
}

static void edgex_keeper_client_query_service
(
  void *impl,
  const char *servicename,
  char **host,
  uint16_t *port,
  devsdk_error *err
)
{
  keeper_impl_t *keeper = (keeper_impl_t *)impl;
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  if (!err)
  {
    return;
  }
  if ((!servicename) || (!host) || (!port))
  {
    *err = EDGEX_INVALID_ARG;
    return;
  }
  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url, URL_BUF_SIZE - 1,
    "http://%s:%u/api/v3/registry/serviceId/%s",
    keeper->host, keeper->port, servicename
  );
  url[URL_BUF_SIZE-1] = '\0';
  *err = EDGEX_OK;

  iot_data_t *jwt_data = edgex_secrets_request_jwt (keeper->service->secretstore);
  ctx.jwt_token = iot_data_string(jwt_data);

  long http_code = edgex_http_get (keeper->lc, &ctx, url, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  if (err->code == 0)
  {
    JSON_Value *val = json_parse_string (ctx.buff);
    if (val)
    {
        JSON_Object *obj = json_value_get_object(val);
        if (obj)
        {
            JSON_Object *reg_obj = json_object_get_object(obj, "registration");
            if (reg_obj)
            {
                *port = (uint16_t) json_object_get_uint(reg_obj, "port");
                const char *provided_host = json_object_get_string(reg_obj, "host");
                if ((provided_host) && (*port != 0))
                {
                    *host = strdup(provided_host);
                    iot_log_debug(keeper->lc, "Keeper Registry found service %s at %s:%u", servicename, *host, *port);
                }
                else
                {
                    iot_log_warn(keeper->lc, "Could not parse host or port from registry response");
                    *err = EDGEX_BAD_CONFIG;
                }
            }
            else
            {
                iot_log_warn(keeper->lc, "'registration' entry not found in registry response");
                *err = EDGEX_BAD_CONFIG;
            }
        }
        else
        {
            iot_log_warn(keeper->lc, "Registry response was not a JSON object");
            *err = EDGEX_BAD_CONFIG;
        }
        json_value_free(val);
    }
    else
    {
        iot_log_warn(keeper->lc, "Registry resposne was not valid JSON");
        *err = EDGEX_BAD_CONFIG;
    }
    free (ctx.buff);
  }
  else
  {
    if (http_code == 404)
    {
        iot_log_info(keeper->lc, "Registry entry for service %s not found", servicename);
        *err = EDGEX_BAD_CONFIG;
    }
  }
}

const devsdk_registry_impls devsdk_registry_keeper_fns =
{
  edgex_keeper_client_init,
  edgex_keeper_client_ping,
  edgex_keeper_client_get_common_config,
  edgex_keeper_client_get_config,
  edgex_keeper_client_write_config,
  edgex_keeper_client_register_service,
  edgex_keeper_client_deregister_service,
  edgex_keeper_client_query_service,
  edgex_keeper_client_free
};
