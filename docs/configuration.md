# Device service configuration options

The device service configuration is held in TOML format. By default the SDK will load configuration from a file named `configuration.toml` in the `res` directory, but this may be changed using the --confdir, --file and --profile options.

Configuration parameters are organized within a number of sections. A section is represented by a TOML table, eg `[Service]`.

If the Registry is in use, configuration is contained in subfolders of `edgex/core/1.0/<service-name>`. The "Clients" section is not present in this scenario, as the Registry provides a specific mechanism for maintaining service information.

When a device service is run for the first time with Registry enabled, it reads its configuration from a TOML file and uploads it to the Registry.

The value of any configuration element can be over-ridden with a value from a corresponding environment variable. The service looks for an environment variable whose name is of the form

`<section-name>_<config-element>`

For example, `Service_ServerBindAddr`.

## Service section

Option | Type | Notes
:--- | :--- | :---
Host | String | This is the hostname to use when the service generates URLs pointing to itself. It must be resolvable by other services in the EdgeX deployment.
Port | Int | Port on which to accept the device service's REST API. The assigned port for experimental / in-development device services is 49999.
Timeout | Int | Time (in milliseconds) to wait between attempts to contact core-data and core-metadata when starting up.
ConnectRetries | Int | Number of times to attempt to contact core-data and core-metadata when starting up.
StartupMsg | String | Message to log on successful startup.
CheckInterval | String | The checking interval to request if registering with Consul
ServerBindAddr | String | The interface on which the service's REST server should listen. By default the server listens on all available interfaces.
MaxRequestSize | Int | Amount of data beyond which the service will reject an incoming HTTP request. Zero (the default) disables checking.

## Clients section

Defines the endpoints for other microservices in an EdgeX system.

### edgex-core-data

Option | Type | Notes
:--- | :--- | :---
Host | String | Hostname on which to contact the core-data service.
Port | Int | Port on which to contact the core-data service.

### edgex-core-metadata

Option | Type | Notes
:--- | :--- | :---
Host | String | Hostname on which to contact the core-metadata service.
Port | Int | Port on which to contact the core-metadata service.

## Writable section

This contains configuration which, if the registry is in use, can be altered without restarting the service

Option | Type | Notes
:--- | :--- | :---
LogLevel | String | Sets the logging level. Available settings in order of increasing severity are: TRACE, DEBUG, INFO, WARNING, ERROR.

### Writable/Device section

Option | Type | Notes
:--- | :--- | :---
DataTransform | Bool | For enabling/disabling transformations on data between the device and EdgeX. Defaults to true (enabled).
Discovery/Enabled | Bool | For enabling/disabling device discovery. Defaults to true (enabled).
Discovery/Interval | Int | Time between automatic discovery runs, in seconds. Defaults to zero (do not run discovery automatically).
MaxCmdOps | Int | Defines the maximum number of resource operations that can be sent to the driver in a single command.
MaxCmdResultLen | Int | Not implemented. Maximum string length for command results returned from the driver.
UpdateLastConnected | Bool | If true, update the LastConnected attribute of a device whenever it is successfully accessed. Defaults to false.

## Device section

Option | Type | Notes
:--- | :--- | :---
ProfilesDir | String | A directory which the service will scan at startup for Device Profile definitions in `.yaml` files. Any such profiles which do not already exist in EdgeX will be uploaded to core-metadata.
DevicesDir | String | A directory which the service will scan at startup for Device definitions in `.json` or `/toml` files. Any such devices which do not already exist in EdgeX will be uploaded to core-metadata.
EventQLength | Int | Sets the maximum number of events to be queued for transmission to core-data before blocking. Zero (default) results in no limit.

## Driver section

This section is for driver-specific options. Any configuration specified here will be passed to the driver implementation during initialization.

## MessageQueue section

Option | Type | Notes
:--- | :--- | :---
Type | String | If this option is present and set to `mqtt` or `redisstream`, the service will deliver events via the specified Message Bus implementation rather than by making REST calls to the core-data service.

The following basic options may be configured for Message Bus connections:

Option | Type | Notes
:--- | :--- | :---
Protocol | String | The protocol to use. For MQTT this is `tcp` or `ssl`. Defaults to `tcp`. For Redis this should be `redis`.
Host | String | The host on which the MQTT or Redis server is running. Defaults to `localhost`.
Port | Unsigned Int | The port on which the MQTT or Redis server is running. Defaults to `1883` for MQTT or `6379` for Redis.
Topic | String | The topic under which events are to be published. Defaults to `edgex/events` which results in events being published to `edgex/events/profile-name/device-name/source-name`. For Redis the `/` delimeter in topic names is replaced with `.`.

### MessageQueue/Optional section

The following options are unset by default but may be required in some applications, eg if ssl is to be used. All are recognized for MQTT but for Redis none are implemented.

Option | Type | Notes
:--- | :--- | :---
Username | String |
Password | String |
ClientId | String |
Qos | Unsigned Int | Available Qos values are 0 (default) ("fire and forget"), 1 ("at least once") or 2 ("exactly once").
KeepAlive | Unsigned Int | Measured in seconds. Defaults to 60.
Retained | Boolean | defaults to false, event messages are not retained on the MQTT server.
SkipCertVerify | Boolean | defaults to false, ie certificates are verified.
CertFile | String | Filename of a PEM-format file containing trusted certificates.
KeyFile | String | Filename of a PEM-format file containing the client's key and certificate chain.
