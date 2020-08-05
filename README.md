# Anjay-zephyr-client [<img align="right" height="50px" src="https://avsystem.github.io/Anjay-doc/_images/avsystem_logo.png">](http://www.avsystem.com/)

## Supported hardware and overview

This repository contains LwM2M Client application based on open-source
[Anjay](https://github.com/AVSystem/Anjay) library and
[Zephyr OS](https://github.com/zephyrproject-rtos/zephyr). This example targets
[B-L475E-IOT01A Discovery kit](https://www.st.com/en/evaluation-tools/b-l475e-iot01a.html).

The following LwM2M Objects are supported:

- Security (/0),
- Server (/1),
- Access Control (/2),
- Device (/3),
- Temperature (/3303),
- Humidity (/3304),
- Accelerometer (/3313),
- Magnetometer (/3314),
- Barometer (/3315),
- Distance (/3330),
- Gyrometer (/3334),
- Push button (/3347).

## Cloning the repository

```
git clone --recursive https://github.com/AVSystem/Anjay-zephyr-client
```

## Compilation guide

First of all, get Zephyr, SDK and other dependecies, as described in Zephyr's
[Getting Started Guide](https://docs.zephyrproject.org/latest/getting_started/index.html) (first 4 steps).

Before building the project, you need to prepare bash environment by running
`~/zephyrproject/zephyr/zephyr-env.sh`. The main directory of Zephyr may vary,
depending on argument passed to `west init`.

Finally, you may compile the project using:
```
west build -b disco_l475_iot1 .
```
in Anjay-zephyr-client root directory.

If the project doesn't compile because of the linking problems, please go to
the `~/zephyrproject/zephyr/` and ensure you're using commit
`1105aa521c298d74427e35d4cef27b501df40b6b`, or remove `CONFIG_FS_LOG_LEVEL_OFF=y`
from `prj.conf`. The second solution will cause a lot of logs from filesystem
being printed on bootup.

After successful build you can flash the target using `west flash`.

## Connecting to the LwM2M Server

To connect to [Coiote IoT Device
Management](https://www.avsystem.com/products/coiote-iot-device-management-platform/) LwM2M Server,
please register at https://www.avsystem.com/try-anjay/. Then have a look at the Configuration menu
to configure security credentials and other necessary settings (like Wi-Fi SSID etc.).

[Video guide showing basic usage of Try-Anjay](https://www.youtube.com/watch?v=fgy38XfttM8) is available on YouTube.

NOTE: You may use any LwM2M Server compliant with LwM2M 1.0 TS. The server URI can be changed
in the Configuration menu.

## Configuration menu

While connected to a serial port interface, and during bootup, the device shows:

```
Press any key to enter config menu...
```

You can then press any key on your keyboard to enter the configuration menu. After that you'll
see a few configuration options that can be altered and persisted within the flash memory for
future bootups.
