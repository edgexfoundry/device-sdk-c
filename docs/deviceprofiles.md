Notes on device profiles
------------------------

This document gives a brief overview of the contents of device profiles, how
these affect the behavior of the SDK, and how their elements are presented to
the device service implementation.

The device profile
------------------

The device profile describes a type of device within the EdgeX system. Each
device managed by a device service has an association with a device profile,
which defines that device type in terms of the operations which it supports.

The profile contains some identification information - it has a name and
a description, and a set of labels. It also indicates the brand name of the
device to which it applies, and the manufacturer of that device.

This information is followed by three sections, "deviceResources", "resources"
and "commands".

Commands
--------

This section specifies the commands which are available via the REST API, for
reading and writing to the device. These are presented at the "device" endpoint,
```
http://<device-service>:<port>/api/v1/device/<device id>/<command name>
```
Commands may allow get or put methods (or both). For a get type, the returned
values are specified in the "expectedValues" field, for a put type, the
parameters to be given are specified in "parameterNames". In either case, the
different http response codes that the service may generate are shown.

Note that in the current implementation, only the command name and its get/put
nature are processed by the device service: the other information is indicative
only.

Resources
---------

In this section the relationship between commands and the values on the device
(device resources) to which they apply is defined. Each resource corresponds
to a command, with which it shares its name. It should contain a get and/or a
set section, describing the get or put command respectively.

Each line of a get section indicates a deviceResource which is to be read, and
the lines in a set section indicate deviceResources to be written. The values
in these lines are as follows:

* index - a number, used to define an order in which the resource is processed.
* operation - get or set. Ignored in this implementation, mixing of get and set
operations is not supported.
* object - the name of the deviceResource to access.
* parameter - the name of the corresponding parameter in a PUT request, or the
returned reading in a GET request.
* property - the property within the deviceResource which is to be read or
written. This is generally "value".

deviceResources
---------------

A deviceResource specifies an individual value within a device that may be
read from or written to as part of a command. It has a name, which identifies
it in a Resource, and a description for informational purposes.

The Attributes in a deviceResource are the device-service-specific parameters
required to access the particular value. Each device service implementation
will have its own set of named values that are required here, for example a
BACnet device service may need an Object Identifier and a Property Identifier
whereas a Bluetooth device service could use a UUID to identify a value.

The properties section in a deviceResource describes the value. Conventionally
each logical value is given two properties, named value and units. The
following fields are available in a property:

* type - Required. The data type of the value. Supported types are bool,
int8 - int64, uint8 - uint64, float32, float64, binary and string. Note that the
undifferentiated Integer and Float types are deprecated in EdgeX and not
supported by the SDK.
* readWrite - "R", "RW", or "W" indicating whether the value is readable or
writable.
* defaultValue - a value assumed before any readings are taken.
* base - a value to be raised to the power of the raw reading before it is returned.
* scale - a factor by which to multiply a reading before it is returned.
* offset - a value to be added to a reading before it is returned.

The processing defined by base, scale and offset is applied in that order. This
is done within the SDK.

The units property is used to indicate the units of the value, eg Amperes,
degrees C, etc. It should have a type of String, readWrite "R" indicating
read-only, and a defaultValue that specifies the units.

The Device Profile in the C SDK
-------------------------------

When the SDK invokes the get or set handler method, parts of the device profile
which pertain to the request are passed in the edgex_device_commandrequest
structure.

* edgex_resourceoperation represents a get or set line of an operation within
the resource section.
* edgex_deviceobject represents a deviceResource.

In most cases the required information will be

* The "property" field in edgex_resourceoperation. This names the property of
the deviceResource that is to be read or written, usually "value".
* The "attributes" field in the edgex_deviceobject.
* The datatype required, found in edgex_deviceobject->properties->value->type.

