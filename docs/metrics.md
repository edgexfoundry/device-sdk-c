# Device service metrics availability

Device services built with the C SDK support the EdgeX service metrics endpoint

```
http://host:port/api/v2/metrics
```

This provides brief information on CPU and memory usage. Example output (formatted
for clarity) is shown here:


```
{
  "apiVersion":"v2","requestId":"5959907d-87d6-4f66-8c1f-cc2c9f9d9d26","statusCode":200,

  "Alloc":329072,
  "TotalAlloc":839680
  "CpuLoadAvg":3.375,
  "CpuTime":0.027213000000000001,
  "CpuAvgUsage":0.0010293528009986004
}
```

The meaning of the fields is as follows:

* `Alloc` : Amount of heap memory in use, in bytes.
* `TotalAlloc` : Total heap size, in bytes.
* `CpuLoadAvg` : Average overall CPU usage for the last minute, as a percentage.
* `CpuTime` : The amount of CPU time used by this service, in seconds.
* `CpuAvgUsage`: The amount of CPU time used by this service, as a fraction of elapsed time.

