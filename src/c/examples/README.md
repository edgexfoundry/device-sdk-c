## Build the Example Device Service

After [building the C SDK](../../../README.md), you can build this example device service using `make`:

1. First, tell the compiler where to find the C SDK files::
```
    export CSDK_DIR=../../../build/release/_CPack_Packages/Linux/TGZ/csdk-0.7.2
```

> The exact path to your compiled CSDK_DIR may differ, depending on the tagged version number on the SDK

2. Now you can build the example device service executable::
```
    make 
```

## Running the Example Device Service

To run the example device service, you first need an instance of the EdgeX Foundry stack running for it to use.

1. Follow the [Getting Started](https://docs.edgexfoundry.org/Ch-GettingStartedUsers.html) guide to start all of the EdgeX services in Docker.  From the folder containing the docker-compose file, start EdgeX with a call to:
```
    docker-compose up -d
```

2. Back in the example Device Service directory, you must tell the device service where to find the `libcsdk.so`::
```
    export LD_LIBRARY_PATH=$CSDK_DIR/lib
```

3. Then you can run your device service::
```
    ./device-example-c
```

4. You should now see your Device Service having it's /Random command called every 10 seconds. You can verify that it is sending data into EdgeX by watching the logs of the `edgex-core-data` service::
```
    docker logs -f edgex-core-data
```

Which would print an Event record every time your Device Service is called. Note that the value of the "randomnumber" reading is an integer between 0 and 100::

    INFO: 2019/02/05 20:27:05 Posting Event: {"id":"","pushed":0,"device":"RandNum-Device01","created":0,"modified":0,"origin":1549398425000,"schedule":null,"event":null,"readings":[{"id":"","pushed":0,"created":0,"origin":0,"modified":0,"device":null,"name":"randomnumber","value":"63"}]}
    INFO: 2019/02/05 20:27:05 Putting event on message queue

## Creating your own Device Service

This example device service demonstrates how the Device Serevice C SDK can be used to build an EdgeX Foundry Device Service. For a complete guide on how to do this, read our [Guide to writing a Device Service in C](https://docs.edgexfoundry.org/Ch-GettingStartedSDK-C.html).