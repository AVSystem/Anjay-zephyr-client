# Anjay-zephyr-client [<img align="right" height="50px" src="https://avsystem.github.io/Anjay-doc/_images/avsystem_logo.png">](http://www.avsystem.com/)

## Overview

This repository contains LwM2M Client samples based on open-source
[Anjay Zephyr Module](https://github.com/AVSystem/Anjay-zephyr) and
[Zephyr OS](https://github.com/zephyrproject-rtos/zephyr).

The following examples are present:

| Directory      | Description                                                                                                                                            |
|----------------|--------------------------------------------------------------------------------------------------------------------------------------------------------|
| demo/          | Showcase example implementing all features and sensors available on board. Supported boards:<br>[B-L475E-IOT01A Discovery kit](https://www.st.com/en/evaluation-tools/b-l475e-iot01a.html)<br>[nRF9160 Development kit](https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF9160-DK)<br>[Nordic Thingy:91 Prototyping kit](https://www.nordicsemi.com/Products/Development-hardware/Nordic-Thingy-91)<br>any other board of your choice (by adding appropriate `*.conf`/`*.overlay` files|


## Getting started

First of all, get Zephyr, SDK and other dependecies, as described in Zephyr's
[Getting Started Guide](https://docs.zephyrproject.org/latest/getting_started/index.html)
(first 4 steps).

After navigating to Zephyr workspace (~/zephyrproject is default after following Getting Started Guide), clone Anjay Zephyr client repository.
```
git clone https://github.com/AVSystem/Anjay-zephyr-client
```

If you want to compile for nRF9160 targets, additional GNU Arm Embedded Toolchain is necessary, to install it follow
[installing the toolchain for NCS guide](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/gs_installing.html#install-the-toolchain). Also for flashing [nRF Command Line Tools](https://www.nordicsemi.com/Products/Development-tools/nrf-command-line-tools) have to be installed.

For further instructions, check `README.md` in example folder in which you are interested.
## Connecting to the LwM2M Server

To connect to [Coiote IoT Device
Management](https://www.avsystem.com/products/coiote-iot-device-management-platform/)
LwM2M Server, please register at https://www.avsystem.com/try-anjay/. Then have
a look at the example configuration to configure security credentials and other
necessary settings (like Wi-Fi SSID etc.).

[Video guide showing basic usage of Try-Anjay](https://www.youtube.com/watchv=fgy38XfttM8)
is available on YouTube.

NOTE: You may use any LwM2M Server compliant with LwM2M 1.0 TS. The server URI
can be changed in the example configuration options.
