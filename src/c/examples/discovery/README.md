## Dynamic discovery example

### About

This example device service extends the template service to include dynamic
provisioning of devices.

### Prerequisites

The environment variable CSDK_DIR should be set to a directory containing the
C SDK include files and libraries.

Set LD_LIBRARY_PATH to $CSDK_DIR/lib

### Building

```
gcc -I$CSDK_DIR/include -L$CSDK_DIR/lib -o device-template template.c -lcsdk
```

### Device Profile

A device profile for the simulated device is provided in the `res` directory. This will be uploaded to core-metadata by the device service on first run.

### Dynamic Provisioning via Discovery

In this example, the service "finds" four devices when discovery runs. As
shipped, the service is configured to run discovery every ten seconds. If the
configuration is changed to prevent the automatic discovery runs (by setting
Discovery/Interval to 0), discovery may be forced by calling the discovery
endpoint manually:

```
curl -X POST 0:49999/api/v1/discovery
```

Initially, none of the discovered devices will be added to EdgeX, but by
using appropriate Provision Watchers (see below) they can be accepted.

### Running the service

An EdgeX system containing at least a database and the core-data and core-metadata services must be running. The configuration file must be edited to reflect the locations of the core-data and core-metadata services.

```
./device-template -c res
```

To upload the supplied Provision Watcher to core-metadata:

```
curl -X POST -d@watcher.json 0:48081/api/v1/provisionwatcher
```

The Provision Watcher matches two of the discovered devices. It works work by
specifying acceptance criteria (`identifiers`) that match all of the devices
(two each on HTTP and MAC protocols), and rejection criteria
(`blockingIdentifiers`) which specifically block two of them. Note that
`identifiers` is a regular expression, and `blockingIdentifiers` is a literal
string match.

When a discovered device matches a Provision Watcher, it is created and assigned
the Profile and AdministrativeState specified in that watcher.
