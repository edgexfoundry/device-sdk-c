## EdgeX Device C SDK

### About

The C SDK provides a framework for building EdgeX device services in C.

### Prerequisites

* A Linux build host
* A version of GCC supporting C11.
* CMake version 3 or greater and make.
* Development libraries and headers for:
  * curl (version 7.32 or later)
  * microhttpd (version 0.9)
  * libyaml (version 0.1.6 or later)
  * libcbor (version 0.5)
  * libuuid (from util-linux v2.x)

On Debian 10 "Buster" these libraries can be installed by
```
apt-get install libcurl4-openssl-dev libmicrohttpd-dev libyaml-dev libcbor-dev 
```

### Building

At the toplevel C SDK directory, run
```
make
```
This retrieves dependencies and uses CMake to build the SDK. Subsequent
rebuilds may be performed by moving to the ```build/release``` or
```build/debug``` directories and running ```make```.

A .tar.gz file containing the SDK headers and library is created in the
build/{debug, release} directories. When building on some distributions
a .deb or .rpm file is also created, as appropriate.

### Creating a Device Service

The main include file ```devsdk/devsdk.h``` contains the functions provided by
the SDK and defines the callbacks which a device service implementor needs to
create. Documentation is provided within that file in doxygen format.
An outline device service is provided in ```src/c/examples``` to illustrate
usage.

### Building with docker

To build the SDK in a docker image, run the following command:

`make docker`

This will generate the sdk files in a results directory at the root of this project.

Alternatively, you can build a docker image which can be used to build device services in, with the following command:

`docker build -t edgex-csdk-base:1.2.0 -f scripts/Dockerfile.alpine-3.9-base .`

You can then write a Dockerfile for your service that begins `FROM edgex-csdk-base:1.2.0`
