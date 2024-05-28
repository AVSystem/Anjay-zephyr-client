# Copyright 2020-2024 AVSystem <avsystem@avsystem.com>
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

if(NOT DEFINED Zephyr_VERSION)
    message(ERROR "Zephyr version not set")
endif()

# NOTE: The directory for sysbuild has been called _sysbuild instead of
# sysbuild, because we want to avoid any tries of default merging .conf
# or .overlay by sysbuild, because we are not using that mechanism,
# which is not completely correct for our use case.
if(EXISTS "${CMAKE_BINARY_DIR}/../_sysbuild/mcuboot/boards/${BOARD}.conf")
    if (Zephyr_VERSION VERSION_LESS "3.4")
        set(MCUBOOT_CONF_FILE "mcuboot_OVERLAY_CONFIG")
    else()
        set(MCUBOOT_CONF_FILE "mcuboot_EXTRA_CONF_FILE")
    endif()

    set(${MCUBOOT_CONF_FILE}
        "${CMAKE_BINARY_DIR}/../_sysbuild/mcuboot/boards/${BOARD}.conf"
        CACHE INTERNAL "Conf for mcuboot")
endif()

set(MCUBOOT_DIR ${ZEPHYR_BASE}/../bootloader/mcuboot)
set(mcuboot_DTC_OVERLAY_FILE
${MCUBOOT_DIR}/boot/zephyr/app.overlay
CACHE INTERNAL "Overlay for mcuboot")

if(EXISTS "${CMAKE_BINARY_DIR}/../_sysbuild/mcuboot/boards/${BOARD}.overlay")
    list(APPEND mcuboot_DTC_OVERLAY_FILE
        "${CMAKE_BINARY_DIR}/../_sysbuild/mcuboot/boards/${BOARD}.overlay")
endif()

# NOTE: This is largely copied from CMakeLists.txt in Zephyr itself.
# However, using that would require setting HEX_FILES_TO_MERGE *before*
# find_package(Zephyr), and we can't do that because we need to examine
# Kconfig variables, which are populated by find_package(Zephyr).
# That's why we do it ourselves here...
set(MERGED_HEX_NAME "${CMAKE_CURRENT_BINARY_DIR}/zephyr/merged.hex")
set(MCUBOOT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/mcuboot")

if(Zephyr_VERSION VERSION_LESS "3.2")
    set(MERGEHEX_SCRIPT "${ZEPHYR_BASE}/scripts/mergehex.py")
else()
    set(MERGEHEX_SCRIPT "${ZEPHYR_BASE}/scripts/build/mergehex.py")
endif()

add_custom_command(OUTPUT "${MERGED_HEX_NAME}"
                    COMMAND "${PYTHON_EXECUTABLE}"
                            "${MERGEHEX_SCRIPT}"
                            -o "${MERGED_HEX_NAME}"
                            "${MCUBOOT_BINARY_DIR}/zephyr/zephyr.hex"
                            "${CMAKE_CURRENT_BINARY_DIR}/demo/zephyr/zephyr.signed.hex"
                    DEPENDS demo mcuboot)
add_custom_target(merged_hex ALL DEPENDS "${MERGED_HEX_NAME}")

set(FOTA_BINARY_FILE "${CMAKE_CURRENT_BINARY_DIR}/demo/zephyr/zephyr.signed.bin")
if(CONFIG_ANJAY_ZEPHYR_FOTA)
    # NOTE: Condider removing this (and the validate_fota_file_size.py script
    # itself) when https://github.com/nrfconnect/sdk-mcuboot/pull/185 is merged
    add_custom_target(validate_fota_file_size ALL DEPENDS merged_hex
                      COMMAND "${PYTHON_EXECUTABLE}"
                              "${CMAKE_CURRENT_SOURCE_DIR}/../tools/validate_fota_file_size.py"
                              "${FOTA_BINARY_FILE}")
endif()
