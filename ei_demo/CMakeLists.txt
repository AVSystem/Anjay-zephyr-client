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

cmake_minimum_required(VERSION 3.13.1)

# Edge Impulse
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Anjay zephyr client
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(anjay_zephyr_demo)
set(root_dir ${ANJAY_ZEPHYR_CLIENT_DIR})

# Edge Impulse

set(MODEL_FOLDER ei)
include(${MODEL_FOLDER}/edge-impulse-sdk/cmake/utils.cmake)
add_subdirectory(${MODEL_FOLDER}/edge-impulse-sdk/cmake/zephyr)
target_include_directories(app PRIVATE
    ${MODEL_FOLDER}
    ${MODEL_FOLDER}/tflite-model
    ${MODEL_FOLDER}/model-parameters
)

RECURSIVE_FIND_FILE(MODEL_FILES "${MODEL_FOLDER}/tflite-model" "*.cpp")
list(APPEND SOURCE_FILES ${MODEL_FILES})
target_sources(app PRIVATE ${SOURCE_FILES})

# Anjay zephyr client
set(app_common_sources
    src/main.c
    src/utils.c
    src/utils.h
    src/led.c
    src/led.h
    src/inference.cpp
    src/inference.h
    src/objects/objects.h
    src/objects/device.c
    src/objects/pattern_detector.c)

target_sources(app PRIVATE
                ${app_common_sources}
                )
