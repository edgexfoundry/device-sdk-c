## Virtual Gyro example

### About

This example device service simulates a 3-axis gyro sensor. It returns
rotation rate data on each axis in a range -250 to +250 degrees per second.
The example illustrates the use of the resourceCommand construct in a device
profile to aggregate device resources.

### Prerequisites

The environment variable CSDK_DIR should be set to a directory containing the
C SDK include files and libraries.

Set LD_LIBRARY_PATH to $CSDK_DIR/lib

### Building

```
gcc -I$CSDK_DIR/include -L$CSDK_DIR/lib -o device-gyro device-gyro.c -lcsdk
```

### Device Profile

A device profile for the simulated device is provided in the `res` directory. This will be uploaded to core-metadata by the device service on first run.

### Provisioning

The supplied configuration file `res/configuration.toml` includes a definition for a single gyro device. No protocol properties are defined for the device - the example runs on the basis of one device per service instance.

### Running the service

An EdgeX system containing at least a database and the core-data and core-metadata services must be running. The configuration file must be edited to reflect the locations of the core-data and core-metadata services.

```
./device-gyro -c res
```

To take a set of readings from the gyro:

```
curl 0:49999/api/v2/device/name/Gyro/rotation
```

This returns an Event containing three Readings representing the three axes of measurement.
