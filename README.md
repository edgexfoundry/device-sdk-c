## EdgeX Device C SDK

### About

The C SDK provides a framework for building EdgeX device services in C.

### Prerequisites

* A Linux build host
* A version of GCC supporting C99.
* CMake version 3 or greater and make.
* Development libraries and headers for curl, microhttpd and yaml.

### Building

At the toplevel C SDK directory, run
```
./scripts/build.sh
```
This retrieves dependencies and uses CMake to build the SDK. Subsequent
rebuilds may be performed by moving to the ```build/release``` or
```build/debug``` directories and running ```make```.

### Creating a Device Service

The main include file ```edgex/devsdk.h``` contains the functions provided by
the SDK and defines the callbacks which a device service implementor needs to
create. Documentation is provided within that file in doxygen format.
An outline device service is provided in ```src/c/examples``` to illustrate
usage.

### Building with docker

To build the docker image which will build the deliverable, run the following command:

`docker build -t device-sdk-go-c-builder -f scripts/Dockerfile.alpine-3.7 .`

Once you have the builder image, to compile the code, run the following command:

`docker run --rm -e UID -e GID -v $PWD/release:/edgex-c-sdk/build/release device-sdk-go-c-builder`

This will generate the release files at the root of this project.
