# Changelog

## 22.03 (Mar 24th, 2022)

- Updated Anjay-zephyr to 2.14.1
- Updated Zephyr to 2.7.0
- Updated sdk-nrf to 1.9.1
- Updated ei_demo to use the Edge Impulse wrapper from sdk-nrf

### Added

- Added support for firmware update

### Improvements

- Made GPS support configurable through Kconfig
- Migrated nRF targets to use nrfxlib sockets
- Migrated configuration to use Zephyr settings API

### Fixes

- Made the code more thread-safe
- Fixed erroneous uses of memset()

## 21.10 (Oct 11th, 2021)

- Updated Anjay-zephyr to 2.14.0

### Added

- Edge Impulse example
- L475 support in minimal client
- nRF9160 DK support in minimal client
- Thingy:91 support in minimal client

### Improvements

- Cleaner code using Anjay event loop
- Simpler demo implementation using Anjay IPSO objects

### Fixes

- Fixed global names

## 21.07 (Jul 30th, 2021)

- Updated Anjay-zephyr to 2.13.0
- Updated Zephyr to 2.6.0
- Updated sdk-nrf to 1.6.0

### Added
- Moved client containing all features to demo folder
- Minimal client example for qemu_x86
- Support for Thingy:91 in demo application
- Location object
- Buzzer object

### Improvements
- Changed regulatory domain on eS-WiFi modem to World Wide
- Sensor objects implementation depends on sensor presence, rather than
  board name.

## 21.06 (Jun 7th, 2021)

### Added
- Use Anjay-zephyr 2.11.1 integration layer
- Use Zephyr v2.5.0
- Use Zephyr logger as log handler

## 20.10 (Oct 1st, 2020)

### Added
- Add initial support for nRF9160DK board

### Fixed
- Fix assertion failure when invalid URI is passed
- Fix problem with invalid file state after write

## 20.07 (Jul 7th, 2020)
- Initial release
