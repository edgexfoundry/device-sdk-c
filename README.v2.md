### SDK API v2

Version 1.2 of the SDK introduces v2 of the API. Both versions will be usable until the release of version 2.0, currently expected to be the EdgeX Hanoi release.

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

The tags in `edgex_propertytype` are now scoped with the `Edgex_` prefix, although the old type names are still available via `#define` constants.

Data is stored in an `iot_data_t` using the following functions:

```
iot_data_t * iot_data_alloc_i8 (int8_t val)
iot_data_t * iot_data_alloc_ui8 (uint8_t val)
(etc)
iot_data_t * iot_data_alloc_f32 (float val)
iot_data_t * iot_data_alloc_f64 (double val)
iot_data_t * iot_data_alloc_bool (bool val)
iot_data_t * iot_data_alloc_string (const char * val, iot_data_ownership_t ownership)
iot_data_t * iot_data_alloc_blob (uint8_t * data, uint32_t size, iot_data_ownership_t ownership)
```

When no longer needed, an `iot_data_t` must be released:
```
void iot_data_free (iot_data_t * data)
```

However in all interactions between the SDK and the DS implementation, `iot_data_free` occurs in the SDK.

Note that when allocating strings and blobs, the required ownership semantic is
specified. This can take the following values:

- `IOT_DATA_COPY` The data is copied on allocation and freed on free
- `IOT_DATA_TAKE` A pointer is taken to the data, which is then freed on free
- `IOT_DATA_REF` A pointer is taken to the data and no action is taken on free

This enables more choices for memory management in the device service and the potential to minimise the amount of data copying and allocations/deallocations.

#### Callback functions

```
bool (*devsdk_device_initialize)
void (*devsdk_discover)
bool (*devsdk_handle_get)
bool (*devsdk_handle_put)
void (*devsdk_stop)
void * (*devsdk_autoevent_start_handler)
void (*devsdk_autoevent_stop_handler)
void (*devsdk_add_device_callback)
void (*devsdk_update_device_callback)
void (*devsdk_remove_device_callback)
```

These are mostly identical to their v1 counterparts, with the following changes

- In `initialize`, Driver configuration is passed via an `iot_data_t` of `IOT_DATA_MAP` type rather than a name-value pair list. This permits numeric types to be passed as such rather than in strings.

- In `handle_get`, the incoming query parameters are passed as a name-value pair list instead of being marshalled into a special `Attribute`

- In `handle_get`, the type of the Reading data being requested is now specified using the structured typecode `iot_typecode_t`. An `edgex_propertytype` may be extracted from this if required using the following function:

`edgex_propertytype edgex_propertytype_typecode (const iot_typecode_t *tc)`

- In `handle_put` the data to be set is passed as an array of `iot_data_t` rather than an array of `commandresult`

- The get and put handlers allow for exceptions to be returned so that the implementation may give more information about a failure. These are implemented as iot_data_t; they should be set as IOT_DATA_STRING.

- The `disconnect_device` callback is removed (obsoleted by `remove_device`)

Note that all callbacks are now specified at `service_new` time. NULL may be given for all except `intialize`, `handle_get`, `handle_put` and `stop`.

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

The various initialization parameters have been removed from the `service_start` function, all such setup is now handled in the `service_new` function. This takes `argc` and `argv` and processes them in the same way as `service_processparams` did in v1. The default name for the service must be provided. No other changes have been made to these functions.

#### Device and Profile management

```
edgex_add_device
edgex_remove_device
edgex_remove_device_byname
edgex_update_device
edgex_devices
edgex_get_device
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

