# Anjay Demo [<img align="right" height="50px" src="https://avsystem.github.io/Anjay-doc/_images/avsystem_logo.png">](http://www.avsystem.com/)

Project containing all implemented features, intended to be a showcase.
## Supported hardware and overview

This folder contains LwM2M Client application example, which targets
[B-L475E-IOT01A Discovery kit](https://www.st.com/en/evaluation-tools/b-l475e-iot01a.html), [nRF9160 Development kit](https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF9160-DK) and [Nordic Thingy:91 Prototyping kit](https://www.nordicsemi.com/Products/Development-hardware/Nordic-Thingy-91).

It's possible to run the demo on other boards of your choice, by adding appropriate configuration files and aliases for available sensors/peripherals (more info below).

The following LwM2M Objects are supported:

| Target         | Objects                                                                                                                                                |
|----------------|--------------------------------------------------------------------------------------------------------------------------------------------------------|
| Common         | Security (/0)<br>Server (/1)<br>Device (/3)<br>Push button (/3347)                                                                                     |
| B-L475E-IOT01A | Temperature (/3303)<br>Humidity (/3304)<br>Accelerometer (/3313)<br>Magnetometer (/3314)<br>Barometer (/3315)<br>Distance (/3330)<br>Gyrometer (/3334) |
| nRF9160DK      | Location (/6)<br>On/Off switch (/3342)                                                                                                                 |
| Thingy:91      | Location (/6)<br>Temperature (/3303)<br>Humidity (/3304)<br>Accelerometer (/3313)<br>Barometer (/3315)<br>Buzzer (/3338)<br>LED color light (/3420)    |

## Compilation

Set West manifest path to `Anjay-zephyr-client/demo`, and manifest file to `west.yml` and do `west update`.
```
west config manifest.path Anjay-zephyr-client/demo
west config manifest.file west.yml
west update
```

You can now compile the project for B-L475E-IOT01A using `west build -b disco_l475_iot1` in `demo` directory.

### Compilation guide for nRF9160DK and Thingy:91

Because NCS uses different Zephyr version, it is necessary to change our Zephyr workspace, it is handled by using different manifest file.
Set West manifest path to `Anjay-zephyr-client/demo`, and manifest file to `west-nrf.yml` and do `west update`.
```
west config manifest.path Anjay-zephyr-client/demo
west config manifest.file west-nrf.yml
west update
```
Now you can compile the project using `west build -b nrf9160dk_nrf9160ns` or `west build -b thingy91_nrf9160ns` in `demo` directory, respectively.

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

Additionally, you can define `status-led` alias for a LED, which blinks when Anjay is running.

## Connecting to the LwM2M Server

To connect to [Coiote IoT Device
Management](https://www.avsystem.com/products/coiote-iot-device-management-platform/)
LwM2M Server, please register at https://www.avsystem.com/try-anjay/. Then have
a look at the Configuration menu to configure security credentials and other
necessary settings (like Wi-Fi SSID etc.).

[Video guide showing basic usage of Try-Anjay](https://www.youtube.com/watch?v=fgy38XfttM8)
is available on YouTube.

NOTE: You may use any LwM2M Server compliant with LwM2M 1.0 TS. The server URI
can be changed in the Configuration menu.

## Configuration menu

Using serial port terminal, you can manage Anjay client using built-in Zephyr shell. Use `anjay` command to list possible options.

```
uart:~$ anjay
anjay - Anjay commands
Subcommands:
  start   :Save config and start Anjay
  stop    :Stop Anjay
  config  :Configure Anjay params
```
