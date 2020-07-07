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

cmake_minimum_required(VERSION 3.13.1)
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(anjay_zephyr)

zephyr_get_compile_options_for_lang(C ZEPHYR_COMPILE_OPTIONS_FOR_ANJAY)
get_target_property(ZEPHYR_COMPILE_DEFINITIONS_FOR_ANJAY zephyr_interface INTERFACE_COMPILE_DEFINITIONS)
get_target_property(ZEPHYR_INCLUDE_DIRS_FOR_ANJAY zephyr_interface INTERFACE_INCLUDE_DIRECTORIES)
list(APPEND ZEPHYR_INCLUDE_DIRS_FOR_ANJAY ${ZEPHYR_BASE}/include/posix)

add_compile_options(${ZEPHYR_COMPILE_OPTIONS_FOR_ANJAY})
add_compile_definitions(${ZEPHYR_COMPILE_DEFINITIONS_FOR_ANJAY})
include_directories(${ZEPHYR_INCLUDE_DIRS_FOR_ANJAY})

file(GLOB_RECURSE ANJAY_SOURCES "deps/anjay/src/*.c")
file(GLOB_RECURSE AVS_COMMONS_SOURCES "deps/anjay/deps/avs_commons/src/*.c")
file(GLOB_RECURSE AVS_COAP_SOURCES "deps/anjay/deps/avs_coap/src/*.c")

add_library(anjay MODULE
            ${ANJAY_SOURCES}
            ${AVS_COMMONS_SOURCES}
            ${AVS_COAP_SOURCES}
            compat/mbedtls_compat.c)

target_include_directories(anjay
                           PRIVATE
                           deps/anjay/src
                           deps/anjay/deps/avs_commons/src
                           deps/anjay/deps/avs_coap/src
                           PUBLIC
                           config
                           deps/anjay/include_public
                           deps/anjay/deps/avs_commons/include_public
                           deps/anjay/deps/avs_coap/include_public)

target_link_libraries(anjay PRIVATE zephyr)

target_sources(app PRIVATE
               src/main.c
               src/led.c
               src/led.h
               src/menu.c
               src/menu.h
               src/utils.c
               src/utils.h
               src/default_config.h
               src/objects/objects.h
               src/objects/temperature.c
               src/objects/humidity.c
               src/objects/distance.c
               src/objects/barometer.c
               src/objects/accelerometer.c
               src/objects/gyrometer.c
               src/objects/magnetometer.c
               src/objects/push_button.c
               src/objects/device.c
               src/objects/impl/basic_sensor_impl.c
               src/objects/impl/basic_sensor_impl.h
               src/objects/impl/three_axis_sensor_impl.c
               src/objects/impl/three_axis_sensor_impl.h)

target_link_libraries(app PRIVATE anjay)
