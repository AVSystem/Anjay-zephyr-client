# Anjay-zephyr-client [<img align="right" height="50px" src="https://avsystem.github.io/Anjay-doc/_images/avsystem_logo.png">](http://www.avsystem.com/)

## Overview

This repository contains LwM2M Client samples based on open-source
[Anjay Zephyr Module](https://github.com/AVSystem/Anjay-zephyr) and
[Zephyr OS](https://github.com/zephyrproject-rtos/zephyr).

More information and step-by-step instruction can be found in [the Anjay integration IoT Developer Zone](https://iotdevzone.avsystem.com/docs/Anjay_integrations/Getting_started/).

The following examples are present:

<table>
  <thead>
    <tr>
      <th>Directory</th>
      <th>Description</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>demo/</td>
      <td>
        Showcase example implementing all features and sensors available on board. Supports experimental <strong>factory provisioning</strong> feature of Anjay 3. Supported boards:<br>
        <a href="https://www.st.com/en/evaluation-tools/b-l475e-iot01a.html">B-L475E-IOT01A Discovery kit</a><br>
        <a href="https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF9160-DK">nRF9160 Development kit</a><br>
        <a href="https://www.nordicsemi.com/Products/Development-hardware/Nordic-Thingy-91">Nordic Thingy:91 Prototyping kit</a><br>
        <a href="https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF9151-DK">nRF9151 Development kit</a><br>
        <a href="https://www.espressif.com/en/products/devkits/esp32-devkitc">ESP32-DevKitC</a><br>
        <a href="https://www.nordicsemi.com/Products/Development-hardware/nrf52840-dk">nRF52840 Development kit</a><br>
        <a href="https://www.nordicsemi.com/Products/Development-hardware/nRF7002-DK">nRF7002 Development kit</a><br>
        <a href="https://store.arduino.cc/products/arduino-nano-33-ble-sense">Arduino Nano 33 BLE Sense Lite</a><br>
        <a href="https://devedge.t-mobile.com/solutions/iotdevkit">DevEdge</a><br>
        any other board of your choice (by adding appropriate <code>*.conf</code>/<code>*.overlay</code> files). FOTA support for selected boards.
      </td>
    </tr>
    <tr>
      <td>minimal/</td>
      <td>
        Minimalistic client samples which may serve as a base for specific application. Supported boards/targets:<br>
        <a href="https://docs.zephyrproject.org/latest/boards/x86/qemu_x86/doc/index.html">qemu_x86</a><br>
        <a href="https://www.st.com/en/evaluation-tools/b-l475e-iot01a.html">B-L475E-IOT01A Discovery kit</a><br>
        <a href="https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF9160-DK">nRF9160 Development kit</a><br>
        <a href="https://www.nordicsemi.com/Products/Development-hardware/Nordic-Thingy-91">Nordic Thingy:91 Prototyping kit</a><br>
        <a href="https://www.espressif.com/en/products/devkits/esp32-devkitc">ESP32-DevKitC</a><br>
        <a href="https://www.nordicsemi.com/Products/Development-hardware/nrf52840-dk">nRF52840 Development kit</a><br>
        <a href="https://store.arduino.cc/products/arduino-nano-33-ble-sense">Arduino Nano 33 BLE Sense Lite</a><br>
        <a href="https://devedge.t-mobile.com/solutions/iotdevkit">DevEdge</a><br>
      </td>
    </tr>
    <tr>
      <td>ei_demo/</td>
      <td>
        An example containing motion recognition model, built with Edge Impulse. Supported boards/targets:<br>
        <a href="https://www.nordicsemi.com/Products/Development-hardware/Nordic-Thingy-91">Nordic Thingy:91 Prototyping kit</a>
      </td>
    </tr>
    <tr>
      <td>bubblemaker/</td>
      <td>
        An interactive example containing integration of water meters and different sensors with LwM2M protocol.  Supported boards:<br>
        <a href="https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF9160-DK">nRF9160 Development kit</a><br>
        <a href="https://www.nordicsemi.com/Products/Development-hardware/nRF7002-DK">nRF7002 Development kit</a><br>
      </td>
    </tr>
  </tbody>
</table>

## Getting started

First of all, get Zephyr, SDK and other dependencies, as described in Zephyr's
[Getting Started Guide](https://docs.zephyrproject.org/latest/getting_started/index.html)
(first 4 steps).

After navigating to Zephyr workspace (`~/zephyrproject` is default after following Getting Started Guide), clone Anjay Zephyr client repository.
```
git clone https://github.com/AVSystem/Anjay-zephyr-client
```

If you want to compile for nRF91 targets, additional GNU Arm Embedded Toolchain is necessary, to install it follow
[installing the toolchain for NCS guide](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/gs_installing.html#install-the-toolchain). Also for flashing [nRF Command Line Tools](https://www.nordicsemi.com/Products/Development-tools/nrf-command-line-tools) have to be installed.

For further instructions, check `README.md` in the folder with the example in which you are interested.

### Setting up Visual Studio Code (for nRF91 targets)

The nRF Connect SDK has a set of plugins for Visual Studio Code (VSC). They can
be used to simplify the build process for the nRF boards. To do that we need to
set up VSC first.

> **__NOTE:__**
> On a Linux system, first make sure that `GNUARMEMB_TOOLCHAIN_PATH` variable is
> set in the scope that Visual Studio Code is run.
>
> You can set this permanently by adding a line similar to the following
> (adjusted to your toolchain installation directory) to your `~/.profile` file
> (or `~/.bash_profile` or `~/.zprofile` etc., depending on your shell and
> preferences):
>
> ```
> export GNUARMEMB_TOOLCHAIN_PATH="$HOME/gcc-arm-none-eabi-10.3-2021.10"
> ```

Open VSC and, on the Extensions tab, install the "nRF Connect for VS Code"
plugin or the "nRF Connect Extension Pack" (which includes the former). After
the installation, the "Welcome to nRF Connect" window should appear. In case it
doesn't, you can open it from the nRF Connect tab -> Welcome -> Open Welcome
page. Set nRF Connect SDK to the folder where `west` initialized Zephyr
(`~/zephyrproject` is the default suggested in the Getting Started Guide). The
"nRF Connect Toolchain" setting should point to the gnuarmemb toolchain
installation folder, or in case of Linux, it should be set as "PATH" (it will
use the setting from `GNUARMEMB_TOOLCHAIN_PATH` to find it).

If the environment was not set up earlier, go to the Source Control tab, open
the Terminal tab (<code>Ctrl+Shift+&grave;</code>). In case it opens in a wrong
directory, navigate to where you have initialized Zephyr and its dependencies
(e.g. `~/zephyrproject`). Then run:

```
west config manifest.path Anjay-zephyr-client/demo
west config manifest.file west-nrf.yml
west update
```

> **__NOTE:__**
> If building a project other than `demo`, please adjust the path in the above
> commands accordingly.

> **__NOTE:__**
> West manifest folder can be changed in Source Control by clicking "Select
> manifest" on the root of the Zephyr repository and choosing a different
> folder. If the manifest is different a "west update" icon will show up next to
> the manifest file name. Clicking it will update the repository. If the
> manifest you want to change to has a different name (and not only a different
> path) then it must be changed directly using the `west config` command in the
> terminal.
>
> This is why using the command line is necessary to choose the manifest for
> this project - the nRF-specific manifests are called `west-nrf.yml`, which is
> not supported by the plugin's GUI at the moment.

Now we can add the application to VSC workspace. Click on nRF Connect tab ->
Application -> Add Application. Open the folder containing the `prj.conf` file
(e.g. `Anjay-zephyr-client/demo`). In the Application section the demo
application should show up. Click on "No build configuration" to add a
configuration for the desired target, e.g. `nrf9160dk/nrf9160/ns` (you can have
multiple configurations for different targets). Choose your target and click
"Build Configuration" in order to build the application. Next builds can be
started by choosing Actions -> Build.

### Enabling GPS implementation (for nRF91 targets)

Anjay-zephyr-client has the support for GPS implementation for nRF91 targets
which can be enabled using `Kconfig` options. Some of the available options are:
- `CONFIG_ANJAY_ZEPHYR_GPS` - enables/disables support for GPS
- `CONFIG_ANJAY_ZEPHYR_GPS_NRF_A_GPS` - enables/disables A-GPS using Nordic
Location Services over LwM2M
- `CONFIG_ANJAY_ZEPHYR_GPS_NRF_PRIO_MODE_PERMITTED` - enables/disables support
for allowing temporary activation of the GPS priority mode
  - if set, Anjay Zephyr will temporarily activate the GPS priority over LTE
  idle mode **which shuts down LTE completely** and attempts to acquire the fix
  in case GPS fix cannot be produced
  - by default, this option is disabled and can be enabled using `Kconfig` or
  shell commands (`anjay config set gps_prio_mode_permitted`)
- `CONFIG_ANJAY_ZEPHYR_GPS_NRF_PRIO_MODE_COOLDOWN` - determines (in seconds) how
much time must pass after a failed try to produce a GPS fix to enable GPS
priority mode again

## Connecting to the LwM2M Server

To connect to [Coiote IoT Device
Management](https://www.avsystem.com/products/coiote-iot-device-management-platform/)
LwM2M Server, please register at https://eu.iot.avsystem.cloud/. Then have
a look at the example configuration to configure security credentials and other
necessary settings (like Wi-Fi SSID etc.).

[Guide showing basic usage of Coiote DM](https://iotdevzone.avsystem.com/docs/IoT_quick_start/Device_onboarding/)
is available on IoT Developer Zone.

> **__NOTE:__**
> You may use any LwM2M Server compliant with LwM2M 1.0 TS. The server URI can
> be changed in the example configuration options.

## Updating es-WiFi module firmware on B-L475E-IOT01A Discovery kit to make FOTA work

There are known issues with the older es-WiFi firmware, particularly in handling
multiple socket connections, which manifest during pull FOTA operations. To get
rid of these problems, a WiFi module firmware update is required.

Please download `Inventek ISM 43362 Wi-Fi module firmware update` package from
[ST website](https://www.st.com/en/evaluation-tools/b-l475e-iot01a.html#tools-software).
If you are using Windows you can simple follow instructions from `readme.txt`.
For Linux, please install the appropriate version of
[STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html).
Then follow steps 2 through 5 in `readme.txt`. Next, go to the
`en.inventek_fw_updater.2.0/bin` directory, but do not run the `update_Wifi.bat`
script. Instead, run (please update the path to `STM32_Programmer_CLI` and
`/dev/ttyACM0` device port if necessary):

```bash
~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI -c port=SWD -e all -d ./InventekBootloaderPassthrough.bin 0x08000000 -v -hardrst
sleep 10
~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI -c port=/dev/ttyACM0 br=115200 p=EVEN db=8 sb=1 fc=OFF -e all
~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI -c port=/dev/ttyACM0 br=115200 p=even db=8 sb=1 fc=OFF -d ./ISM43362_M3G_L44_SPI_C3.5.2.7.STM.bin 0x08000000 -v
```
