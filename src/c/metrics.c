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
#include "iot/time.h"

#include <sys/time.h>
#include <sys/resource.h>

#ifdef __GNU_LIBRARY__
#include <malloc.h>
#include <sys/sysinfo.h>
#endif

#include <microhttpd.h>

int edgex_device_handler_metrics
(
  void *ctx,
  char *url,
  const devsdk_nvpairs *qparams,
  edgex_http_method method,
  const char *upload_data,
  size_t upload_data_size,
  void **reply,
  size_t *reply_size,
  const char **reply_type
)
{
  struct rusage rstats;
  devsdk_service_t *svc = (devsdk_service_t *)ctx;

  JSON_Value *val = json_value_init_object ();
  JSON_Object *obj = json_value_get_object (val);

#ifdef __GNU_LIBRARY__
  JSON_Value *memval = json_value_init_object ();
  JSON_Object *memobj = json_value_get_object (memval);

  struct mallinfo mi = mallinfo ();
  json_object_set_uint (memobj, "Alloc", mi.uordblks);
  json_object_set_uint (memobj, "TotalAlloc", mi.arena + mi.hblkhd);

  json_object_set_value (obj, "Memory", memval);

  double loads[1];
  if (getloadavg (loads, 1) == 1)
  {
    json_object_set_number (obj, "CpuLoadAvg", loads[0] * 100.0 / get_nprocs());
  }
#endif


  if (getrusage (RUSAGE_SELF, &rstats) == 0)
  {
    double cputime = rstats.ru_utime.tv_sec + rstats.ru_stime.tv_sec;
    cputime += (double)(rstats.ru_utime.tv_usec + rstats.ru_stime.tv_usec) / 1e6;
    double walltime = (double)(iot_time_msecs() - svc->starttime) / 1e3;
    json_object_set_number (obj, "CpuTime", cputime);
    json_object_set_number (obj, "CpuAvgUsage", cputime / walltime);
  }
  *reply = json_serialize_to_string (val);
  *reply_size = strlen (*reply);
  *reply_type = "application/json";
  json_value_free (val);
  return MHD_HTTP_OK;
}
