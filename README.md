# Anjay-zephyr-client [<img align="right" height="50px" src="https://avsystem.github.io/Anjay-doc/_images/avsystem_logo.png">](http://www.avsystem.com/)

## Supported hardware and overview

This repository contains LwM2M Client application based on open-source
[Anjay Zephyr Module](https://github.com/AVSystem/Anjay-zephyr) and
[Zephyr OS](https://github.com/zephyrproject-rtos/zephyr). This example targets
[B-L475E-IOT01A Discovery kit](https://www.st.com/en/evaluation-tools/b-l475e-iot01a.html)
and [nRF9160 Development kit](https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF9160-DK).

The following LwM2M Objects are supported:

| Target         | Objects                                                                                                                                                |
|----------------|--------------------------------------------------------------------------------------------------------------------------------------------------------|
| Common         | Security (/0)<br>Server (/1)<br>Access Control (/2)<br>Device (/3)<br>Push button (/3347)                                                              |
| B-L475E-IOT01A | Temperature (/3303)<br>Humidity (/3304)<br>Accelerometer (/3313)<br>Magnetometer (/3314)<br>Barometer (/3315)<br>Distance (/3330)<br>Gyrometer (/3334) |
| nRF9160DK      | On/Off switch (/3342)                                                                                                                                  |

## Getting started

First of all, get Zephyr, SDK and other dependecies, as described in Zephyr's
[Getting Started Guide](https://docs.zephyrproject.org/latest/getting_started/index.html)
(first 4 steps).

After navigating to Zephyr workspace (~/zephyrproject is default after following Getting Started Guide), clone Anjay Zephyr client repository.
```
git clone --recursive https://github.com/AVSystem/Anjay-zephyr-client
```

Set West manifest path to `Anjay-zephyr-client`, and manifest file to `west.yml` and do `west update`.
```
west config manifest.path Anjay-zephyr-client
west config manifest.file west.yml
west update
```

Finally, you may compile the project for B-L475E-IOT01A using `west build` in `projects/stm32l475` directory.

### Compilation guide for nRF9160DK

To compile for nRF9160DK, additional GNU Arm Embedded Toolchain is necessary, to install it follow
[installing the toolchain for NCS guide](https://www.nordicsemi.com/Software-and-tools/Software/nRF-Connect-SDKhttps://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/gs_installing.html#install-the-toolchain).

Because NCS uses different Zephyr version, it is necessary to change our Zephyr workspace, it is handled by using different manifest file.
Set West manifest path to `Anjay-zephyr-client`, and manifest file to `west-nrf.yml` and do `west update`.
```
west config manifest.path Anjay-zephyr-client
west config manifest.file west-nrf.yml
west update
```
Now you can compile the project using `west build` in `projects/nrf9160` directory.

> **__NOTE:__**
> To switch back to mainstream zephyr version, change manifest file to `west.yml` and do `west update`.
> ```
> west config manifest.file west.yml
> west update
> ```
> [More about managing west workspace through manifests can be found here.](https://docs.zephyrproject.org/latest/guides/west/manifest.html)

## Flashing the target

After successful build you can flash the target using `west flash`.

## Connecting to the LwM2M Server

To connect to [Coiote IoT Device
Management](https://www.avsystem.com/products/coiote-iot-device-management-platform/)
LwM2M Server, please register at https://www.avsystem.com/try-anjay/. Then have
a look at the Configuration menu to configure security credentials and other
necessary settings (like Wi-Fi SSID etc.).

[Video guide showing basic usage of Try-Anjay](https://www.youtube.com/watchv=fgy38XfttM8)
is available on YouTube.

NOTE: You may use any LwM2M Server compliant with LwM2M 1.0 TS. The server URI
can be changed in the Configuration menu.

## Configuration menu

While connected to a serial port interface, and during bootup, the device shows:

```
Press any key to enter config menu...
```

You can then press any key on your keyboard to enter the configuration menu.
After that you'll see a few configuration options that can be altered and
persisted within the flash memory for future bootups.
