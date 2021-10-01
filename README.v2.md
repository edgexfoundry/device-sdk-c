### SDK API v2

Version 1.2 of the SDK introduced v2 of the API. In version 2.0 (the EdgeX Ireland release) the v1 API is no longer available.

#### Using the v2 API

Replace `#include "edgex/devsdk.h"` with `#include "devsdk/devsdk.h"`

Replace `#include "edgex/device-mgmt.h"` with `#include "edgex/devices.h"` and/or `#include "edgex/profiles.h"`

Note that `"edgex/eventgen.h"` will no longer be needed.

The new functions and type definitions are prefixed with `devsdk_`, this replaces `edgex_device_`. The following sections describe the new API.

#### Value-holding types

```
devsdk_commandrequest
devsdk_commandresult
```

The pairing of an `edgex_propertytype` and an `edgex_device_resultvalue` is replaced by the `iot_data_t` type. This type is defined in `"iot/data.h"`

The type of data held in an `iot_data_t` may be obtained using the following function:

`edgex_propertytype edgex_propertytype_data (const iot_data_t *data)`

The tags in `edgex_propertytype` are now scoped with the `Edgex_` prefix.

The command request now contains a 'mask' field. If this is nonzero, implementations of the put handler should first read the current value in the resource, then compute `(current-value & mask) | request-value` and write that value. The driver should lock the resource so as to make this operation atomic.

Data is stored in an `iot_data_t` using the following functions:

```
iot_data_t * iot_data_alloc_i8 (int8_t val)
iot_data_t * iot_data_alloc_ui8 (uint8_t val)
(etc)
iot_data_t * iot_data_alloc_f32 (float val)
iot_data_t * iot_data_alloc_f64 (double val)
iot_data_t * iot_data_alloc_bool (bool val)
iot_data_t * iot_data_alloc_string (const char * val, iot_data_ownership_t ownership)
iot_data_t * iot_data_alloc_array (void *data, uint32_t length, iot_data_type_t type, iot_data_ownership_t ownership)
```

For Binary readings use an array of `uint8_t`. For readings of Object type, populate a string-keyed map:
```
value = iot_data_alloc_map (IOT_DATA_STRING);
iot_data_string_map_add (value, "Attribute1", iot_data_alloc_float (38.7));
iot_data_string_map_add (value, "Attribute2", iot_data_alloc_bool (true));
```

When no longer needed, an `iot_data_t` must be released:
```
void iot_data_free (iot_data_t * data)
```

Note however that in the get and set handler interactions, `iot_data_free` occurs in the SDK.

When allocating strings and blobs, the required ownership semantic is
specified. This can take the following values:

- `IOT_DATA_COPY` The data is copied on allocation and freed on free
- `IOT_DATA_TAKE` A pointer is taken to the data, which is then freed on free
- `IOT_DATA_REF` A pointer is taken to the data and no action is taken on free

This enables more choices for memory management in the device service and the potential to minimise the amount of data copying and allocations/deallocations.

#### Callback functions

Mandatory:

```
bool (*devsdk_initialize)
bool (*devsdk_handle_get)
bool (*devsdk_handle_put)
void (*devsdk_stop)
devsdk_address_t (*devsdk_create_address)
void (*devsdk_free_address)
devsdk_resource_attr_t (*devsdk_create_resource_attr)
void (*devsdk_free_resource_attr)
```

Optional:

```
void (*devsdk_discover)
devsdk_device_resources * (*devsdk_describe)
void (*devsdk_reconfigure)
void * (*devsdk_autoevent_start_handler)
void (*devsdk_autoevent_stop_handler)
void (*devsdk_add_device_callback)
void (*devsdk_update_device_callback)
void (*devsdk_remove_device_callback)
```

These are mostly identical to their v1 counterparts, with the following changes

- In `initialize`, Driver configuration is passed via an `iot_data_t` of `IOT_DATA_MAP` type rather than a name-value pair list. This permits numeric types to be passed as such rather than in strings.

- The new `reconfigure` callback allows the driver to respond to dynamic reconfiguration requests. This is possible when using the Registry.

- In `handle_get`, the incoming query options are passed as a string/string map instead of being marshalled into a special `Attribute`. Query options are also available for put requests.

- In `handle_get`, the type of the Reading data being requested is now specified using the structured typecode `iot_typecode_t`. An `edgex_propertytype` may be extracted from this if required using the following function:

`edgex_propertytype edgex_propertytype_typecode (const iot_typecode_t *tc)`

- New callback functions create/free_address and create/free_resource_attr are defined for parsing protocol properties and resource attributes respectively. This allows the parsing procedure to be performed once per device / device profile rather than on every get/put invocation

- In `handle_put` the data to be set is passed as an array of `iot_data_t` rather than an array of `commandresult`

- The get and put handlers allow for exceptions to be returned so that the implementation may give more information about a failure. These are implemented as iot_data_t; they should be set as IOT_DATA_STRING.

- The `disconnect_device` callback is removed (obsoleted by `remove_device`)

- A `describe` callback is added for future extended discovery functionality, subject to change

Note that all callbacks are now specified at `service_new` time.

A function devsdk_callbacks_init function is supplied for setting up the mandatory callbacks. Other devsdk_callbacks_set_x functions support setup of the optional callbacks.

#### Device Service Lifecycle

```
struct devsdk_service_t;
devsdk_usage
devsdk_service_new
devsdk_service_start
devsdk_post_readings
devsdk_service_stop
devsdk_service_free
```

The various initialization parameters have been removed from the `service_start` function, all such setup is now handled in the `service_new` function. This takes `argc` and `argv` and processes them in the same way as `service_processparams` did in v1. The default name for the service must be provided.

#### Configuration

The device_service_start function now takes an iot_data_t* which should hold a map of all implementation-specific configuration options with their default values. These will be overridden by values specified in the configuration file or registry, and by environment variables, before being passed back to the implementation in the initialize callback.

The SDK will apply a "Driver" scoping element to the names of the configuration values. Thus "Param1" will relate to an element in the "[Driver]" section of a .toml configuration file, in a "Driver" folder in the registry, or with a "DRIVER_" prefix when considering environment variables.

To specify that a configuration element may be updated dynamically, prefix it with "Writable/". The SDK will then apply scoping as above so that "Writable/Param2" becomes "Writable/Driver/Param2".

#### Device and Profile management

```
edgex_add_device
edgex_remove_device_byname
edgex_update_device
edgex_devices
edgex_get_device_byname
edgex_free_device
edgex_profiles
edgex_get_deviceprofile_byname
edgex_free_deviceprofile
edgex_add_profile
```

These are all renamed from their edgex_device_ counterparts in the v1 API.

#### Basic types

Some basic types have also changed:

`edgex_error` is renamed to `devsdk_error`
`edgex_nvpairs` is renamed to `devsdk_nvpairs`
`edgex_strings` is renamed to `devsdk_strings`
`edgex_protocols` is renamed to `devsdk_protocols`
`edgex_device_adminstate` is replaced by a boolean indicating whether the relevant device is to be accessible

