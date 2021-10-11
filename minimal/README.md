# Anjay Minimal Client [<img align="right" height="50px" src="https://avsystem.github.io/Anjay-doc/_images/avsystem_logo.png">](http://www.avsystem.com/)

Project with minimal features needed to run Anjay on zephyr.
It should serve as a template for custom projects.
## Supported hardware and overview

This folder contains LwM2M Client minimal application example for following targets:
 - [qemu_x86](https://docs.zephyrproject.org/latest/boards/x86/qemu_x86/doc/index.html)
 - [disco_l475_iot1](https://docs.zephyrproject.org/latest/boards/arm/disco_l475_iot1/doc/index.html)
 - [nrf9160dk_nrf9160ns](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/ug_nrf9160.html)
 - [thingy91_nrf9160ns](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/ug_thingy91.html)

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

### Compilation guide for nRF9160DK and Thingy:91

Because NCS uses different Zephyr version, it is necessary to change our Zephyr workspace, it is handled by using different manifest file.
Set West manifest path to `Anjay-zephyr-client/minimal`, and manifest file to `west-nrf.yml` and do `west update`.
```
west config manifest.path Anjay-zephyr-client/minimal
west config manifest.file west-nrf.yml
west update
```
Now you can compile the project using `west build -b nrf9160dk_nrf9160ns` or `west build -b thingy91_nrf9160ns` in `minimal` directory, respectively.

> **__NOTE:__**
> To switch back to mainstream zephyr version, change manifest file to `west.yml` and do `west update`.
> ```
> west config manifest.file west.yml
> west update
> ```
> [More about managing west workspace through manifests can be found here.](https://docs.zephyrproject.org/latest/guides/west/manifest.html)

## Flashing the target

After successful build you can flash the target using `west flash`.

> **__NOTE (Thingy:91):__**
> Thingy:91 doesn't have an onboard debug probe (and MCUBoot is not supported), so you must use an external J-link programmer with support for Arm Cortex-M33 devices.
> 
> Make sure that SWD SELECT (SW2) switch is set to NRF91, connect the Thingy:91 to a programmer and run `west flash`.

## qemu_x86 networking setup

To enable networking when running qemu_x86 target and be able to connect to the internet,
there is an [official guide on networking with the host system](https://docs.zephyrproject.org/latest/guides/networking/networking_with_host.html). Below is a shorter version on how to run the example.

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
LwM2M Server, please register at https://www.avsystem.com/try-anjay/. Then have
a look at the Configuration menu using `west build -t guiconfig` or `west build -t menuconfig` to configure security credentials and other
necessary settings (like Wi-Fi SSID etc.).

[Video guide showing basic usage of Try-Anjay](https://www.youtube.com/watchv=fgy38XfttM8)
is available on YouTube.

NOTE: You may use any LwM2M Server compliant with LwM2M 1.0 TS. The server URI
can be changed in the Configuration menu.
