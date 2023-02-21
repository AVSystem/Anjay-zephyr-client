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
        Showcase example implementing all features and sensors available on board. Supports experimental <strong>factory provisioning</strong> feature of Anjay 3.0. Supported boards:<br>
        [B-L475E-IOT01A Discovery kit](https://www.st.com/en/evaluation-tools/b-l475e-iot01a.html)<br>
        [nRF9160 Development kit](https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF9160-DK)<br>
        [Nordic Thingy:91 Prototyping kit](https://www.nordicsemi.com/Products/Development-hardware/Nordic-Thingy-91)<br>
        [ESP32-DevKitC](https://www.espressif.com/en/products/devkits/esp32-devkitc)<br>
        [Arduino Nano 33 BLE Sense Lite](https://store.arduino.cc/products/arduino-nano-33-ble-sense)<br>
        any other board of your choice (by adding appropriate <code>*.conf</code>/<code>*.overlay</code> files). FOTA support for selected boards.
      </td>
    </tr>
    <tr>
      <td>minimal/</td>
      <td>
        Minimalistic client samples which may serve as a base for specific application. Supported boards/targets:<br>
        [qemu_x86](https://docs.zephyrproject.org/latest/boards/x86/qemu_x86/doc/index.html)<br>
        [B-L475E-IOT01A Discovery kit](https://www.st.com/en/evaluation-tools/b-l475e-iot01a.html)<br>
        [nRF9160 Development kit](https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF9160-DK)<br>
        [Nordic Thingy:91 Prototyping kit](https://www.nordicsemi.com/Products/Development-hardware/Nordic-Thingy-91)<br>
        [ESP32-DevKitC](https://www.espressif.com/en/products/devkits/esp32-devkitc)<br>
        [Arduino Nano 33 BLE Sense Lite](https://store.arduino.cc/products/arduino-nano-33-ble-sense)<br>
      </td>
    </tr>
    <tr>
      <td>ei_demo/</td>
      <td>
        An example containing motion recognition model, built with Edge Impulse. Supported boards/targets:<br>
        [Nordic Thingy:91 Prototyping kit](https://www.nordicsemi.com/Products/Development-hardware/Nordic-Thingy-91)
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

If you want to compile for nRF9160 targets, additional GNU Arm Embedded Toolchain is necessary, to install it follow
[installing the toolchain for NCS guide](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/gs_installing.html#install-the-toolchain). Also for flashing [nRF Command Line Tools](https://www.nordicsemi.com/Products/Development-tools/nrf-command-line-tools) have to be installed.

For further instructions, check `README.md` in the folder with the example in which you are interested.

### Setting up Visual Studio Code (for nRF9160 targets)

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
configuration for the desired target, e.g. `nrf9160dk_nrf9160_ns` (you can have
multiple configurations for different targets). Choose your target and click
"Build Configuration" in order to build the application. Next builds can be
started by choosing Actions -> Build.

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
