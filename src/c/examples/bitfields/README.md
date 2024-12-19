## Virtual Bitfields example

### About

This example device service illustrates the use of `mask` and `shift` in a
device profile to provide access to bitfields within a device register.

This may be useful in scenarios where access to a device is restricted to
fixed-size machine words and multiple logical values are packed into one
word.

The simulated device holds a single 32-bit register in which four 8-bit values
are held. Device Resources are defined which allow independent access to each
of these four values.

### Prerequisites

The environment variable CSDK_DIR should be set to a directory containing the
C SDK include files and libraries.

Set LD_LIBRARY_PATH to `$CSDK_DIR/lib:/opt/iotech/iot/1.5/lib`

### Building

```
gcc -I$CSDK_DIR/include -I/opt/iotech/iot/1.5/include -L$CSDK_DIR/lib -L/opt/iotech/iot/1.5/lib -o device-bitfields device-bitfields.c -lcsdk -liot
```

### Device Profile

A device profile for the simulated device is provided in the `res` directory. This will be uploaded to core-metadata by the device service on first run.

Note the `mask` and `shift` value properties which extract the relevant bytes.
When a value is read, first the individual value is selected via the mask, then
the shift operation moves it into the normal 0-255 8-bit range. When writing a
value, the shift and mask operations are performed in the reverse order.

### Implementation

Unlike other data transforms which are processed in the SDK, the device service
implementation needs to perform the `mask` operation in the put handler. This
is because a read-modify-write sequence is required, and this needs to be an
atomic operation to avoid inconsistent values being written. It is up to the
DS implementation to control concurrent access in an efficient manner suitable
for the class of device being managed. Here as we are operating in local memory
we can use an atomic compare/exchange to ensure consistency.

### Provisioning

The supplied configuration file `res/configuration.yaml` includes a definition for a single device. No protocol properties are defined for the device - the example runs on the basis of one device per service instance.

### Running the service

An EdgeX system containing at least a database and the core-data and core-metadata services must be running. The configuration file must be edited to reflect the locations of the core-data and core-metadata services.

```
./device-bitfields -cp=keeper.http://localhost:59890
```

To read value "B" (bits 8-15):

```
curl 0:59999/api/v3/device/name/Bitfields/B
```

To write a value into "B":

```
curl -X PUT -d '{"B":"221"}' 0:59999/api/v3/device/name/Bitfields/B
```
