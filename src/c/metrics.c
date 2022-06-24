/*
 * Copyright (c) 2019-2022
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

#include <sys/time.h>
#include <sys/resource.h>

#ifdef __GNU_LIBRARY__
#include <malloc.h>
#include <sys/sysinfo.h>
#endif

#include <microhttpd.h>

static void edgex_metrics_populate (edgex_metricsresponse *m, uint64_t starttime, const char *name)
{
  struct rusage rstats;
#ifdef __GNU_LIBRARY__
  double loads[1];
#if (__GLIBC__ * 100 + __GLIBC_MINOR__) >= 233
  struct mallinfo2 mi = mallinfo2 ();
#else
  struct mallinfo mi = mallinfo ();
#endif
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
  m->svcname = name;
}

void edgex_device_handler_metricsv2 (void *ctx, const devsdk_http_request *req, devsdk_http_reply *reply)
{
  devsdk_service_t *svc = (devsdk_service_t *)ctx;
  edgex_metricsresponse mr;
  memset (&mr, 0, sizeof (mr));

  edgex_baseresponse_populate ((edgex_baseresponse *)&mr, "v2", MHD_HTTP_OK, NULL);
  edgex_metrics_populate (&mr, svc->starttime, svc->name);

  edgex_metricsresponse_write (&mr, reply);
}
