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

import contextlib
import time

THROTTLED_WRITE_DELAY = 0.01


def throttled_write(device, payload):
    for byte in payload:
        device.write(bytes([byte]))
        device.flush()
        time.sleep(THROTTLED_WRITE_DELAY)


class ZephyrAdapterCommon:
    def __init__(self, config):
        self.config = config
        self.device = None

    def prepare_device(self):
        self.flash_device()

        self.skip_until("Initializing Anjay-zephyr-client demo")
        self.skip_until_prompt()
        print("Prompt message received, configuring...")

        self.__write_options()
        print("Device configured, waiting for connection...")

        self.skip_until("registration successful")
        print("Device successfully registered")

    @contextlib.contextmanager
    def __override_timeout(self, timeout):
        old_timeout = self.device.timeout
        try:
            if timeout is not None:
                self.device.timeout = timeout
            yield
        finally:
            self.device.timeout = old_timeout

    def __passthrough(self, incoming):
        if self.config["passthrough"]:
            decoded_incoming = incoming.decode("ASCII", errors="ignore")
            print("\033[35m", end="")
            print(decoded_incoming, end="")
            print("\033[0m")

    def passthrough(self, timeout):
        result = b''
        deadline = time.time() + timeout
        while True:
            new_timeout = max(deadline - time.time(), 0)
            with self.__override_timeout(new_timeout):
                result += self.device.read(65536)
            if new_timeout <= 0:
                break
            self.__passthrough(result)

    def skip_until(self, expected):
        expected_encoded = expected.encode("ASCII")
        incoming = self.device.read_until(expected_encoded)

        self.__passthrough(incoming)

        if not incoming.endswith(expected_encoded):
            raise Exception("skip_until timed out")

        return incoming

    def skip_until_prompt(self):
        return self.skip_until("uart:~$ ")

    def __write_options(self):
        throttled_write(self.device, b"\r\nanjay stop\r\n")
        while True:
            with self.__override_timeout(90):
                line = self.skip_until('\n')

            if b'Anjay stopped' in line or b'Anjay is not running' in line:
                break

        for opt_key, opt_val in self.config["device-config"].items():
            print(f"Writing option {opt_key}")
            throttled_write(self.device,
                            f"anjay config set {opt_key} {opt_val}\r\n".encode("ASCII"))
            self.skip_until_prompt()

        throttled_write(self.device, b"anjay config save\r\n")

        self.skip_until("Configuration successfully saved")

        time.sleep(1)
        throttled_write(self.device, b"kernel reboot cold\r\n")
