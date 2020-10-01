# Copyright 2020 AVSystem <avsystem@avsystem.com>
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

zephyr_get_compile_options_for_lang(C ZEPHYR_COMPILE_OPTIONS_FOR_ANJAY)
get_target_property(ZEPHYR_COMPILE_DEFINITIONS_FOR_ANJAY zephyr_interface INTERFACE_COMPILE_DEFINITIONS)
get_target_property(ZEPHYR_INCLUDE_DIRS_FOR_ANJAY zephyr_interface INTERFACE_INCLUDE_DIRECTORIES)
list(APPEND ZEPHYR_INCLUDE_DIRS_FOR_ANJAY ${ZEPHYR_BASE}/include/posix)

add_compile_options(${ZEPHYR_COMPILE_OPTIONS_FOR_ANJAY})
add_compile_definitions(${ZEPHYR_COMPILE_DEFINITIONS_FOR_ANJAY})
include_directories(${ZEPHYR_INCLUDE_DIRS_FOR_ANJAY})

# We can't use paths relative to this file because it's included by 
# other CMakeLists.txt files placed under projects/* directories.
# Therefore, we use CMAKE_CURRENT_LIST_DIR to get the project root directory.
set(root_dir ${CMAKE_CURRENT_LIST_DIR})

file(GLOB_RECURSE ANJAY_SOURCES "${root_dir}/deps/anjay/src/*.c")
file(GLOB_RECURSE AVS_COMMONS_SOURCES "${root_dir}/deps/anjay/deps/avs_commons/src/*.c")
file(GLOB_RECURSE AVS_COAP_SOURCES "${root_dir}/deps/anjay/deps/avs_coap/src/*.c")

set(anjay_common_sources
    ${ANJAY_SOURCES}
    ${AVS_COMMONS_SOURCES}
    ${AVS_COAP_SOURCES}
    ${root_dir}/compat/mbedtls_compat.c)

set(app_common_sources 
    ${root_dir}/src/main.c
    ${root_dir}/src/led.c
    ${root_dir}/src/led.h
    ${root_dir}/src/menu.c
    ${root_dir}/src/menu.h
    ${root_dir}/src/utils.c
    ${root_dir}/src/utils.h
    ${root_dir}/src/default_config.h
    ${root_dir}/src/objects/objects.h
    ${root_dir}/src/objects/device.c
    ${root_dir}/src/objects/push_button.c)

if(${BOARD} STREQUAL "disco_l475_iot1")
    add_library(anjay MODULE
                ${anjay_common_sources})

    target_sources(app PRIVATE
                   ${app_common_sources}
                   ${root_dir}/src/objects/temperature.c
                   ${root_dir}/src/objects/humidity.c
                   ${root_dir}/src/objects/distance.c
                   ${root_dir}/src/objects/barometer.c
                   ${root_dir}/src/objects/accelerometer.c
                   ${root_dir}/src/objects/gyrometer.c
                   ${root_dir}/src/objects/magnetometer.c
                   ${root_dir}/src/objects/impl/basic_sensor_impl.c
                   ${root_dir}/src/objects/impl/basic_sensor_impl.h
                   ${root_dir}/src/objects/impl/three_axis_sensor_impl.c
                   ${root_dir}/src/objects/impl/three_axis_sensor_impl.h)
elseif(${BOARD} STREQUAL "nrf9160dk_nrf9160ns")
    add_library(anjay MODULE
                ${anjay_common_sources}
                ${root_dir}/src/objects/switch.c
                ${root_dir}/compat/time_compat.c)

    target_sources(app PRIVATE
                   ${app_common_sources})
else()
    message(FATAL_ERROR "${BOARD} board is not supported in this project")
endif()

target_include_directories(anjay
                           PRIVATE
                           ${root_dir}/deps/anjay/src
                           ${root_dir}/deps/anjay/deps/avs_commons/src
                           ${root_dir}/deps/anjay/deps/avs_coap/src
                           PUBLIC
                           ${root_dir}/config
                           ${root_dir}/deps/anjay/include_public
                           ${root_dir}/deps/anjay/deps/avs_commons/include_public
                           ${root_dir}/deps/anjay/deps/avs_coap/include_public)

target_link_libraries(anjay PRIVATE zephyr)

target_link_libraries(app PRIVATE anjay)
