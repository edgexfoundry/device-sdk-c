/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "metrics.h"
#include "parson.h"
#include "service.h"
#include "edgex_time.h"

#include <sys/time.h>
#include <sys/resource.h>

#ifdef __GNU_LIBRARY__
#include <malloc.h>
#endif

#include <microhttpd.h>

int edgex_device_handler_metrics
(
  void *ctx,
  char *url,
  edgex_http_method method,
  const char *upload_data,
  size_t upload_data_size,
  char **reply,
  const char **reply_type
)
{
  struct rusage rstats;
  edgex_device_service *svc = (edgex_device_service *)ctx;

  JSON_Value *val = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (val);

  JSON_Value *memval = json_value_init_object ();
  JSON_Object *memobj = json_value_get_object (memval);

#ifdef __GNU_LIBRARY__
  struct mallinfo mi = mallinfo ();
  json_object_set_number (memobj, "Alloc", mi.uordblks);
  json_object_set_number (memobj, "Heap", mi.arena + mi.hblkhd);
#endif

  json_object_set_value (obj, "Memory", memval);

  if (getrusage (RUSAGE_SELF, &rstats) == 0)
  {
    double cputime = rstats.ru_utime.tv_sec + rstats.ru_stime.tv_sec;
    cputime += (double)(rstats.ru_utime.tv_usec + rstats.ru_stime.tv_usec) / EDGEX_MICROS;
    double walltime = (double)(edgex_device_millitime() - svc->starttime) / EDGEX_MILLIS;
    json_object_set_number (obj, "CpuTime", cputime);
    json_object_set_number (obj, "CpuAvgUsage", cputime / walltime);
  }
  *reply = json_serialize_to_string (val);
  *reply_type = "application/json";
  json_value_free (val);
  return MHD_HTTP_OK;
}
