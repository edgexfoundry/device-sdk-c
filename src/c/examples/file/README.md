## File monitor example

### About

This example device service watches files on the local system and uploads
their content each time they are modified.
The example illustrates the use of the devsdk_post_readings function to
deliver events asynchronously.

### Prerequisites

The environment variable CSDK_DIR should be set to a directory containing the
C SDK include files and libraries.

Set LD_LIBRARY_PATH to $CSDK_DIR/lib

### Building

```
gcc -I$CSDK_DIR/include -L$CSDK_DIR/lib -o device-file device-file.c -lcsdk
```

### Device Profile

A device profile for the file devices is provided in the `res` directory. This will be uploaded to core-metadata by the device service on first run. The profile sets the MIME type of the files to text/plain, additional profiles could be constructed to specify files of other types.

### Provisioning

The supplied configuration file `res/configuration.toml` includes a definition for a single file device. The only protocol property required to address the device is its filename.

### Running the service

An EdgeX system containing at least a database and the core-data and core-metadata services must be running. The configuration file must be edited to reflect the locations of the core-data and core-metadata services.

```
./device-file -c res
```

To trigger a file upload:

```
echo "Bump!" >> ./res/lorem.txt
```
