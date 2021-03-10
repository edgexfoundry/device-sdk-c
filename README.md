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
An outline device service and various examples are provided in
[```src/c/examples```](src/c/examples/README.md) to illustrate usage.

### Building with docker

To build the SDK in a docker image, run the following command:

`make docker`

This will generate the sdk files in a results directory at the root of this project.

Alternatively, you can build a docker image which can be used to build device services in, with the following command:

`docker build -t edgex-csdk-base:2.0.0 -f scripts/Dockerfile.alpine-base .`

You can then write a Dockerfile for your service that begins `FROM edgex-csdk-base:2.0.0`

### Versioning

Please refer to the EdgeX Foundry [versioning policy](https://wiki.edgexfoundry.org/pages/viewpage.action?pageId=21823969) for information on how EdgeX services are released and how EdgeX services are compatible with one another.  Specifically, device services (and the associated SDK), application services (and the associated app functions SDK), and client tools (like the EdgeX CLI and UI) can have independent minor releases, but these services must be compatible with the latest major release of EdgeX.

### Long Term Support

Please refer to the EdgeX Foundry [LTS policy](https://wiki.edgexfoundry.org/display/FA/Long+Term+Support) for information on support of EdgeX releases. The EdgeX community does not offer support on any non-LTS release outside of the latest release.
