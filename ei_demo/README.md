# Anjay Edge Impulse example [<img align="right" height="50px" src="https://avsystem.github.io/Anjay-doc/_images/avsystem_logo.png">](http://www.avsystem.com/)

This example runs example motion recognition model, built with Edge Impulse, and streams detection statistics to a LwM2M server.
## Supported hardware and overview

This example works on following targets:
 - `thingy91_nrf9160_ns`

The following LwM2M Objects are supported:
 - Security (/0)
 - Server (/1)
 - Device (/3)
 - Pattern Detector (/33650, custom object, see pattern_detector.xml)

## Compilation

Set West manifest path to `Anjay-zephyr-client/ei_demo`, and manifest file to `west-nrf.yml` and do `west update`.
```
west config manifest.path Anjay-zephyr-client/ei_demo
west config manifest.file west-nrf.yml
west update
```

You can now compile the project for Thingy:91 using `west build -b thingy91_nrf9160_ns` in `ei_demo` directory.

## Flashing the target

After successful build you can flash the target using `west flash`.

## Connecting to the LwM2M Server

To connect to [Coiote IoT Device
Management](https://www.avsystem.com/products/coiote-iot-device-management-platform/)
LwM2M Server, please register at https://eu.iot.avsystem.cloud/. Then have
a look at the Configuration menu using `west build -t guiconfig` or `west build -t menuconfig` to configure security credentials and other
necessary settings (like Wi-Fi SSID etc.).

[Guide showing basic usage of Coiote DM](https://iotdevzone.avsystem.com/docs/IoT_quick_start/Device_onboarding/)
is available on IoT Developer Zone.

NOTE: You may use any LwM2M Server compliant with LwM2M 1.0 TS. The server URI
can be changed in the Configuration menu.
