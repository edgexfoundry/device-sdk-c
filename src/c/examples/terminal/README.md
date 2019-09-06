## Virtual Terminal example

### About

This example device service illustrates a mechanism for accepting actuation
commands within the EdgeX device model. It allows messages to be written to
a terminal at specified positions.

When the service is run, it will clear the terminal in which it is running,
and wait for commands. The only command implemented takes as its parameters a
string to be written to the terminal, and the x and y coordinates at which to
write it.

### Prerequisites

Building the device service requires the `ncurses` development library to be
installed on the system.

The environment variable CSDK_DIR should be set to a directory containing the
C SDK include files and libraries.

Set LD_LIBRARY_PATH to $CSDK_DIR/lib

### Building

```
gcc -I$CSDK_DIR/include -L$CSDK_DIR/lib -o device-terminal device-terminal.c -lcsdk -lncurses
```

### Device Profile

A device profile for the simulated device is provided in the `res` directory. This will be uploaded to core-metadata by the device service on first run.

### Provisioning

The supplied configuration file `res/configuration.toml` includes definitions for a single terminal device. No protocol properties are defined for the device - the example runs on the basis of one device per service instance.

### Running the service

An EdgeX system containing at least a database and the core-data and core-metadata services must be running. The configuration file must be edited to reflect the locations of the core-data and core-metadata services.

```
./device-terminal -c res
```

Note that for this example the service log is written to a file (`device-terminal.log`). To display a message on the terminal,

```
curl -X PUT -d '{"Message":"Hello World", "Xposition":"35", "Yposition":"12"}' 0:49999/api/v1/device/name/Terminal/WriteMsg
```

### Implementation notes

The service uses the defaulting mechanism in deviceCommands in order to pass the name of the operation being requested to the implementation. The `parameter` field in a resource operation sets a value to be used if one is not supplied in the PUT request.
