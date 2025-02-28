# Copyright 2020-2025 AVSystem <avsystem@avsystem.com>
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
    if(CONFIG_ANJAY_ZEPHYR_FOTA)
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

    endif()
endif()

set(app_sources
    src/main_app.c
    src/sensors.c
    src/sensors.h
    src/status_led.c
    src/status_led.h
    src/peripherals.h
    src/bubblemaker.c
    src/bubblemaker.h
    src/led_strip.c
    src/led_strip.h
    src/water_meter.c
    src/water_meter.h
    src/water_pump.c
    src/water_pump.h)

target_sources(app PRIVATE
               ${app_sources})
