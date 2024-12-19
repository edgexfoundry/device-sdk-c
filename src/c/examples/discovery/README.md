## Dynamic discovery example

### About

This example device service extends the template service to include dynamic
provisioning of devices.

### Prerequisites

The environment variable CSDK_DIR should be set to a directory containing the
C SDK include files and libraries.

Set LD_LIBRARY_PATH to `$CSDK_DIR/lib:/opt/iotech/iot/1.5/lib`

### Building

```
gcc -I$CSDK_DIR/include -I/opt/iotech/iot/1.5/include -L$CSDK_DIR/lib -L/opt/iotech/iot/1.5/lib -o device-template template.c -lcsdk -liot
```

### Device Profile

A device profile for the simulated device is provided in the `res` directory. This will be uploaded to core-metadata by the device service on first run.

### Running the service

An EdgeX system containing at least a database and the core-data and core-metadata services must be running. The configuration file must be edited to reflect the locations of the core-data and core-metadata services.

```
./device-template -cp=keeper.http://localhost:59890
```

### Dynamic Provisioning via Discovery

In this example, the service "finds" four devices when discovery runs. As
shipped, the service is configured to run discovery every ten seconds. If the
configuration is changed to prevent the automatic discovery runs (by setting
Discovery/Interval to 0), discovery may be forced by calling the discovery
endpoint manually:

```
curl -X POST 0:59999/api/v3/discovery
```

Initially, none of the discovered devices will be added to EdgeX, but by
using appropriate Provision Watchers they can be accepted. To upload the
supplied Provision Watchers to core-metadata:

```
curl -X POST -d@watcher1.json 0:59881/api/v3/provisionwatcher
curl -X POST -d@watcher2.json 0:59881/api/v3/provisionwatcher
```

The Provision Watchers each match one of the discovered devices. They work by
specifying acceptance criteria (`identifiers`) that match potentially many
devices, and rejection criteria (`blockingIdentifiers`) which specifically
block one of them. Note that `identifiers` is a regular expression, and
`blockingIdentifiers` is a literal string match.

A Device must have Protocol fields which match all identifiers and none of the
blocking identifiers in order for it to be passed by a Provision Watcher.

When a discovered device matches a Provision Watcher, it is created and assigned
the Profile and AdministrativeState specified in that watcher.
