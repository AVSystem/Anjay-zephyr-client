# Changelog

## 25.02 (February 28th, 2024)

### Features
- Added nRF9151DK board as a target for demo sample

### Improvements
- Updated Anjay-zephyr to 3.9.0

## 24.11 (November 13th, 2024)

### Features
- Added FOTA support for nRF7002DK board

### Improvements
- Updated Zephyr to 3.6.0
- Updated sdk-nrf to 2.7.0
- Updated Anjay-zephyr to 3.8.1
- Added instruction for updating es-WiFi module for B-L475E-IOT01A Discovery kit

## 24.05 (May 28th, 2024)

### Improvements
- Adjusted `nrf9160dk_nrf9160_ns` build to meet new default target
- Reworked build system to use `sysbuild` for FOTA builds on upstream Zephyr and T-Mobile SDKs

## 24.02 (February 16th, 2024)

### Improvements
- Changed C stdlib to Picolibc (Zephyr's default) for most projects
- Added TF-M support for nRF7002 DK board (changed the board variant from `nrf7002dk_nrf5340_cpuapp` to `nrf7002dk_nrf5340_cpuapp_ns`)
- Added new parameter to specify log virtual port for factory provisioning script
- Updated Zephyr SDK to 0.16.4

## 23.11 (November 21st, 2023)

### Improvements
- Updated Zephyr to 3.5.0
- Updated sdk-nrf to 2.5.0
- Adjusted Kconfig options to make projects build on newest Zephyr and nCS versions

## 23.09.1 (September 22nd, 2023)

### Bugfixes
- Fix Anjay-zephyr revision in west*.yml files

## 23.09 (September 7th, 2023)

### Features
- Added Bubblemaker app
- Added tmo_dev_edge board as a target for demo and minimal samples with the possibility to switch
  preferred network bearer and perform firmware update using the external flash
- Added support for Location object in `asset_tracker` application

## 23.06 (June 23rd, 2023)

### Features
- Added support for nRF7002DK board

### Improvements
- Updated Zephyr to 3.3.0
- Updated sdk-nrf to 2.3.0
- Updated Anjay-zephyr to 3.4.1
- Adjusted partition layout and MCUboot configuration on nRF9160DK builds to
  support the newer version of sdk-nrf
- Added overlay for nRF9160DK in demo application which enables experimental Advanced Firmware Update object (/33629)

### Bugfixes
- Fixed firmware update on Thingy:91 - made sure that enough free space is left
  on the flash partition for MCUboot's move algorithm to work
- Fixed runtime certificate configuration on nRF52840 based boards

## 23.02 (Feb 21st, 2023)

### Features
- Added support for arduino_nano_33_ble_sense board
- Added runtime certificate configuration option
- Added possibility to set PSK in hexadecimal format

### Improvements
- Moved common code from demo samples to Anjay Zephyr
- Updated sdk-nrf to final 2.2.0
- Disabled Access Control in the minimal project

### Bugfixes
- Made firmware update with external flash on nRF9160DK work again

## 22.12 (Dec 13th, 2022)

### Features
- Added provisioning script for `demo` examples
- Added nRF52840dk_nRF52840 with OpenThread support
- Added automatic handling of connection link state
- Added option for use of non-secure WiFi networks
- Added configuration for software-based security on nRF9160DK

### Improvements
- Updated Zephyr to 3.2.0
- Updated sdk-nrf to 2.2.0-rc1
- Refactored network connection handling
- Firmware update success status can be now persisted across reboots until actual delivery

### Bugfixes
- Fixed critical memory corruption bug in the factory provisioning app
- Fixed problem with push buttons IID

## 22.08.1 (Aug 31st, 2022)

### Features
- Added ESP32-DevKitC board support to `demo` and `minimal` examples
- Experimental support for firmware update on the L475 board
- Added GPS and assisted location to Thingy:91 and nRF9160dk targets as default
- Turned off Thread Analyzer and TCP support by default.

### Improvements
- Deferred network initialization so that network settings can actually be changed if connection fails
- Updated Anjay-zephyr to 3.1.2

### Bugfixes

## 22.08 (Aug 5th, 2022)

### Features
- Added support for factory provisioning
- Added `nls_agps_req` shell command for Nordic Location Services A-GPS testing purposes

### Improvements
- Updated Anjay-zephyr to 3.1.1
- Updated static partition definitions
- Improved logging in Location Assistance object
- Cleaned up use of logging statements
- Separated factory provisioning-related code from the main application

### Bugfixes
- Enabled full printf to work with Zephyr SDK 0.14.2
- Fixed PWM code for Thingy:91
- LwM2M 1.1 is not enforced anymore when using persistence
- Fixed memleak in persist_target_to_settings()

## 22.06 (Jun 21st, 2022)

### Improvements
- Updated Anjay-zephyr to 3.0.0
- Reformatted code so it meets Zephyr standards

### Features
- Implemented Location Assistance object
- Implemented Connectivity Monitoring object
- Added shell command to enable runtime lifetime reconfiguration
- Added persistence implementation

## 22.03 (Mar 24th, 2022)

### Features
- Added support for firmware update

### Improvements
- Made GPS support configurable through Kconfig
- Migrated nRF targets to use nrfxlib sockets
- Migrated configuration to use Zephyr settings API
- Updated Anjay-zephyr to 2.14.1
- Updated Zephyr to 2.7.0
- Updated sdk-nrf to 1.9.1
- Updated ei_demo to use the Edge Impulse wrapper from sdk-nrf

### Bugfixes
- Made the code more thread-safe
- Fixed erroneous uses of memset()

## 21.10 (Oct 11th, 2021)

### Features
- Edge Impulse example
- L475 support in minimal client
- nRF9160 DK support in minimal client
- Thingy:91 support in minimal client

### Improvements
- Cleaner code using Anjay event loop
- Simpler demo implementation using Anjay IPSO objects
- Updated Anjay-zephyr to 2.14.0

### Bugfixes
- Fixed global names

## 21.07 (Jul 30th, 2021)

### Features
- Moved client containing all features to demo folder
- Minimal client example for qemu_x86
- Support for Thingy:91 in demo application
- Location object
- Buzzer object

### Improvements
- Changed regulatory domain on eS-WiFi modem to World Wide
- Sensor objects implementation depends on sensor presence, rather than
  board name.
- Updated Anjay-zephyr to 2.13.0
- Updated Zephyr to 2.6.0
- Updated sdk-nrf to 1.6.0

## 21.06 (Jun 7th, 2021)

### Improvements
- Use Anjay-zephyr 2.11.1 integration layer
- Use Zephyr v2.5.0
- Use Zephyr logger as log handler

## 20.10 (Oct 1st, 2020)

### Features
- Add initial support for nRF9160DK board

### Bugfixes
- Fix assertion failure when invalid URI is passed
- Fix problem with invalid file state after write

## 20.07 (Jul 7th, 2020)

### Features
- Initial release
