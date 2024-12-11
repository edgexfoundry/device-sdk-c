# Device service startup parameters

Certain aspects of a device service's operation are controlled by command-line options and environment variable settings. These are not stored with the service configuration as they may be required before the configuration is read - in particular, they may be required in order to locate the configuration. Each commandline option may be overridden by an environment variable.

## Registry

If configuration is to be obtained from the Registry, its location may be specified as a URL. The `scheme` part of the URL indicates the registry implementation to use. Currently the only supported implentation in the SDK is `keeper.http`, where the URL takes the form `keeper.http://hostname:port`.

|Long option | Short option|Environment ||
|-|-|-|-|
`--configProvider` | `-cp` | `EDGEX_CONFIGURATION_PROVIDER` | specifies the registry URL.

## Service name

Typically a device service will have a default service name, eg device-modbus or device-virtual. However when an EdgeX deployment contains multiple instances of a particular device service, they must be assigned different names. This can be done by assigning an instance name on the command line. The instance name is then appended to the default service name, so if the bacnet device service is run with `-i floor3` the service name will be `device-bacnet_floor3`.

|Long option | Short option|Environment ||
|-|-|-|-|
`--instance` | `-i` | `EDGEX_INSTANCE_NAME` | specifies the instance name

## Profile

A service has a default configuration profile, but other profiles may be selected using this option. In file-based configuration, additional profiles may be defined in files named `configuration-<profilename>.yaml`. In Core Keeper, they are stored in KV-store named `edgex/v4/<servicename>`.

|Long option | Short option|Environment ||
|-|-|-|-|
`--profile` | `-p` | `EDGEX_PROFILE` | specifies the configuration profile

## Configuration directory

For file-based configuration, this is the directory containing TOML files. The default value is `res` (note that this is a relative path).

|Long option | Short option|Environment ||
|-|-|-|-|
`--confdir` | `-c` | `EDGEX_CONF_DIR` | specifies the configuration directory

## Configuration file

For file-based configuration, this is the filename of the TOML configuration file. If this option is used, the `--profile` option will be ineffective.

|Long option | Short option|Environment ||
|-|-|-|-|
`--file` | `-f` | `EDGEX_CONFIG_FILE` | specifies the configuration filename

## Timeouts

Two additional environment variables control the retry behavior when contacting the registry at startup:

|Environment variable||
|-|-|
`EDGEX_STARTUP_DURATION` | sets the amount of time (in seconds) in which to attempt to connect to the registry before abandoning startup
`EDGEX_STARTUP_INTERVAL` | sets the interval (in seconds) between attempts to connect to the registry

## Logging

The default log level during startup is INFO. The log level is controlled by the `[Writable]/LogLevel` configuration entry. If debugging of the service startup (before the configuration has been read) is required, the override environment variable `WRITABLE_LOGLEVEL` should be set to the desired level name.
