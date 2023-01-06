### SDK API v3

For EdgeX version 3.0 ("Minnesota") the SDK API is updated, and some re-coding of device service implementations will be required.

#### New dependency

In version 3.0 the SDK has a dependency on IOTech's C Utilities which should be satisfied by installing the relevant package. Previous
versions built the utilities into the SDK library.

#### Data types

The `type` field in both `devsdk_resource_t` and `devsdk_device_resources` is now an `iot_typecode_t` rather than a
pointer to one. Additionally the `type` field in `edgex_resourceoperation` is an `iot_typecode_t`.

The `edgex_propertytype` enum and the functions for obtaining one from `iot_data_t` have been removed. Instead, first consult the
`type` field of an `iot_typecode_t`. This is an instance of the `iot_data_type_t` enumeration, the enumerands of which are similar
to the EdgeX types, except that there are some additional values (not used in the C SDK) such as Vectors and Pointers, and there is
a singular Array type. The type of array elements is held in the `element_type` field of the `iot_typecode_t`.

Binary data is now supported directly in the utilities, so instead of allocating an array of uint8, the
`iot_data_alloc_binary` function is available.
