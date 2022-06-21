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

set(app_common_sources
    src/anjay_shell.c
    src/common.h
    src/config.c
    src/config.h
    src/default_config.h
    src/firmware_update.h
    src/gps.h
    src/main.c
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

if(CONFIG_ANJAY_CLIENT_GPS_NRF)
    list(APPEND app_common_sources
         src/gps_impl/gps_nrf.c)
endif()

if(CONFIG_ANJAY_CLIENT_NRF_LC_INFO)
    list(APPEND app_common_sources
         src/nrf_lc_info.c
         src/nrf_lc_info.h
         src/objects/conn_mon.c
         src/objects/ecid.c)
endif()

if(CONFIG_ANJAY_CLIENT_LOCATION_SERVICES)
    list(APPEND app_common_sources
         src/objects/loc_assist.c)
endif()

if(CONFIG_ANJAY_CLIENT_FOTA)
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

    list(APPEND app_common_sources
         src/firmware_update.c)
endif()

if(CONFIG_ANJAY_CLIENT_PERSISTENCE)
    list(APPEND app_common_sources
         src/persistence.c)
endif()

target_sources(app PRIVATE
               ${app_common_sources})
