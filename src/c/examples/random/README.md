## Random generator example

### About

This example device service simulates a device with two random number generators and a switch whose state may be read and written.

### Prerequisites

The environment variable CSDK_DIR should be set to a directory containing the
C SDK include files and libraries.

Set LD_LIBRARY_PATH to $CSDK_DIR/lib

### Building

```
gcc -I$CSDK_DIR/include -L$CSDK_DIR/lib -o device-random device-random.c -lcsdk
```

### Device Profile

A device profile for the simulated device is provided in the `res` directory. This will be uploaded to core-metadata by the device service on first run.

### Provisioning

The supplied configuration file `res/configuration.toml` includes definitions for a single "random" device, and two AutoEvents which causes readings to be generated at intervals. On first run of the device service, this device will be created in metadata. The example emulates only a single device.

### Running the service

An EdgeX system containing at least a database and the core-data and core-metadata services must be running. The configuration file must be edited to reflect the locations of the core-data and core-metadata services.

```
./device-random -c res
```

Once the service is running it will begin to send one sequence of Events every ten seconds, and another every fifteen. To obtain a reading manually,

```
curl 0:49999/api/v2/device/name/RandomDevice1/SensorOne
```

To read the switch state,
```
curl 0:49999/api/v2/device/name/RandomDevice1/Switch
```

To set the switch state,

```
curl -X PUT -d '{"Switch":"true"}' 0:49999/api/v2/device/name/RandomDevice1/Switch
```

### Device details

The example emulates a single device and ignores all protocols information sent to it.

The driver supports the request attributes "SensorId" and "SwitchID" which indicate which random generator or switch is being requested. An additional attribute "SensorType" is used to select the range for random numbers.
