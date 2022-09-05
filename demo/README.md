# Anjay Demo [<img align="right" height="50px" src="https://avsystem.github.io/Anjay-doc/_images/avsystem_logo.png">](http://www.avsystem.com/)

Project containing all implemented features, intended to be a showcase.
## Supported hardware and overview

This folder contains LwM2M Client application example, which targets
[B-L475E-IOT01A Discovery kit](https://www.st.com/en/evaluation-tools/b-l475e-iot01a.html), [nRF9160 Development kit](https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF9160-DK), [Nordic Thingy:91 Prototyping kit](https://www.nordicsemi.com/Products/Development-hardware/Nordic-Thingy-91), [ESP32-DevKitC](https://www.espressif.com/en/products/devkits/esp32-devkitc) and [nRF52840 Development kit](https://www.nordicsemi.com/Products/Development-hardware/nrf52840-dk).

There's an alternative configuration for nRF9160DK, revisions 0.14.0 and up, which utilizes external flash chip to perform firmware updates.

It's possible to run the demo on other boards of your choice, by adding appropriate configuration files and aliases for available sensors/peripherals (more info below).

The following LwM2M Objects are supported:

| Target         | Objects                                                                                                                                                                                                                                                                                                                                                       |
|----------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Common         | Security (/0)<br>Server (/1)<br>Device (/3)                                                                                                                                                                                                                                                                                                                   |
| B-L475E-IOT01A | **Firmware Update (/5)** (experimental)<br>Temperature (/3303)<br>Humidity (/3304)<br>Accelerometer (/3313)<br>Magnetometer (/3314)<br>Barometer (/3315)<br>Distance (/3330)<br>Gyrometer (/3334)<br>Push button (/3347)                                                                                                                                      |
| nRF9160DK      | Connectivity Monitoring (/4)<br>**Firmware Update (/5)**<br>Location (/6, configurable in Kconfig)<br>On/Off switch (/3342)<br>Push button (/3347)<br>ECID-Signal Measurement Information (/10256)<br>Location Assistance (/50001, experimental)                                                                                                              |
| Thingy:91      | Connectivity Monitoring (/4)<br>**Firmware Update (/5)**<br>Location (/6, configurable in Kconfig)<br>Temperature (/3303)<br>Humidity (/3304)<br>Accelerometer (/3313)<br>Barometer (/3315)<br>Buzzer (/3338)<br>Push button (/3347)<br>LED color light (/3420)<br>ECID-Signal Measurement Information (/10256)<br>Location Assistance (/50001, experimental) |
| nRF52840DK     | Push button (/3347)                                                                                                                                                                                                                                                                                                                                           |

## Compilation

Set West manifest path to `Anjay-zephyr-client/demo`, and manifest file to `west.yml` and do `west update`.
```
west config manifest.path Anjay-zephyr-client/demo
west config manifest.file west.yml
west update
```

You can now compile the project for B-L475E-IOT01A using `west build -b disco_l475_iot1` in `demo` directory.

### Compilation guide for nRF9160DK, Thingy:91 and nRF52840DK

Because NCS uses different Zephyr version, it is necessary to change our Zephyr workspace, it is handled by using different manifest file.
Set West manifest path to `Anjay-zephyr-client/demo`, and manifest file to `west-nrf.yml` and do `west update`.
```
west config manifest.path Anjay-zephyr-client/demo
west config manifest.file west-nrf.yml
west update
```
Now you can compile the project using `west build -b nrf9160dk_nrf9160_ns`, `west build -b thingy91_nrf9160_ns` or `west build -b nrf52840dk_nrf52840` in `demo` directory, respectively. The last command compiles project for use with the OpenThread network, more about this can be found in the section `Connecting to the LwM2M Server with OpenThread`.


> **__NOTE:__**
> To switch back to mainstream zephyr version, change manifest file to `west.yml` and do `west update`.
> ```
> west config manifest.file west.yml
> west update
> ```
> [More about managing west workspace through manifests can be found here.](https://docs.zephyrproject.org/latest/guides/west/manifest.html)


### Compiling for external flash usage

For nRF9160DK hardware revisions 0.14.0 and up, an alternate configuration that puts the MCUboot secondary partition on the external flash instead of dividing the internal flash space is available.

To compile in this configuration, use `west build -b nrf9160dk_nrf9160_ns@0.14.0 -- -DCONF_FILE=prj_extflash.conf`.

### Compiling with software-based cryptography

On Nordic boards, security is provided using the (D)TLS sockets implemented in modem firmware and provided by nrfxlib.

However, on nRF9160DK revisions 0.14.0 and up, it is possible to switch to software-based implementation based on Mbed TLS instead. This is not recommended due to lowered security and performance, but may be desirable if you require some specific (D)TLS features (e.g. ciphersuites) that are not supported by the modem.

To compile in this configuration, use `west build -b nrf9160dk_nrf9160_ns@0.14.0 -- -DCONF_FILE=prj_extflash.conf -DOVERLAY_CONFIG=overlay_nrf_mbedtls.conf`.

## Flashing the target

After successful build you can flash the target using `west flash`.

> **__NOTE:__**
> The nRF91 targets require the modem firmware to be upgraded to version 1.3.1 or later. See the [Updating the modem firmware](https://infocenter.nordicsemi.com/index.jsp?topic=%2Fnan_041%2FAPP%2Fnan_production_programming%2Fmodem_update.html) article for more information.
>
> Alternatively, you can remove the ``udp_dtls_hs_tx_params`` assignment in the ``initialize_anjay()`` function in ``main.c`` to make it work on older versions. However, the ``anjay stop`` command may then take an excessive amount of time. Please also note that the application has not been tested with older modem firmware versions.

## Running the demo on other boards

To run the demo on another board not listed above, create `boards/<board_symbol>.conf` and `boards/<board_symbol>.overlay` files. Then, in `.conf` file supply `CONFIG_ANJAY_CLIENT_DEVICE_MANUFACTURER` and `CONFIG_ANJAY_CLIENT_MODEL_NUMBER` settings, to make your device show up correctly in Device (/3) object. You should also add other settings enabling connectivity, peripherals, etc.

If you wish to connect the available sensors on your board to the LwM2M client, you must add appropriate labels and aliases in `.overlay` file ([official Zephyr HOWTO on devicetree overlays](https://docs.zephyrproject.org/latest/guides/dts/howtos.html#use-devicetree-overlays) might be helpful).

The build system is able to automatically determine whether an alias to a peripheral is defined, and is able to compile in the required code.

Currently, our demo searches for aliases listed below (they map to appropriate LwM2M objects):
- `temperature`
- `humidity`
- `distance`
- `barometer`
- `accelerometer`
- `gyrometer`
- `magnetometer`
- `rgb_pwm`
- `buzzer_pwm`
- `push-button-[0-2]`
- `switch-[0-2]`
- `illuminance`

Additionally, you can define `status-led` alias for a LED, which blinks when Anjay is running.

## Connecting to the LwM2M Server

To connect to [Coiote IoT Device
Management](https://www.avsystem.com/products/coiote-iot-device-management-platform/)
LwM2M Server, please register at https://eu.iot.avsystem.cloud/. Then have
a look at the Configuration menu to configure security credentials and other
necessary settings (like Wi-Fi SSID etc.).

[Guide showing basic usage of Coiote DM](https://iotdevzone.avsystem.com/docs/IoT_quick_start/Device_onboarding/)
is available on IoT Developer Zone.

NOTE: You may use any LwM2M Server compliant with LwM2M 1.0 TS. The server URI
can be changed in the Configuration menu.

## Connecting to the LwM2M Server with OpenThread

To use this project on the nRF52840dk board, in addition to the configuration shown in the previous paragraph, you will need to configure the OpenThread Border Router and Commissioner as described in the guides from the links below.
You can change default `CONFIG_OPENTHREAD_JOINER_PSKD` value in the `boards/nrf52840dk_nrf52840.conf`. In same file, replace `CONFIG_OPENTHREAD_FTD=y` with `CONFIG_OPENTHREAD_MTD=y` if you want your device to run as an MTD.

Resources:
- [Introduction to OpenThread](https://openthread.io/guides)
- [Border Router guide](https://openthread.io/guides/border-router)
- [Commissioner guide](https://openthread.io/guides/commissioner)

## Configuration menu

Using serial port terminal, you can manage Anjay client using built-in Zephyr shell. Use `anjay` command to list possible options.

```
uart:~$ anjay
anjay - Anjay commands
Subcommands:
  start                :Save config and start Anjay
  stop                 :Stop Anjay
  config               :Configure Anjay params
  session_cache_purge  :Remove the TLS session data cached in the nRF modem
```

## Tips and tricks

1. The TLS session cache in the nRF modem is enabled by default in anjay-zephyr.
   It might cause some issues when trying to connect to a server with changed
   credentials. To solve the issue, use `anjay session_cache_purge` shell
   command which cleans up the cache, so the new credentials will be used
   for the next connection.

## Upgrading the firmware over-the-air

To upgrade the firmware, upload the proper image using standard means of LwM2M Firmware Update object.

The image to use is:

* for Nordic boards: `build/zephyr/app_update.bin`
* for other boards: `build/zephyr/zephyr.signed.bin`

## Factory provisioning (experimental)

This application supports experimental factory provisioning feature of Anjay 3.0, thanks
to which the user is able to pre-provision credentials to the device using a specially
tailored version of the application. This feature allows to easily pre-provision large
quantities of devices in a semi-automatic manner.

To use this feature, one can use a script `tools/provisioning-tools/ptool.py`.
It might be used in the similar manner as the script of the same name described in the documentation:
[Factory Provisioning Tool](https://avsystem.github.io/Anjay-doc/Tools/FactoryProvisioning.html).
There are a few new and important command-line arguments:

* `--board` (`-b`) - the board for which the images should be built,
* `--image_dir` (`-i`) - directory for the cached Zephyr hex images,
* `--serial` (`-s`) - serial number of the device to be used,
* `--baudrate` (`-B`) - baudrate for the used serial port, when it is not provided the default value is 115200.

If the image `initial.hex` exists in the given `image_dir` the initial provisioning image won't be built and the same works for
final image and `final.hex`. When `image_dir` path is provided, but some images are missing, they will be built in the given directory.
If `image_dir` is not provided then the images will be built in `$(pwd)/provisioning_builds`.

Before using the script make sure that in the shell in which you run it the `west build` command would work and
that all of the configs passed to the script are valid - in particular, make sure that you changed `<YOUR_DOMAIN>` in `lwm2m_server.json`
config file to your actual domain in EU cloud Coiote installation (or fill the whole file with some different valid server configuration).

Currently the script is designed only for Nordic boards, and it was tested with nRF 9160DK.

Example script invocation from the `demo` for provisioning some nRF 9160DK board directory may look like:

```bash
../tools/provisioning-tool/ptool.py -b nrf9160dk_nrf9160_ns -s <SERIAL> -c ../tools/provisioning-tool/configs/endpoint_cfg -t <TOKEN> -S ../tools/provisioning-tool/configs/lwm2m_server.json
```

where `<SERIAL>` should be replaced by our board's serial number and `<TOKEN>` should be replaced by some valid authentication token for the Coiote server provided in the `lwm2m_server.json` file.

### Using Certificate Mode with factory provisioning

If supported by the underlying (D)TLS backend (if using Mbed TLS, make sure that
it is configured appropriately), the application can authenticate with the
server using certificate mode.

You will need to download the server certificate first. One possible way to do
it is with `openssl`:

```bash
openssl s_client -showcerts -connect eu.iot.avsystem.cloud:5684 | openssl x509 -outform der -out eu-cloud-cert.der
```

> **__NOTE:__**
> Only servers that use self-signed certificates are reliably supported by
> default. You can change this behavior by setting the Certificate Usage
> resource in the endpoint configuration file. However, this might not be
> supported by all (D)TLS backends.
>
> In particular, when `CONFIG_ANJAY_COMPAT_ZEPHYR_TLS` is enabled (which is the
> default for Nordic boards), the Certificate Usage are only approximated by
> adding the server certificate to traditional PKIX trust store if Certificate
> Usage is set to 2 or 3 (note that 3 is the default) and ignoring it otherwise.

You should then modify the `cert_info.json` file that's located in
`tools/provisioning-tool/configs` for the desired self-signed certificate
configuration.

Once you have the server certificate, you can now provision the board. Example
script invocation may look like:

```bash
../tools/provisioning-tool/ptool.py -b nrf9160dk_nrf9160_ns -s <SERIAL> -c ../tools/provisioning-tool/configs/endpoint_cfg_cert -p eu-cloud-cert.der -C ../tools/provisioning-tool/configs/cert_info.json
```

> **__NOTE:__**
> Coiote DM currently does not support registering devices together with
> uploading dynamically generated self-signed certificates using command-line
> tools.
>
> You will need to manually add the new device on Coiote DM via GUI and upload
> the certificate during the "Add device credentials" step.
