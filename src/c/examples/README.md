## Device service examples

An outline device service, `template.c` is provided here.

A number of example device services illustrating specific concepts are provided within their own subdirectories:

Example | Notes
--- | ---
[Random](random/README.md) | A simple device service
[Counters](counters/README.md) | Device addressing via ProtocolProperties
[Discovery](discovery/README.md) | Dynamic device discovery and provision watchers
[Gyro](gyro/README.md) | Use of `resourceCommand` to aggregate Readings
[Terminal](terminal/README.md) | One possible mechanism for accepting actuation commands
[Bitfields](bitfields/README.md) | Use `mask` and `shift` attributes to access bitfields within a device register
[File](file/README.md) | Use of `devsdk_post_readings` to generate Events autonomously

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

Once the service is running it will begin to send a sequence of Events every ten seconds. It will also respond to the REST API for device services. To obtain a reading manually,

```
curl 0:59999/api/v2/device/name/Device1/SensorOne
```

Note that the template device service returns a constant String reading regardless of the requested operation.

