# Anjay Minimal Client [<img align="right" height="50px" src="https://avsystem.github.io/Anjay-doc/_images/avsystem_logo.png">](http://www.avsystem.com/)

Project with minimal features needed to run Anjay on zephyr.
It should serve as a template for custom projects.
## Supported hardware and overview

This folder contains LwM2M Client minimal application example for following targets:
 - [qemu_x86](https://docs.zephyrproject.org/latest/boards/x86/qemu_x86/doc/index.html)
 - [disco_l475_iot1](https://docs.zephyrproject.org/latest/boards/arm/disco_l475_iot1/doc/index.html)
 - [nrf9160dk_nrf9160_ns](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/ug_nrf9160.html)
 - [thingy91_nrf9160_ns](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/ug_thingy91.html)
 - [ESP32-DevKitC](https://www.espressif.com/en/products/devkits/esp32-devkitc)
 - [nrf52840dk_nrf52840](https://docs.zephyrproject.org/latest/boards/arm/nrf52840dk_nrf52840/doc/index.html)
 - [nRF7002 Development kit](https://www.nordicsemi.com/Products/Development-hardware/nRF7002-DK)
 - [Arduino Nano 33 BLE Sense Lite](https://store.arduino.cc/products/arduino-nano-33-ble-sense)
 - [DevEdge](https://devedge.t-mobile.com/solutions/iotdevkit)

The following LwM2M Objects are supported:
 - Security (/0)
 - Server (/1)
 - Device (/3)

## Compilation

Set West manifest path to `Anjay-zephyr-client/minimal`, and manifest file to `west.yml` and do `west update`.
```
west config manifest.path Anjay-zephyr-client/minimal
west config manifest.file west.yml
west update
```

You can now compile the project using `west build -b <target>` in `minimal` directory.

### Compilation guide for nRF9160DK, Thingy:91, nRF7002DK, nRF52840DK and Arduino Nano 33 BLE Sense

Because NCS uses different Zephyr version, it is necessary to change our Zephyr workspace, it is handled by using different manifest file.
Set West manifest path to `Anjay-zephyr-client/minimal`, and manifest file to `west-nrf.yml` and do `west update`.
```
west config manifest.path Anjay-zephyr-client/minimal
west config manifest.file west-nrf.yml
west update
```
Now you can compile the project using `west build -b nrf9160dk_nrf9160_ns`, `west build -b thingy91_nrf9160_ns`, `west build -b nrf7002dk_nrf5340_cpuapp`, `west build -b nrf52840dk_nrf52840` or `west build -b arduino_nano_33_ble_sense` in `minimal` directory, respectively. The last two commands compiles project for use with the OpenThread network, more about this can be found in the section [Connecting to the LwM2M Server with OpenThread](#connecting-to-the-lwm2m-server-with-openthread).

### Compilation guide for T-Mobile DevEdge

Because T-Mobile DevEdge board uses fork of Zephyr, it is necessary to change our Zephyr workspace, it is handled by using different manifest file.
Set West manifest path to `Anjay-zephyr-client/minimal`, and manifest file to `west-t-mobile.yml` and do `west update`.
```
west config manifest.path Anjay-zephyr-client/minimal
west config manifest.file west-t-mobile.yml
west update
```
Now you can compile the project using `west build -b tmo_dev_edge` in `minimal` directory.

> **__NOTE:__**
> For proper support of the cellular connection, you may need to set the right value of CONFIG_MODEM_MURATA_1SC_APN in `tmo_dev_edge.conf` file beforehand.


> **__NOTE:__**
> To switch back to mainstream zephyr version, change manifest file to `west.yml` and do `west update`.
> ```
> west config manifest.file west.yml
> west update
> ```
> [More about managing west workspace through manifests can be found here.](https://docs.zephyrproject.org/latest/guides/west/manifest.html)

## Flashing the target

After successful build you can flash the target using:

```shell
west flash
```

For the Thingy:91 target, if you don't have an external programmer board, you can also enter the MCUboot recovery mode by turning the Thingy:91 on while holding the main button (SW3), and use [mcumgr](https://github.com/apache/mynewt-mcumgr-cli):

```shell
mcumgr --conntype serial --connstring dev=/dev/ttyACM0 image upload build/zephyr/app_update.bin
mcumgr --conntype serial --connstring dev=/dev/ttyACM0 reset
```

### Flashing Arduino Nano 33 BLE Sense

Arduino boards are shipped with an onboard bootloader that requires Arduino variant of bossac tool.
There are two ways to obtain it, as described in `Programming and Debugging` section of
[Arduino Nano 33 BLE in Zephyr documentation](https://docs.zephyrproject.org/latest/boards/arm/arduino_nano_33_ble/doc/index.html).
According to the above, once you have a path to bossac, you are ready to flash the board.

Attach the board to your computer using the USB cable. Then, to get onboard bootloader ready to flash the program,
double-tap the RESET button. There should be a pulsing orange LED near the USB port.
In directory `anjay-zephyr-client/demo/` run command to flash a board:
```bash
west flash --bossac="<path to the arduino version of bossac>"
```
For example
```bash
west flash --bossac=$HOME/.arduino15/packages/arduino/tools/bossac/1.9.1-arduino2/bossac
```

## qemu_x86 networking setup

To enable networking when running qemu_x86 target and be able to connect to the internet,
there is an [official guide on networking with the host system](https://docs.zephyrproject.org/latest/connectivity/networking/networking_with_host.html). Below is a shorter version on how to run the example.

First download [Zephyr `net-tools` project](https://github.com/zephyrproject-rtos/net-tools)
and run `net-setup.sh`, this will create `zeth` interface that will be used for networking by qemu.
```
git clone https://github.com/zephyrproject-rtos/net-tools
cd net-tools
sudo ./net-setup.sh
```
While the script is running, `zeth` interface is available. Open up a second terminal window and set up ip forwarding
to allow connection to the internet, as `[internet_interface]` use your current interface that has access to internet.

There is a helper script `anjay_qemu_net.sh` that accepts interface name as an argument and calls commands below.
For example if network connection is on interface `wlan0` it can be called like this `sudo ./anjay_qemu_net.sh wlan0`
```
sysctl -w net.ipv4.ip_forward=1
iptables -A FORWARD -i zeth -o [internet_interface] -j ACCEPT
iptables -A FORWARD -i [internet_interface] -o zeth -m state --state ESTABLISHED,RELATED -j ACCEPT
iptables -t nat -A POSTROUTING -o [internet_interface] -j MASQUERADE
```

After that, `qemu_x86` target can be run, and will successfully register to outside lwm2m servers.
```
west build -t run
```

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

## Connecting to the LwM2M Server with OpenThread

To use this project on the nRF52840dk board or Arduino Nano 33 BLE Sense in addition to the configuration shown in the previous paragraph, you will need to configure the OpenThread Border Router and Commissioner as described in the guides from the links below.
You can change default `CONFIG_OPENTHREAD_JOINER_PSKD` value in the `boards/nrf52840dk_nrf52840.conf` for nRF or in `boards/arduino_nano_33_ble_sense.conf` for Arduino. In same file, replace `CONFIG_OPENTHREAD_MTD=y` with `CONFIG_OPENTHREAD_FTD=y` if you want your device to run as an FTD.

Resources:
- [Introduction to OpenThread](https://openthread.io/guides)
- [Border Router guide](https://openthread.io/guides/border-router)
- [Commissioner guide](https://openthread.io/guides/commissioner)
