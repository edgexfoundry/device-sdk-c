Notes on writing a device service
---------------------------------
This document aims to complement the example provided with the EdgeX C SDK (template.c) by providing insight into what actions should be performed in the various parts of a Device Service.

Fundamentally a Device Service is composed of a number of callbacks. These callbacks are provided by the SDK to allow the service to respond to different events. These callbacks (edgex_device_callbacks) are as follows:

* init
* discover
* get
* put
* disconnect
* stop

A device service must provide an implementation of each callback. A small amount of setup is required of a device service, this is usually performed in the main. An edgex_device_service should be created, containing, amongst other fields the edgex_device_callbacks and an impldata pointer which is passed back every time a callback is invoked. The service must then call edgex_device_service_start, upon exit the service should call edgex_device_service_stop.

Init
----
Init is called when the device service starts up, its purpose is to perform protocol-specific initialization for the device service. This typically involves allocating memory for driver specific structures, initialising synchronisation mechanisms (mutex etc.) and setting up a logging client. The logging client is being provided to the implementation, most implementations will want to store the pointer in their impldata structure for later use. Any initialization required by the device should be performed here.

Discover
--------
In Discover, protocols which are able to perform discovery can do so. They must perform their protocol specific discovery and add devices using the edgex_device_add_device() method. Careful consideration should be given to whether this mechanism is required and how dynamically discovered devices can be intelligently and correctly mapped to device profiles.

Get
---
The Get handler deals with incoming requests to get data from a device. The following information is provided to the device service developer:

* void *impl - The impldata pointer given as part of edgex_device_service.
* edgex_addressable *devaddr - This provides information about the endpoint that this get request is seeking to access. Typically this is mapped to a device-specific client/connection etc.
* uint32_t nreadings - The following requests and reading parameters are arrays of size nreadings.
* edgex_device_commandrequest *requests - The deviceresource and resourceoperation supplied are pointers into the lists of such objects held in the relevant device profile. As such, each contains a "next" pointer. This should be ignored - each edgex_device_commandrequest describes only one operation. Here a device service would typically drill down to the attributes used to describe device resources, it would use these attributes to understand how to query the device for the requested data. Note that if a deviceresource is accessed directly, the resourceoperation will be NULL.
* edgex_device_commandresult * readings - Once a reading has been taken from a device, the resulting value is placed into the readings. This is used by the SDK to return the result to EdgeX. If a reading is of String or Binary type, memory ownership is taken by the SDK.

In general the GET handler should implement a translation between a GET request from edgex and a read/get via the protocol-specific mechanism. Multiple sources of metadata are provided to allow the device-service to identify what it should query on receipt of the callback.

Put
---
The Put handler deals with requests to write/transmit data to a specific device. It is provided with the same set of metadata as the GET callback. However, this time the put handler should write the data provided to the device associated with the addressable. The process of drilling into the metadata provided and using this to perform the correct protocol-specific write/put action is similar to that of performing a get.

Disconnect
----------
Currently the disconnect callback is not used.

Stop
----
The stop handler is called when edgex_device_service_stop is called. This handler should be used to clean-up all device service specific resources. Typically this would involve freeing any resources allocated during execution of the device service.
