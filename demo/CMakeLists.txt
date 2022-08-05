# Copyright 2020-2022 AVSystem <avsystem@avsystem.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(anjay_zephyr_demo)
set(root_dir ${ANJAY_ZEPHYR_CLIENT_DIR})

if(CONFIG_PARTITION_MANAGER_ENABLED)
    # defined in nrf/cmake/partition_manager.cmake
    if(NOT EXISTS "${static_configuration_file}")
        set(pm_missing_message "Missing static partition manager file for board: ${BOARD}")

        if(ACTIVE_BOARD_REVISION)
            set(pm_missing_message "${pm_missing_message}, rev: ${ACTIVE_BOARD_REVISION}")
        endif()

        if(CONF_FILE_BUILD_TYPE)
            set(pm_missing_message "${pm_missing_message}, build type: ${CONF_FILE_BUILD_TYPE}")
        endif()

        message(FATAL_ERROR "${pm_missing_message}")
    endif()
elseif(CONFIG_ANJAY_CLIENT_BUILD_MCUBOOT_AUTOMATICALLY)
    if("${CONFIG_MCUBOOT_SIGNATURE_KEY_FILE}" STREQUAL "")
        if(NOT CONFIG_MCUBOOT_GENERATE_UNSIGNED_IMAGE)
            message(FATAL_ERROR "Either MCUBOOT_SIGNATURE_KEY_FILE or MCUBOOT_GENERATE_UNSIGNED_IMAGE need to be set in Kconfig")
        endif()
    else()
        set(MCUBOOT_SIGNATURE_KEY_FILE "${CONFIG_MCUBOOT_SIGNATURE_KEY_FILE}")
        if(NOT IS_ABSOLUTE "${MCUBOOT_SIGNATURE_KEY_FILE}")
            set(MCUBOOT_SIGNATURE_KEY_FILE "${WEST_TOPDIR}/${MCUBOOT_SIGNATURE_KEY_FILE}")
        endif()
    endif()
    make_directory("${CMAKE_CURRENT_BINARY_DIR}/mcuboot")
    execute_process(COMMAND "${CMAKE_COMMAND}"
                            -GNinja
                            "-DAPPLICATION_CONFIG_DIR=${CMAKE_CURRENT_SOURCE_DIR}/child_image/mcuboot"
                            "-DBOARD=${BOARD}"
                            "-DCONF_FILE=${ZEPHYR_BASE}/../bootloader/mcuboot/boot/zephyr/prj.conf"
                            "-DCONF_FILE_INCLUDE_FRAGMENTS=ON"
                            "-DCONFIG_BOOT_SIGNATURE_KEY_FILE=\"${MCUBOOT_SIGNATURE_KEY_FILE}\""
                            "-DDTC_OVERLAY_FILE=${CMAKE_CURRENT_SOURCE_DIR}/child_image/mcuboot/boards/${BOARD}.overlay"
                            "${ZEPHYR_BASE}/../bootloader/mcuboot/boot/zephyr"
                    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/mcuboot"
                    RESULT_VARIABLE MCUBOOT_CMAKE_RESULT)
    if(NOT MCUBOOT_CMAKE_RESULT EQUAL 0)
        message(FATAL_ERROR "Configuring MCUboot failed")
    endif()

    add_custom_target(mcuboot COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_CURRENT_BINARY_DIR}/mcuboot")

    # NOTE: This is largely copied from CMakeLists.txt in Zephyr itself.
    # However, using that would require setting HEX_FILES_TO_MERGE *before*
    # find_package(Zephyr), and we can't do that because we need to examine
    # Kconfig variables, which are populated by find_package(Zephyr).
    # That's why we do it ourselves here...
    set(MERGED_HEX_NAME "${CMAKE_CURRENT_BINARY_DIR}/zephyr/merged.hex")
    add_custom_command(OUTPUT "${MERGED_HEX_NAME}"
                       COMMAND "${PYTHON_EXECUTABLE}"
                               "${ZEPHYR_BASE}/scripts/mergehex.py"
                               -o "${MERGED_HEX_NAME}"
                               "${CMAKE_CURRENT_BINARY_DIR}/mcuboot/zephyr/zephyr.hex"
                               "${CMAKE_CURRENT_BINARY_DIR}/zephyr/zephyr.signed.hex"
                       DEPENDS mcuboot zephyr_final)
    add_custom_target(mergehex ALL DEPENDS "${MERGED_HEX_NAME}")

    if(TARGET runners_yaml_props_target)
        get_target_property(RUNNERS_YAML_CONTENTS runners_yaml_props_target yaml_contents)
        string(REGEX REPLACE "zephyr[a-zA-Z0-9._-]*[.]hex" "merged.hex" RUNNERS_YAML_CONTENTS "${RUNNERS_YAML_CONTENTS}")
        set_property(TARGET runners_yaml_props_target PROPERTY yaml_contents "${RUNNERS_YAML_CONTENTS}")
    endif()
endif()

if(CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING_INITIAL_FLASH)
    set(app_sources
        src/config.c
        src/config.h
        src/persistence.c
        src/persistence.h
        src/utils.c
        src/utils.h
        src/factory_provisioning/factory_flash.c
        src/factory_provisioning/factory_flash.h
        src/factory_provisioning/provisioning_app.c)
else()
    set(app_sources
        src/anjay_shell.c
        src/common.h
        src/config.c
        src/config.h
        src/default_config.h
        src/firmware_update.h
        src/gps.h
        src/main_app.c
        src/persistence.h
        src/status_led.c
        src/status_led.h
        src/utils.c
        src/utils.h
        src/objects/basic_sensors.c
        src/objects/buzzer.c
        src/objects/device.c
        src/objects/led_color_light.c
        src/objects/location.c
        src/objects/objects.h
        src/objects/push_button.c
        src/objects/switch.c
        src/objects/three_axis_sensors.c)

    if(CONFIG_ANJAY_CLIENT_PERSISTENCE)
        list(APPEND app_sources
             src/persistence.c)
    endif()

    if(CONFIG_ANJAY_CLIENT_GPS_NRF)
        list(APPEND app_sources
             src/gps_impl/gps_nrf.c)
    endif()

    if(CONFIG_ANJAY_CLIENT_NRF_LC_INFO)
        list(APPEND app_sources
             src/nrf_lc_info.c
             src/nrf_lc_info.h
             src/objects/conn_mon.c
             src/objects/ecid.c)
    endif()

    if(CONFIG_ANJAY_CLIENT_LOCATION_SERVICES)
        list(APPEND app_sources
             src/objects/loc_assist.c)
    endif()

    if(CONFIG_ANJAY_CLIENT_FOTA)
        list(APPEND app_sources
             src/firmware_update.c)
    endif()
endif()

target_sources(app PRIVATE
               ${app_sources})

if(CONF_FILE_BUILD_TYPE STREQUAL "extflash" AND CONFIG_BOOTLOADER_MCUBOOT)
    # This is a workaround for a bug in nCS' integration with TF-M.
    #
    # When MCUboot and TF-M are both in use, the CONFIG_BOOTLOADER_MCUBOOT
    # option is propagated to TF-M (https://github.com/nrfconnect/sdk-nrf/blob/v2.0.0/modules/tfm/zephyr/CMakeLists.txt#L115),
    # which in turn enables the BL2 option (https://github.com/nrfconnect/sdk-nrf/blob/v2.0.0/modules/tfm/tfm/boards/CMakeLists.txt#L34)
    # and that declares a flash region (https://github.com/nrfconnect/sdk-trusted-firmware-m/blob/v1.5.0-ncs2/platform/ext/target/nordic_nrf/common/nrf9160/target_cfg.c#L420)
    # which TF-M will try to configure as non-secure
    # (https://github.com/nrfconnect/sdk-trusted-firmware-m/blob/v1.5.0-ncs2/platform/ext/target/nordic_nrf/common/nrf9160/target_cfg.c#L552).
    #
    # Because the hardware flash protection only works for the internal flash,
    # and these addresses are not valid in that context, the MCU hard-faults and
    # consequently enters a boot loop.
    #
    # This workaround overrides the CONFIG_BOOTLOADER_MCUBOOT setting, so that
    # BL2 macro is not defined, and this issue does not happen.
    #
    # See also the bug report on Nordic DevZone:
    # https://devzone.nordicsemi.com/f/nordic-q-a/89735/nrf9160-external-flash-mcuboot-buggy-flash-protection-configuration-in-sdk
    set_property(TARGET zephyr_property_target APPEND PROPERTY TFM_CMAKE_OPTIONS
                 -DCONFIG_BOOTLOADER_MCUBOOT=FALSE)
endif()
