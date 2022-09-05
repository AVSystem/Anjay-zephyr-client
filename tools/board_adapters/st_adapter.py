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

import subprocess

import serial
import serial.tools.list_ports

import zephyr_adapter_common


# Yo dawg, I heard you like hexdumps
# So we put a hexdump in your hexdump so you can hexdump while you hexdump

# Apparently, st-flash's required format of --serial
# parameter is a hexlified ASCII-hexstring (sic!), while
# pyserial displays the serial number properly.
def unhexlify(hexstring):
    return bytes.fromhex(hexstring).decode("ASCII")


class TargetAdapter(zephyr_adapter_common.ZephyrAdapterCommon):
    def acquire_device(self):
        stlink_serial_unhexlified = unhexlify(self.config["stlink-serial"])
        ports = [
            port
            for port
            in serial.tools.list_ports.comports()
            if port.serial_number == stlink_serial_unhexlified
        ]

        if len(ports) != 1:
            raise Exception(
                f"Found {len(ports)} ports for given ST-Link serial ({self.config['stlink-serial']})")

        self.device = serial.Serial(ports[0].device, self.config["baudrate"],
                                    timeout=self.config["timeout"])

    def flash_device(self):
        if subprocess.run(["st-flash",
                           "--serial", self.config["stlink-serial"],
                           "--format", "ihex",
                           "write", self.config["hex-path"]
                           ]).returncode != 0:
            raise Exception("Flash failed")

    def release_device(self):
        if self.device is not None:
            subprocess.run(["st-flash", "--serial", self.config["stlink-serial"], "erase"])
            self.device.close()
            self.device = None
