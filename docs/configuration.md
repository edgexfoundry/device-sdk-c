# Device service configuration options

The device service configuration is held in TOML format. The SDK method `edgex_device_service_start` takes the name of a directory, and an optional profile name. It will attempt to load the configuration from a file named `configuration.toml` or `configuration-<profile>.toml` in that directory.

Configuration parameters are organized within a number of sections. A section is represented by a TOML table, eg `[Service]`. Multiple Schedules and ScheduleEvents may be configured, this is done using an appropriate number of `[[Schedule]]` and `[[ScheduleEvent]]` tables.

## Service section

Option | Type | Notes
:--- | :--- | :---
Host | String | This is the hostname to use when the service generates URLs pointing to itself. It must be resolvable by other services in the EdgeX deployment.
Port | Int | Port on which to accept the device service's REST API.
Timeout | Int | Time (in milliseconds) to wait between attempts to contact core-data and core-metadata when starting up.
ConnectRetries | Int | Number of times to attempt to contact core-data and core-metadata when starting up.
StartupMsg | String | Message to log on successful startup.
ReadMaxLimit | Int | Limits the number of items returned by a GET request to `/api/v1/device/all/<command>`.
CheckInterval | String | The checking interval to request if registering with Consul

## Clients section

Defines the endpoints for other microservices in an EdgeX system. If using a
registry service this section is not required.

### Data

Option | Type | Notes
:--- | :--- | :---
Host | String | Hostname on which to contact the core-data service.
Port | Int | Port on which to contact the core-data service.

### Metadata

Option | Type | Notes
:--- | :--- | :---
Host | String | Hostname on which to contact the core-metadata service.
Port | Int | Port on which to contact the core-metadata service.

## Device section

Option | Type | Notes
:--- | :--- | :---
DataTransform | Bool | For enabling/disabling transformations on data between the device and EdgeX. Defaults to true (enabled).
Discovery | Bool | For enabling/disabling device discovery. Defaults to true (enabled).
InitCmd | String | Not implemented. Specifies a resource command to be automatically generated when a device is added to the service.
InitCmdArgs | String | Not implemented. Specifies arguments to be included with InitCmd.
MaxCmdOps | Int | Defines the maximum number of resource operations that can be sent to the driver in a single command.
MaxCmdResultLen | Int | Not implemented. Maximum string length for command results returned from the driver.
RemoveCmd | String | Not implemented. Specifies a resource command to be automatically generated when a device is removed from the service.
RemoveCmdArgs | String | Not implemented. Specifies arguments to be included with RemoveCmd.
ProfilesDir | String | A directory which the service will scan at startup for Device Profile definitions in `.yaml` files. Any such profiles which do not already exist in EdgeX will be uploaded to core-metadata.
SendReadingsOnChanged | Bool | Not implemented. To be used to suppress the submission of readings to core-data if the value has not changed.

## Logging section

Option | Type | Notes
:--- | :--- | :---
RemoteURL | String | If this option is set, logs will be submitted to a logging service at the specified URL.
File | String | If this option is set, logs will be written to the named file. Setting a value of "-" causes logs to be written to standard output.
LogLevel | String | Sets the logging level. Available settings in order of increasing severity are: TRACE, DEBUG, INFO, WARNING, ERROR.

## Driver section

This section is for driver-specific options. Any configuration specified here will be passed to the driver implementation during initialization.

## Watchers section

Watchers are not supported in this release.
