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

import argparse
import importlib
import importlib.util
import subprocess
import sys
import os
import shutil
import tempfile
import yaml
import west.util

from requests import HTTPError
from subprocess import CalledProcessError

class ZephyrImageBuilder:
    def __init__(self, board, image_dir):
        script_directory = os.path.dirname(os.path.realpath(__file__))

        self.board = board
        self.image_dir = image_dir if image_dir else os.path.join(os.getcwd(), 'provisioning_builds')
        self.image_path = {}
        self.overlay = {}

        for kind in ['initial', 'final']:
            self.image_path[kind] = os.path.join(self.image_dir, f'{kind}.hex')
            self.overlay[kind] = os.path.join(script_directory, f'{kind}_overlay.conf')

    def __build(self, kind):
        current_build_directory = os.path.join(self.image_dir, kind)
        os.makedirs(current_build_directory, exist_ok=True)

        subprocess.run(['west', 'build', '-b', self.board, '-d', current_build_directory,
                        '-p', '--', f'-DOVERLAY_CONFIG={self.overlay[kind]}'], check=True)

        shutil.move(os.path.join(current_build_directory, 'zephyr/merged.hex'), self.image_path[kind])
        shutil.rmtree(current_build_directory)

        return self.image_path[kind]

    def build_initial_image(self):
        return self.__build('initial')

    def build_final_image(self):
        return self.__build('final')


def get_images(args):
    initial_image = None
    final_image = None

    if args.image_dir:
        potential_initial_image = os.path.join(args.image_dir, 'initial.hex')
        if os.path.exists(potential_initial_image):
            initial_image = potential_initial_image
            print('Using provided initial.hex file as an initial provisioning image')

        potential_final_image = os.path.join(args.image_dir, 'final.hex')
        if os.path.exists(potential_final_image):
            final_image = potential_final_image
            print('Using provided final.hex file as a final provisioning image')

    if args.board and (initial_image is None or final_image is None):
        builder = ZephyrImageBuilder(args.board, args.image_dir)

        if initial_image is None:
            print('Initial provisioning image not provided - building')
            initial_image = builder.build_initial_image()

        if final_image is None:
            print('Final provisioning image not provided - building')
            final_image = builder.build_final_image()

    if initial_image is None or final_image is None:
        raise ValueError('Zephyr images cannot be obtained')

    return initial_image, final_image

def get_device_adapter(serial_number, baudrate):
    sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), '../board_adapters'))
    adapter_module = importlib.import_module('nrf_adapter')

    config = {
        'nrfjprog-serial': serial_number,
        'baudrate': str(baudrate),
        'timeout': 60,
        'hex-path': None,
        'passthrough': False
    }

    return adapter_module.TargetAdapter(config)

def flash_device(device, image, chiperase, success_text):
    device.acquire_device()
    device.flash_device(image, chiperase)
    device.skip_until(success_text)
    device.skip_until_prompt()
    device.release_device(erase=False)

    print('Device flashed succesfully')

def mcumgr_download(port, src, baud=115200):
    with tempfile.TemporaryDirectory() as dst_dir_name:
        dst_file_name = os.path.join(dst_dir_name, 'result.txt')
        connstring = f'dev={port},baud={baud}'
        command = ['mcumgr', '--conntype', 'serial', '--connstring', connstring,
                   'fs', 'download', src, dst_file_name, '-t', '30']

        subprocess.run(command, cwd=os.getcwd(), universal_newlines=True, check=True)

        with open(dst_file_name) as dst_file:
            return dst_file.read()

def mcumgr_upload(port, src, dst, baud=115200):
    subprocess.run(['mcumgr', '--conntype', 'serial', '--connstring', f'dev={port},baud={baud}', 'fs', 'upload', src, dst],
                    cwd=os.getcwd(), universal_newlines=True, check=True)

def get_anjay_zephyr_path(manifest_path):
    with open(manifest_path, 'r') as stream:
        projects = yaml.safe_load(stream)['manifest']['projects']
        for project in projects:
            if project['name'] == 'anjay-zephyr':
                return os.path.join(west.util.west_topdir(), project['path'])

def get_manifest_path():
    west_manifest_path = None
    west_manifest_file = None

    west_config = subprocess.run(['west', 'config', '-l'], capture_output=True).stdout.split(b'\n')
    for config_entry in west_config:
        if config_entry.startswith(b'manifest.file'):
            west_manifest_file = config_entry.split(b'=')[1].strip()
        if config_entry.startswith(b'manifest.path'):
            west_manifest_path = config_entry.split(b'=')[1].strip()

    print(f'Manifest path: {west_manifest_path}')
    print(f'Manifest file: {west_manifest_file}')

    if (not west_manifest_file) or (not west_manifest_path):
        raise Exception('west config incomplete')

    return os.path.join(west_manifest_path, west_manifest_file)


def main():
    parser = argparse.ArgumentParser(
        description='Factory provisioning tool')

    # Arguments for building the Zephyr images
    parser.add_argument('-b', '--board', type=str,
                        help='Board for which the image should be built, may be not provided if images are cached',
                        required=False)
    parser.add_argument('-i', '--image_dir', type=str,
                        help='Directory for the cached Zephyr hex images',
                        required=False,
                        default=None)

    # Arguments for flashing
    parser.add_argument('-s', '--serial', type=str,
                        help='Serial number of the device to be used',
                        required=True)
    parser.add_argument('-B', '--baudrate', type=int,
                        help='Baudrate for the used serial port',
                        required=False, default=115200)

    # Arguments for factory_provisioning library
    parser.add_argument('-c', '--endpoint_cfg', type=str,
                        help='Configuration file containing device information to be loaded on the device',
                        required=True)
    parser.add_argument('-S', '--server', type=str,
                        help='JSON format file containing Coiote server information',
                        required=False)
    parser.add_argument('-t', '--token', type=str,
                        help='Access token for authorization to Coiote server',
                        required=False)
    parser.add_argument('-C', '--cert', type=str,
                        help='JSON format file containing information for the generation of a self signed certificate',
                        required=False)
    parser.add_argument('-k', '--pkey', type=str,
                        help='Endpoint private key in DER format, ignored if CERT parameter is set',
                        required=False)
    parser.add_argument('-r', '--pcert', type=str,
                        help='Endpoint public cert in DER format, ignored if CERT parameter is set',
                        required=False,)
    parser.add_argument('-p', '--scert', type=str,
                        help='Server public cert in DER format',
                        required=False)

    args = parser.parse_args()

    # This is called also as an early check if the proper west config is set
    manifest_path = get_manifest_path()

    if bool(args.server) != bool(args.token):
        raise Exception('Server and token cli arguments should be always provided together')

    anjay_path = os.path.join(get_anjay_zephyr_path(manifest_path), 'deps/anjay')
    provisioning_tool_path = os.path.join(anjay_path, 'tools/provisioning-tool')
    sys.path.append(provisioning_tool_path)
    fp = importlib.import_module('factory_prov.factory_prov')

    initial_image, final_image = get_images(args)

    print('Zephyr Images ready!')

    adapter = get_device_adapter(args.serial, args.baudrate)
    flash_device(adapter, initial_image, True, 'Device ready for provisioning.')

    port = adapter.get_port()
    endpoint_name = mcumgr_download(port, '/factory/endpoint.txt', args.baudrate)

    print(f'Downloaded endpoint name: {endpoint_name}')

    fcty = fp.FactoryProvisioning(args.endpoint_cfg, endpoint_name, args.server,
                                    args.token, args.cert)
    if fcty.get_sec_mode() == 'cert':
        if args.scert is not None:
            fcty.set_server_cert(args.scert)

        if args.cert is not None:
            fcty.generate_self_signed_cert()
        elif args.pkey is not None and args.pcert is not None:
            fcty.set_endpoint_cert_and_key(args.pcert, args.pkey)

    with tempfile.TemporaryDirectory() as temp_directory:
        last_cwd = os.getcwd()
        os.chdir(temp_directory)
        fcty.provision_device()
        mcumgr_upload(adapter.get_port(), 'SenMLCBOR', '/factory/provision.cbor', args.baudrate)
        os.chdir(last_cwd)

    if int(mcumgr_download(port, '/factory/result.txt', args.baudrate)) != 0:
        raise RuntimeError('Bad device provisioning result')

    if args.server and args.token:
        fcty.register()

    flash_device(adapter, final_image, False, 'persistence: Anjay restored from')


if __name__ == '__main__':
    main()
