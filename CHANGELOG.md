<a name="EdgeX Device SDK C (found in device-sdk-c) Changelog"></a>
## EdgeX Device SDK C
[Github repository](https://github.com/edgexfoundry/device-sdk-c)

## [v3.1.0] Napa - 2023-11-15 (Only compatible with the 3.x releases)

### ✨  Features

- Add command line/environment flag for commonConfig ([#486](https://github.com/edgexfoundry/device-sdk-c/issues/486)) ([753c0e6…](https://github.com/edgexfoundry/device-sdk-c/commit/753c0e66273d645c82de476ca0a438ee98891f55))
- Load common configuration from config provider ([#484](https://github.com/edgexfoundry/device-sdk-c/issues/484)) ([4c52de3…](https://github.com/edgexfoundry/device-sdk-c/commit/4c52de37db7d3bd0277fba2739b75d0a1bd475b3))
- Update examples to v3 ([75d9ea2…](https://github.com/edgexfoundry/device-sdk-c/commit/75d9ea246e864e615ecdca3b0f3acd24537be31e))


### 🐛 Bug Fixes

- Support for any JSON data type in device command value ([34c657e…](https://github.com/edgexfoundry/device-sdk-c/commit/34c657e38ec517a99e13edaa49f10ff67381ac75))
- Update edgex_get_transformArg to reflect data type changes in ResourceProperties ([3139641…](https://github.com/edgexfoundry/device-sdk-c/commit/3139641de59f77553c7e4c591b1edec7a5540cea))
- Bypass JWT check in non-secure mode ([0aa84d6…](https://github.com/edgexfoundry/device-sdk-c/commit/0aa84d6e2362150f2a7fe76af15db12a3a58aab4))
- Update devsdk_post_readings to alloc and free correction id ([4f8820a…](https://github.com/edgexfoundry/device-sdk-c/commit/4f8820ad03012f3493f4efc0712d2a639770311b))
- Read actual status code after a POST device API call ([c449cd6…](https://github.com/edgexfoundry/device-sdk-c/commit/c449cd6a2c57881ffa1c10c806da9f662413e186))
- Invalid pointer operation in edgex_bus_mqtt_msgarrvd ([ba5e076…](https://github.com/edgexfoundry/device-sdk-c/commit/ba5e0764dcf3c1a727d0662f352e94cf5bde288b))
- Update edgex_data_to_b64 to ignore the last null character ([4504df9…](https://github.com/edgexfoundry/device-sdk-c/commit/4504df9bdc42e67 B

### 🤖 Continuous Integration

- Add automated release workflow on tag creation ([18b6c29…](https://github.com/edgexfoundry/device-sdk-c/commit/18b6c29ec549221dfc69d991ca1228da44bcacc1))


Changes for 3.0.2:

- Update examples to v3
- Remove space character from the URL for uploading device profile
- Update edgex_data_to_b64 to remove the trailing null character from JSON data
- Fix invalid pointer manipulation in edgex_bus_mqtt_msgarrvd
- Update devsdk_post_readings to alloc and free correction id
- Update edgex_metadata_client_add_or_modify_device to read status code from Multi-Status response
- Update edgex_get_transformArg to reflect data type changes in ResourceProperties
- Bypass JWT check in insecure mode

Changes for 3.0.1:

- Fix secret loading in insecure mode

Changes for 3.0.0 "Minnesota":

- Support EdgeX v3 API
  - Flatten config layout in Consul
  - Changed commandline flags and env variables
  - MessageBus configuration changes
  - Query parameters ds-pushevent, ds-returnevent now booleans (true/false)
  - See v3 migration guide for details
- SDK API updates: see README.v3.md
- YAML replaces TOML for configuration file
- REST callbacks from core-metadata replaced by system events
- Implementation of token-based security
- Events always publish to MessageBus (not core-data)
- Legacy metrics API removed
- MQTT support for all message bus operations (not just event push)

Changes for 2.3.0 "Levski":

- Publish metrics on pub/sub (redis only)
- Implement device commands on pub/sub (redis only)
- Implement MaxEventSize configuration

Changes for 2.2.0 "Kamakura":

- Implement ProtocolProperties validation
- Implement secure Consul access using tokens from Vault
- Add CORS support
- Implement secure messagebus connections
- Implement loading secrets from file
- Use Alpine 3.15 for container builds
- Array elements in readings are no longer stringified
  - to re-enable stringification, add the '-legacyv2' flag when
    running scripts/build.sh
- Retry attempts to retrieve client endpoints from registry
- Add serviceName field to version, ping, config, metrics APIs

Changes for 2.1.0 "Jakarta":

- Fix event push via MQTT
- Fix race condition on device deletion with autoevents

Changes for 2.0.1:

- Initial support for Object typed Readings (GET operations only)
- Fix segfault on startup when using MQTT for MessageQueue
- Support EDGEX_STARTUP_DURATION, EDGEX_STARTUP_INTERVAL
- Support EDGEX_USE_REGISTRY
- Support credential loading from Hashicorp Vault

Changes for 2.0.0 "Ireland":

- v2 API update: reconfigure callback no longer mandatory
- v2 API update: new callbacks for parsing attributes and protocol properties
- Ports for EdgeX microservices are re-assigned, eg device services move from 499xx to 599xx.
- Provision Watchers may include AutoEvents to be added on discovered devices.
- Support event transmission by mqtt or redis streams rather than rest
- Device definitions for upload no longer supported in configuration. Instead files may be read from a specified directory (as for profiles)
  - These may be in .json files or the previous .toml format
- Added helper functions for setting up the callback functions structure. This is now the preferred way of doing so.
- REST API moves to v2. v1 interfaces are no longer available.
- v1 C API is no longer supported.
- Add queryparams for PUT operations (Changes signature of put handler function)
- Add MaxRequestSize configuration for limiting inbound http data transfers
- EdgeX resource type names are now CamelCase as per Go services
- Configuration elements which may be updated without restarting the service are found in the "Writable" section of the configuration. The following elements are moved:
  - Logging/LogLevel -> Writable/LogLevel
  - Device section -> Writable/Device except for EventQLength and ProfilesDir
- Support for logging to file and/or the support-logging service has been removed.

Changes for 1.3.1:

- The device service's AdminState can be used to disable/re-enable a running service.

Changes for 1.3.0 "Hanoi":

- 'minimum' and 'maximum' ValueProperties are effective.
- PUT commands on api/v2/device can read CBOR payloads as an alternative to JSON
- 'mask' valueproperty is passed as part of the command request. Drivers should
  atomically perform a read-modify-write operation if this is nonzero. (See README.v2.md)
- v2 API update: Callback added to inform the driver of a configuration update.
- v2 API update: Implementations to supply default values for their configuration.
  - This allows use of numerics/boolean types as well as Strings.
- Event transmission is serialized and operates from a queue with configurable
  maximum length.

Changes for 1.2.2:

- Add ServerBindAddr configuration option for restricting the REST listener
  to a single address.

Changes for 1.2.1:

- Fixes in the compatibility layer for the v1 API.

Changes for 1.2.0 "Geneva":

- --instance flag introduced for suffixing the device service name.
- Environment variable names for config over-rides are all-uppercase.
- Device lastConnected property is managed (configurable and disabled by default)
- Implementations may change the operational state of a device.
- Commandline and environment options aligned with Go microservices.
- Arrays of primitive types supported in Readings.
- Device Readings are populated with datatype information.
- Full dynamic discovery implementation with Provision Watchers.
- New SDK API: see README.v2.md for details.
  - the v1 API will be removed in version 2.0.0
- Utility functions added in edgex-base.h for finding Protocol Properties, and
  for obtaining numeric values from name-value pair lists.

Changes for 1.1.0 "Fuji":

- ResourceOperation field "object" is renamed to "deviceResource".
  - "object" is still recognized for compatibility purposes but is deprecated.
- Implement "defaultValue" and "parameter" fields for write operations.
- Allow query strings to be passed to device GET requests.
- Added /api/version endpoint for obtaining service version strings.
- Commandline argument processing mostly handled within the SDK.
- Added callback API for device addition/update/removal events.
- Configuration can be overridden via environment variables when populating
  the registry.
- Subtables are supported in the Driver configuration.
- Device services de-register from the registry on exit.
- Event timestamps are now at nanosecond resolution, and are monotonic.

Changes for 1.0.0 "Edinburgh":

- EnableLogging replaces RemoteURL in the Logging configuration. The location
  of the logging service is configured in the [Clients] section.
- Removed ReadMaxLimit configuration option.
- Events containing binary Readings are encoded using CBOR.
  - This introduces a dependency on libcbor.
- Data transformations (other than "mask") are applied for device PUT requests.
- When querying for Device Profiles they are returned in a list in the same way
  as devices.
- Device service now follows the new/start/stop/free lifecycle pattern.
- Initial support for configuration changes without restarting.
- Support Correlation IDs. This introduces a requirement for libUUID.
- The API for Event generation other than via device GET requests is split out
  into a seperate header edgex/eventgen.h
- Added support for Automatic Events.
- The API for managing devices and profiles is split out into a seperate header
  edgex/device-mgmt.h
- The device name is passed to the Get and Set handlers. The commandrequest
  structure is simplified.
- Example implementation includes setting service name and registry location
  from the command line and environment.
- Float data is marshalled as base64 unless specified otherwise in the
  floatEncoding field of the device profile.
- Schedules and ScheduleEvents are removed.
- The get and set handlers now take an edgex_protocols object rather than an
  edgex_addressable.
- The device endpoint can be used to access either resources or deviceResources.
  The commands section of a device profile is not used.
- Endpoints for other EdgeX microservices are obtained from the registry (if
  enabled) rather than configuration.
- Metadata for individual devices can be obtained in the device management API.
- The edgex_deviceobject type is renamed to edgex_deviceresource to reflect its
  naming in the profile definition.
- The registry service is now identified by a URL, eg "consul://localhost:8500".
  - Support for consul as the registry service is built-in, but other
    implementations may be added using the API in edgex/registry.h
- Support EdgeX System Management API.
- Initial support for binary data, marshalled as base64 strings.
- Assertions on data readings are implemented.
- Some parts of the edgex model are now held in appropriate types rather
  than strings.
  - edgex_device_resulttype is renamed to edgex_propertytype, reflecting its
    broader usage.

Changes for 0.7.2:

- Device GET commands return Event structures rather than name-value pairs.

Changes for 0.7.1:

- Some verification on Device Profiles is now done at startup. This may result
  in failures during startup rather than in "device" command handling.
- The /device/name/{name}/{command} REST endpoint is implemented.
- Metadata callbacks for device creation and deletion are processed.

Changes for 0.7.0 "Delhi":

- Consul is supported for registration and configuration.
  - New configuration option Service.CheckInterval controlling how frequently
    the registry polls the device service for liveliness.
  - Driver init functions get their configuration as a name-value pair list
    rather than a TOML table.
  - SDK start function now takes parameters for Consul's hostname and port. They
    are no longer included in the configuration file.

- "Discovery", "DataTransform" and "ReadMaxLimit" configuration options are now effective.
- "OpenMsg" option replaced by "StartupMsg" for consistency with other services.
