Notes on writing a device service
---------------------------------
This document aims to complement the examples provided with the EdgeX C SDK by providing insight into what actions should be performed in the various parts of a Device Service.

Fundamentally a Device Service is composed of a number of callbacks. These callbacks are provided by the SDK to allow the service to respond to different events. The required callbacks are as follows:

* init
* get
* put
* stop

A device service must provide an implementation of each callback. A small amount of setup is required of a device service, this is usually performed in the main. A devsdk_service_t should be created, containing, amongst other fields the devsdk_callbacks and an impldata pointer which is passed back every time a callback is invoked. The service must then call devsdk_service_new to create the device service, devsdk_service_start to start it, and upon exit the service should call devsdk_service_stop, followed by devsdk_service_free to clean up.

Init
----
Init is called when the device service starts up, its purpose is to perform protocol-specific initialization for the device service. This typically involves allocating memory for driver specific structures, initialising synchronisation mechanisms (mutex etc.) and setting up a logging client. The logging client is being provided to the implementation, most implementations will want to store the pointer in their impldata structure for later use. Any initialization required by the device should be performed here. 

Get
---
The Get handler deals with incoming requests to get data from a device. The following information is provided to the device service developer:

* void *impl - The impldata pointer given as part of edgex_device_service.
* devsdk_device_t *device - The device being queried. This structure includes the device name and its parsed protocol properties.
* uint32_t nreadings - The following requests and reading parameters are arrays of size nreadings.
* const devsdk_commandrequest *requests - The name, attributes and type of each resource being requested.
* devsdk_commandresult * readings - Once a reading has been taken from a device, the resulting value is placed into the readings. This is used by the SDK to return the result to EdgeX.
* const iot_data_t *options - This contains any options requested via the query string of the request URL.
* iot_data_t ** exception - The handler may store a string here containing details in the event of a read failure.

In general the GET handler should implement a translation between a GET request from edgex and a read/get via the protocol-specific mechanism. Multiple sources of metadata are provided to allow the device-service to identify what it should query on receipt of the callback.

Put
---
The Put handler deals with requests to write/transmit data to a specific device. It is provided with the same set of metadata as the GET callback. However, this time the put handler should write the data provided (in the values[] array) to the device associated with the addressable. The process of using the metadata provided to perform the correct protocol-specific write/put action is similar to that of performing a get.

Stop
----
The stop handler is called when devsdk_service_stop is called. This handler should be used to clean-up all device service specific resources. Typically this would involve freeing any resources allocated during execution of the device service.

Create Address
--------------

The Create Address handler is supplied with a list of protocols which address the device. Each protocol consists of a name and a string/string map representing the parameters required for that protocol. A function devsdk_protocols_properties() is provided which returns the map corresponding to a named protocol.

The handler should allocate and return an implementation-specific object represemting the address of the device in a parsed form that will be convenient for the get and put handlers to work with.

If the protocols cannot be successfully parsed, the handler should return NULL and indicate the problem by returning a string in the `exception` parameter.

Free Address
------------

This handler should free an object created by the Create Address handler.

Create Resource Attr
--------------------

This handler is analagous to Create Address but applies to the attributes specified for a device resource. The attributes are supplied to the function as a string-keyed map.

Free Resource Attr
------------------

This handler should free an object created by the Create Resource Attr handler.

Optional handlers
-----------------

An implementation may implement the device_added, device_updated and device_removed handlers, it will then be notified of changes to the extent of devices managed by the service. This may be helpful if special initialization / shutdown operations are required for optimal usage of the device.

An implementation may also implement the ae_starter and ae_stopper callbacks, in which case it will become responsible for the AutoEvents functionality which is normally handled within the SDK. This facility is currently experimental, it may be useful in scenarios where the device can be set to generate readings autonomously.

If the Registry is in use, then dynamic updates to configuration are possible. If the reconfiguration callback is registered, then when an element of the driver-specific configuration is changed, this callback will be invoked with the new configuration settings passed through.

An implementation may also implement the discover callback. When this is called, the implementation should perform a scan for reachable devices, and register them using the devsdk_add_discovered_devices function.

An implementation may also implement the validate_address callback. This is called when a device is added to the system. The function should check that the protocol properties given for a device are valid, and if not, return an exception (in which case the device addition will be aborted).
