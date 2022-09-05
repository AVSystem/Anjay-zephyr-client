# Changelog

## 22.12 (Dec 13th, 2022)

### Features
- Added provisioning script for `demo` examples
- Added nRF52840dk_nRF52840 with OpenThread support
- Added automatic handling of connection link state
- Added option for use of non-secure WiFi networks
- Added configuration for software-based security on nRF9160DK

### Improvements
- Updated Zephyr to 3.2.0
- Updated sdk-nrf to 2.1.1
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
