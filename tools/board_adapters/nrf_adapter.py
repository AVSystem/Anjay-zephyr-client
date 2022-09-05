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

import zephyr_adapter_common

class TargetAdapter(zephyr_adapter_common.ZephyrAdapterCommon):
    @property
    def nrfjprog_serial(self):
        # As empirically checked (using PRoot and a faked contents of /sys/devices/.../serial),
        # nrfjprog always strips all leading zeros from the serial number
        return self.config['nrfjprog-serial'].strip().lstrip('0')

    def get_port(self):
        with subprocess.Popen(['nrfjprog', '-s', self.nrfjprog_serial, '--com'],
                              stdout=subprocess.PIPE) as p:
            ports = [port[1] for port in
                     (line.strip().decode().split() for line in p.stdout.readlines()) if
                     port[2] == 'VCOM0']

        if len(ports) != 1:
            raise Exception(
                f"Found {len(ports)} VCOM0 ports for given nrfjprog serial ({self.config['nrfjprog-serial']})")

        return ports[0]

    def acquire_device(self):
        self.device = serial.Serial(self.get_port(), self.config['baudrate'],
                                    timeout=self.config['timeout'])

    def flash_device(self, hex_image=None, chiperase=False):
        erase_option = '--chiperase' if chiperase else '--sectorerase'
        image = hex_image if hex_image else self.config['hex-path']

        if subprocess.run(['nrfjprog', '-s', self.nrfjprog_serial, '--program', image, erase_option]).returncode != 0:
            raise Exception('Flash failed')

        if subprocess.run(['nrfjprog', '-s', self.nrfjprog_serial, '--pinreset']).returncode != 0:
            raise Exception('Flash failed')

    def release_device(self, erase=True):
        if self.device is not None:
            if erase:
                subprocess.run(['nrfjprog', '-s', self.nrfjprog_serial, '-e'])
            self.device.close()
            self.device = None
