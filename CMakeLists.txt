# Copyright 2020-2021 AVSystem <avsystem@avsystem.com>
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

# We can't use paths relative to this file because it's included by
# other CMakeLists.txt files placed under projects/* directories.
# Therefore, we use CMAKE_CURRENT_LIST_DIR to get the project root directory.
set(root_dir ${CMAKE_CURRENT_LIST_DIR})

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
                   ${root_dir}/src/objects/impl/three_axis_sensor_impl.h
                   )
elseif(${BOARD} STREQUAL "nrf9160dk_nrf9160ns")

    target_sources(app PRIVATE
                   ${app_common_sources}
                   ${root_dir}/src/objects/switch.c
                   )
else()
    message(FATAL_ERROR "${BOARD} board is not supported in this project")
endif()
