# Anjay Minimal Client [<img align="right" height="50px" src="https://avsystem.github.io/Anjay-doc/_images/avsystem_logo.png">](http://www.avsystem.com/)

Project with minimal features needed to run Anjay on zephyr.
It should serve as a template for custom projects.
## Supported hardware and overview

This folder contains LwM2M Client minimal application example for following targets:
 - [qemu_x86](https://docs.zephyrproject.org/latest/boards/x86/qemu_x86/doc/index.html)

The following LwM2M Objects are supported:
 - Security (/0)
 - Server (/1)
 - Access Control (/2)
 - Device (/3)

## Compilation

Set West manifest path to `Anjay-zephyr-client/minimal`, and manifest file to `west.yml` and do `west update`.
```
west config manifest.path Anjay-zephyr-client/minimal
west config manifest.file west.yml
west update
```

You can now compile the project for qemu_x86 using `west build -b qemu_x86` in `minimal` directory.


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
