# Device service startup parameters

Certain aspects of a device service's operation are controlled by command-line options and environment variable settings. These are not stored with the service configuration as they may be required before the configuration is read - in particular, they may be required in order to locate the configuration.

## Registry

If the Registry is to be used, its location may be specified as a URL. The `scheme` part of the URL indicates the registry implementation to use. Currently the only supported implentation in the SDK is `consul`, but other implementations may be added via the APIs presented in `edgex/registry.h`. For Consul, the URL takes the form `consul://hostname:port`.

|Environment variable||
|-|-|
`edgex_registry` | specifies the registry URL
`edgex_registry_retry_count` | sets the number of attempts to connect to the registry before abandoning startup
`edgex_registry_retry_wait` | sets the interval (in seconds) between attempts to connect to the registry

|Long option | short option||
|-|-|-|
`--registry` | `-r` | specifies the registry URL. Overrides the environment setting

## Service name

Typically a device service will have a default service name, eg device-modbus or device-virtual. However when an EdgeX deployment contains multiple instances of a particular device service, they must be assigned different names. This can be done on the command line:

|Long option | short option||
|-|-|-|
`--name` | `-n` | specifies the device service name

## Profile

A service has a default configuration profile, but other profiles may be selected using this option. In file-based configuration, additional profiles may be defined in files named `configuration-<profilename>.toml`. In Consul, they are stored in KV-store folders named `<servicename>;<profilename>`.

|Long option | short option||
|-|-|-|
`--profile` | `-p` | specifies the configuration profile

## Configuration directory

For file-based configuration, this is the directory containing TOML files. The default value is `res` (note that this is a relative path).

|Long option | short option||
|-|-|-|
`--confdir` | `-c` | specifies the configuration directory
