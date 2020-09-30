/*
 * Copyright (c) 2019
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "metrics.h"
#include "edgex2.h"
#include "edgex-rest.h"
#include "parson.h"
#include "service.h"
#include "iot/time.h"
#include "correlation.h"

#include <sys/time.h>
#include <sys/resource.h>

#ifdef __GNU_LIBRARY__
#include <malloc.h>
#include <sys/sysinfo.h>
#endif

#include <microhttpd.h>

void edgex_device_handler_metrics (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
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

  char *json = json_serialize_to_string (val);
  json_value_free (val);

  reply->data.bytes = json;
  reply->data.size = strlen (json);
  reply->content_type = CONTENT_JSON;
  reply->code = MHD_HTTP_OK;
}

static void edgex_metrics_populate (edgex_metricsresponse *m, uint64_t starttime)
{
  struct rusage rstats;
#ifdef __GNU_LIBRARY__
  double loads[1];
  struct mallinfo mi = mallinfo ();
  m->alloc = mi.uordblks;
  m->totalloc = mi.arena + mi.hblkhd;
  if (getloadavg (loads, 1) == 1)
  {
    m->loadavg = loads[0] * 100.0 / get_nprocs();
  }
#endif
  if (getrusage (RUSAGE_SELF, &rstats) == 0)
  {
    double walltime = (double)(iot_time_msecs() - starttime) / 1e3;
    double cputime = rstats.ru_utime.tv_sec + rstats.ru_stime.tv_sec;
    cputime += (double)(rstats.ru_utime.tv_usec + rstats.ru_stime.tv_usec) / 1e6;
    m->cputime = cputime;
    m->cpuavg = cputime / walltime;
  }
}

void edgex_device_handler_metricsv2 (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *)ctx;
  edgex_metricsresponse mr;
  memset (&mr, 0, sizeof (mr));

  edgex_baseresponse_populate ((edgex_baseresponse *)&mr, "v2", edgex_device_get_crlid (), MHD_HTTP_OK, NULL);
  edgex_metrics_populate (&mr, svc->starttime);

  edgex_metricsresponse_write (&mr, reply);
}
