#!/usr/bin/env python3
# -*- coding: utf-8 -*-
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

import serial
import subprocess
import os
import shlex
import yaml

import zephyr_adapter_common

COLOR_OFF = "\033[0m"
COLOR_MAGENTA = "\033[35m"
COLOR_CYAN = "\033[36m"

class TargetAdapter(zephyr_adapter_common.ZephyrAdapterCommon):
    def acquire_device(self):
        self.device = serial.Serial(
            self.config["device"],
            self.config["baudrate"],
            timeout=self.config["timeout"]
        )

    def release_device(self):
        if self.device is not None:
            self.__run_with_idf([
                os.environ["ESPTOOL_PATH"],
                "-p", self.config["device"],
                "erase_flash"
            ])
            self.device.close()
            self.device = None

    def flash_device(self):
        if self.__run_with_idf(self.__get_flasher_command()).returncode != 0:
            raise Exception("Flash failed")

    def __get_flasher_command(self):
        with open(self.config["yaml_path"], mode="r", encoding="utf-8") as flasher_args_file:
            flasher_args = yaml.safe_load(flasher_args_file)

            res = [
                os.environ["ESPTOOL_PATH"],
                "-p",           self.config["device"],
                "--before",     "default_reset",
                "--after",      "hard_reset",
                "--chip",       flasher_args["flash-runner"],
                "write_flash",
                "--flash_mode", "dio",
                "--flash_size", "detect",
                "--flash_freq", "40m",
            ]

            boot_addr = ""
            part_table_addr = ""
            app_addr = ""
            for option in flasher_args["args"][flasher_args["flash-runner"]]:
                if option.startswith("--esp-boot-address="):
                    boot_addr = option.split("--esp-boot-address=")[1]
                elif option.startswith("--esp-partition-table-address="):
                    part_table_addr = option.split("--esp-partition-table-address=")[1]
                elif option.startswith("--esp-app-address="):
                    app_addr = option.split("--esp-app-address=")[1]

            res.extend([boot_addr, self.config["binaries"]["bootloader"]])
            res.extend([part_table_addr, self.config["binaries"]["partitions"]])
            res.extend([app_addr, self.config["binaries"]["main_app"]])

            return res

    def __run_in_shell(self, commands):
        script = "\n".join([shlex.join(command) for command in commands])
        print(f"{COLOR_CYAN}Running script:\n{script}{COLOR_OFF}", flush=True)
        return subprocess.run(["/bin/bash", "-e"], input=script.encode("utf-8"))

    def __run_with_idf(self, command):
        source_idf = [".", f"{os.environ['IDF_PATH']}/export.sh"]
        return self.__run_in_shell([source_idf, command])
