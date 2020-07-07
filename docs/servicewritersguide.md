Notes on writing a device service
---------------------------------
This document aims to complement the examples provided with the EdgeX C SDK by providing insight into what actions should be performed in the various parts of a Device Service.

Fundamentally a Device Service is composed of a number of callbacks. These callbacks are provided by the SDK to allow the service to respond to different events. These callbacks (devsdk_callbacks) are as follows:

* init
* reconfigure
* discover
* get
* put
* stop

A device service must provide an implementation of each callback, except for `discover` (which may be NULL if dynamic discovery is not implemented) and reconfigure (which may be NULL if the service has no driver-specific configuration). A small amount of setup is required of a device service, this is usually performed in the main. A devsdk_service_t should be created, containing, amongst other fields the devsdk_callbacks and an impldata pointer which is passed back every time a callback is invoked. The service must then call devsdk_service_new to create the device service, devsdk_service_start to start it, and upon exit the service should call devsdk_service_stop, followed by devsdk_service_free to clean up.

Init
----
Init is called when the device service starts up, its purpose is to perform protocol-specific initialization for the device service. This typically involves allocating memory for driver specific structures, initialising synchronisation mechanisms (mutex etc.) and setting up a logging client. The logging client is being provided to the implementation, most implementations will want to store the pointer in their impldata structure for later use. Any initialization required by the device should be performed here. 

Reconfigure
-----------
If the Registry is in use, then dynamic updates to configuration are possible. If an element of the driver-specific configuration is changed, this callback will be invoked with the new configuration settings passed through.

Discover
--------
In Discover, protocols which are able to perform discovery can do so. They must perform their protocol specific discovery and add devices using the edgex_device_add_device() method. Careful consideration should be given to whether this mechanism is required and how dynamically discovered devices can be intelligently and correctly mapped to device profiles.

Get
---
The Get handler deals with incoming requests to get data from a device. The following information is provided to the device service developer:

* void *impl - The impldata pointer given as part of edgex_device_service.
* char *devname - The name of the device being queried.
* devsdk_protocols *protocols - This provides information about the device that this get request is seeking to access. A list of protocols is supplied, each consists of a name and a set of name-value pairs representing the attributes required for that protocol. A function devsdk_protocols_properties() is provided which returns the name-value pairs corresponding to a named protocol.
* uint32_t nreadings - The following requests and reading parameters are arrays of size nreadings.
* const devsdk_commandrequest *requests - The name, attributes and type of each resource being requested.
* devsdk_commandresult * readings - Once a reading has been taken from a device, the resulting value is placed into the readings. This is used by the SDK to return the result to EdgeX.
* const devsdk_nvpairs *qparams - This contains any parameters requested via the query string of the request URL.
* iot_data_t ** exception - The handler may store a string here containing details in the event of a read failure.

In general the GET handler should implement a translation between a GET request from edgex and a read/get via the protocol-specific mechanism. Multiple sources of metadata are provided to allow the device-service to identify what it should query on receipt of the callback.

Put
---
The Put handler deals with requests to write/transmit data to a specific device. It is provided with the same set of metadata as the GET callback. However, this time the put handler should write the data provided to the device associated with the addressable. The process of using the metadata provided to perform the correct protocol-specific write/put action is similar to that of performing a get.

Stop
----
The stop handler is called when devsdk_service_stop is called. This handler should be used to clean-up all device service specific resources. Typically this would involve freeing any resources allocated during execution of the device service.

Optional handlers
-----------------

An implementation may implement the device_added, device_updated and device_removed handlers, it will then be notified of changes to the extent of devices managed by the service. This may be helpful if special initialization / shutdown operations are required for optimal usage of the device.

An implementation may also implement the ae_starter and ae_stopper callbacks, in which case it will become responsible for the AutoEvents functionality which is normally handled within the SDK. This facility is currently experimental, it may be useful in scenarios where the device can be set to generate readings autonomously.
