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

This information is followed by three sections, "deviceResources",
"deviceCommands" and "coreCommands".

coreCommands
------------

This section specifies the commands which are available via the core-command
microservice, for reading and writing to the device.

Commands may allow get or put methods (or both). For a get type, the returned
values are specified in the "expectedValues" field, for a put type, the
parameters to be given are specified in "parameterNames". In either case, the
different http response codes that the service may generate are shown.

deviceCommands
--------------

These are presented at the "device" endpoint,
```
http://<device-service>:<port>/api/v1/device/<device id>/<command name>
```

This section defines access to reads and writes for multiple simultaneous
values. Each deviceCommand should contain a get and/or a set section, describing
the read or write operation respectively.

Each line of a get section indicates a deviceResource which is to be read, and
the lines in a set section indicate deviceResources to be written. The values
in these lines are as follows:

* index - a number, used to define an order in which the resource is processed.
* operation - get or set. Ignored in this implementation, mixing of get and set
operations is not supported.
* deviceResource - the name of the deviceResource to access.
* parameter - optional, a value that will be used if a PUT request does not
specify one.

deviceResources
---------------

These are also presented at the "device" endpoint,
```
http://<device-service>:<port>/api/v1/device/<device id>/<deviceResource name>
```

however if a profile contains a deviceCommand with the same name as a
deviceResource, the deviceCommand will take precedence.

A deviceResource specifies an individual value within a device that may be
read from or written to as part of a command. It has a name for identification
and a description for informational purposes.

The Attributes in a deviceResource are the device-service-specific parameters
required to access the particular value. Each device service implementation
will have its own set of named values that are required here, for example a
BACnet device service may need an Object Identifier and a Property Identifier
whereas a Bluetooth device service could use a UUID to identify a value.

The properties section in a deviceResource describes the value. Conventionally
each logical value is given two properties, named value and units. The
following fields are available in a property:

* type - Required. The data type of the value. Supported types are Bool,
Int8 - Int64, Uint8 - Uint64, Float32, Float64, Binary and String. Arrays of
all types other than String and Binary are also supported and named by adding
"Array" to the typename eg Int8Array.
* readWrite - "R", "RW", or "W" indicating whether the value is readable or
writable.
* defaultValue - a value used for PUT requests which do not specify one.
* base - a value to be raised to the power of the raw reading before it is returned.
* scale - a factor by which to multiply a reading before it is returned.
* offset - a value to be added to a reading before it is returned.
* mask - a binary mask which will be applied to an integer reading.
* shift - a number of bits by which an integer reading will be shifted right.
* minimum - a minimum value for data specified in set operations.
* maxumum - a maximum value for data specified in set operations.

The processing defined by base, scale, offset, mask and shift is applied in
that order. This is done within the SDK. A reverse transformation is applied
by the SDK to incoming data on set operations (mask transforms on set are
implemented in the driver)

The minimum and maximum values may be used to enforce a range limit on values
provided in write requests. They are only valid for numeric data, ie ints and floats.

The units property is used to indicate the units of the value, eg Amperes,
degrees C, etc. It should have only one field, a defaultValue that specifies the units.

The Device Profile in the C SDK
-------------------------------

When the SDK invokes the get or set handler method, parts of the device profile
which pertain to the request are passed in the devsdk_commandrequest
structure. This consists of the name, attributes and type of the deviceResource
which is being queried or set.

