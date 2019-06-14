## Virtual Counter example

### About

This example device service simulates a number of counting devices. Each
device contains a single counter which is accessed via two operations: a
read operation which returns the current count and increments it, and a write
operation which resets the count.

### Prerequisites

The environment variable CSDK_DIR should be set to a directory containing the
C SDK include files and libraries.

Set LD_LIBRARY_PATH to $CSDK_DIR/lib

### Building

```
gcc -I$CSDK_DIR/include -L$CSDK_DIR/lib -o device-counter device-counter.c -lcsdk
```

### Device Profile

A device profile for the simulated devices is provided in the `res` directory. This will be uploaded to core-metadata by the device service on first run.

### Provisioning

The supplied configuration file `res/configuration.toml` includes definitions for two counter devices, one of which has an AutoEvent configured which causes counts to be generated every ten seconds. On first run of the device service, these devices will be created in metadata. Other devices may be created if required.

### Running the service

An EdgeX system containing at least a database and the core-data and core-metadata services must be running. The configuration file must be edited to reflect the locations of the core-data and core-metadata services.

```
./device-counter -c res
```

Once the service is running it will begin to send Events from the Counter1 device every ten seconds. To manually generate an Event,

```
curl 0:49990/api/v1/device/name/Counter2/Counter
```

To reset one of the counters,

```
curl -X PUT -d '{"Counter":"0"}' 0:49990/api/v1/device/name/Counter1/Counter
```

### Device details

The devices are addressed using the "Counter" protocol. This protocol has one property, "Index" which can take a value between 0 and 255. Thus the driver emulates up to 256 individual counter devices. This range may be altered by redefining the NCOUNTERS macro and rebuilding the service.

The driver supports a single request attribute, "register" which indicates which register of the emulated counter devices is to be accessed. In the example only a single register is supported, "count01" being the primary counter.
