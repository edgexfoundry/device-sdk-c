<a name="EdgeX Device SDK C (found in device-sdk-c) Changelog"></a>
## EdgeX Device SDK C
[Github repository](https://github.com/edgexfoundry/device-sdk-c)

## [4.0.0] Odessa - 2025-03-12 (Only compatible with the 4.x releases)

### ✨  Features

- Use JSON object in message envelope payload instead of base64 string ([9f9740a…](https://github.com/edgexfoundry/device-sdk-c/commit/9f9740ab4a238d4cf3fc7521d89eb1913d297b2a))
```text

BREAKING CHANGE: Change MessageEnvelope payload from a byte array to a generic type

```
- Add support for listening to common config changes from core-keeper ([#534](https://github.com/edgexfoundry/device-sdk-c/issues/534)) ([0fa1e20…](https://github.com/edgexfoundry/device-sdk-c/commit/0fa1e200a1b4c9bd2451810fc76dfba979199818))
- Drop support for Consul ([#530](https://github.com/edgexfoundry/device-sdk-c/issues/530)) ([84c8015…](https://github.com/edgexfoundry/device-sdk-c/commit/84c80157ffc4713fcbabd445ae8f8e4d09b4c1ca))
```text

BREAKING CHANGE: Drop support for Consul

```
- Add device up/down detection ([#532](https://github.com/edgexfoundry/device-sdk-c/issues/532)) ([7abe510…](https://github.com/edgexfoundry/device-sdk-c/commit/7abe510cbd0ecbc788545c64ca7f1b281e5aa337))
- Support core-keeper for registry and config ([#520](https://github.com/edgexfoundry/device-sdk-c/issues/520)) ([62fc64c…](https://github.com/edgexfoundry/device-sdk-c/commit/62fc64c90db9bc421ef18031b23432b09bce4dbe))
- Remove Redis Pub/Sub feature and dependency ([#527](https://github.com/edgexfoundry/device-sdk-c/issues/527)) ([8c5e39f…](https://github.com/edgexfoundry/device-sdk-c/commit/8c5e39fa7b5b04e520d4bb53faca14d55facea43))
```text

BREAKING CHANGE: Remove Redis Pub/Sub feature and dependency

```
- Implemented a discovery delete API ([#514](https://github.com/edgexfoundry/device-sdk-c/issues/514)) ([de2f7dd…](https://github.com/edgexfoundry/device-sdk-c/commit/de2f7dd484aed31b8929cc3252062c81ad15313f))
- Publish System Events for device discovery and add discovery request ID ([#513](https://github.com/edgexfoundry/device-sdk-c/issues/513)) ([30a24dd…](https://github.com/edgexfoundry/device-sdk-c/commit/30a24dda2410b85b52167bc1398eb9d9a2b4959b))
```text

BREAKING CHANGE: Publish System Events for device discovery and add discovery request ID 

```
- Add optional Parent field to Device objects ([#512](https://github.com/edgexfoundry/device-sdk-c/issues/512)) ([6abec0d…](https://github.com/edgexfoundry/device-sdk-c/commit/6abec0de53f1544e48264173dbee0aea1a2aa14a))
```text

BREAKING CHANGE: Add Parent field to Device objects

```

### 🐛 Bug Fixes

- Correct map iteration in protocols_convert function ([#544](https://github.com/edgexfoundry/device-sdk-c/issues/544)) ([eb8e2ff…](https://github.com/edgexfoundry/device-sdk-c/commit/eb8e2ff18f0ad6c69de4cfb237d4ad036326ba8e))
- Release `iot_data_to_json` results after use ([#545](https://github.com/edgexfoundry/device-sdk-c/issues/545)) ([f77cf90…](https://github.com/edgexfoundry/device-sdk-c/commit/f77cf90271422167f9817fc2f645f2c019bc5257))
- Allow `-cp` flag to work without `-r` flag ([#535](https://github.com/edgexfoundry/device-sdk-c/issues/535)) ([02ffd3a…](https://github.com/edgexfoundry/device-sdk-c/commit/02ffd3a41c4b218657de9ceac7ee305f7bb09d80))
- Add support for additional MQTT protocol aliases ([#524](https://github.com/edgexfoundry/device-sdk-c/issues/524)) ([1dcfe63…](https://github.com/edgexfoundry/device-sdk-c/commit/1dcfe63ad036bf8d78932132d5dcad6aabcde801))
- Missing discovery delete wrapper for secure mode ([#521](https://github.com/edgexfoundry/device-sdk-c/issues/521)) ([b4a60c1…](https://github.com/edgexfoundry/device-sdk-c/commit/b4a60c126f741d84c14ee104c3263f3235d33930))
- Only 20 devices end up in devmap ([#555](https://github.com/edgexfoundry/device-sdk-c/issues/555)) ([#518](https://github.com/edgexfoundry/device-sdk-c/issues/518)) ([6bae00d…](https://github.com/edgexfoundry/device-sdk-c/commit/6bae00d1ebd04fbf3bb2a120a46a31d42b2bc678))
- Address CVE in Alpine base image ([#496](https://github.com/edgexfoundry/device-sdk-c/issues/496)) ([b5fd43a…](https://github.com/edgexfoundry/device-sdk-c/commit/b5fd43aa6b4e8069e65248caa2e7d6db03f961ef))
- Assert failures on null/empty maps ([#504](https://github.com/edgexfoundry/device-sdk-c/issues/504)) ([c55c6a0…](https://github.com/edgexfoundry/device-sdk-c/commit/c55c6a0709e44599dc9784425fcb0f18c72b9bf5))
- Edgex_device_read does not read labels ([#508](https://github.com/edgexfoundry/device-sdk-c/issues/508)) ([9db6249…](https://github.com/edgexfoundry/device-sdk-c/commit/9db6249cb5b64beed6f22149c866398dbcee266e))
- Memory leaks in devsdk_post_readings function ([#507](https://github.com/edgexfoundry/device-sdk-c/issues/507)) ([6bdae90…](https://github.com/edgexfoundry/device-sdk-c/commit/6bdae90b977cdeecb957260a94da902cfcf10c29))
- When reading's value is string type, it will be released prematurely ([#506](https://github.com/edgexfoundry/device-sdk-c/issues/506)) ([c533cd2…](https://github.com/edgexfoundry/device-sdk-c/commit/c533cd2e0d9c0aa79f18f2e3eeb4447a32e46d6c))
- Remove the refcount from the autoevent impl structure ([#502](https://github.com/edgexfoundry/device-sdk-c/issues/502)) ([cc49468…](https://github.com/edgexfoundry/device-sdk-c/commit/cc49468877935c119f35277098ef123672d1c8b2))
- The slash of a string cannot be removed using json_serialize_to_string ([#505](https://github.com/edgexfoundry/device-sdk-c/issues/505)) ([0864ff1…](https://github.com/edgexfoundry/device-sdk-c/commit/0864ff1b436a7a9e42f7c9629cf36a2dea6c3a02))

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
