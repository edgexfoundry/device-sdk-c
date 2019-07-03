## Device service examples

An outline device service, `template.c` is provided here.

Example device services "Random" and "Counters" are contained within their own subdirectories.

## Template device service

### About

template.c shows a device service in outline. All requests for get and set operations result in debugging output showing the device and resource addressing that is supplied to the driver.

### Prerequisites

The environment variable CSDK_DIR should be set to a directory containing the
C SDK include files and libraries.

Set LD_LIBRARY_PATH to $CSDK_DIR/lib

### Building

```
gcc -I$CSDK_DIR/include -L$CSDK_DIR/lib -o template template.c -lcsdk
```

### Device Profile

An example device profile for the simulated device is provided in the `res` directory. This will be uploaded to core-metadata by the device service on first run.

### Provisioning

The supplied configuration file `res/configuration.toml` includes definitions for a single device, including an AutoEvent which causes readings to be generated at ten-second intervals. On first run of the device service, this device will be created in metadata.

### Running the service

An EdgeX system containing at least a database and the core-data and core-metadata services must be running. The configuration file must be edited to reflect the locations of the core-data and core-metadata services.

```
./template -c res
```

Once the service is running it will begin to send a sequence of Events every ten seconds. To obtain a reading manually,

```
curl 0:49990/api/v1/device/name/Device1/SensorOne
```

Note that the device service returns a constant String reading regardless of the requested operation.
