#!/usr/bin/env python3
# -*- coding: utf-8 -*-
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
import collections
import os
import sys

# See: https://github.com/mcu-tools/mcuboot/blob/v1.10.0/scripts/imgtool/image.py#L569
# This is calculated from default values for Image._trailer_size()
# (write_size=4, max_sectors=128, max_align=8)
# write_size and max_align may change if CONFIG_MCUBOOT_FLASH_WRITE_BLOCK_SIZE,
# but it looks that nobody does that ;)
MCUBOOT_TRAILER_SIZE = 1584


def _split_path_into_components(path):
    result = []
    while True:
        head, tail = os.path.split(path)
        if tail != '' or head != path:
            result.insert(0, tail)
            path = head
        else:
            result.insert(0, head)
            return result


PartitionParams = collections.namedtuple('PartitionParams', ['size', 'sector_size'])


def get_partition_params(dts_filename):
    from devicetree import dtlib
    dt = dtlib.DT(dts_filename)
    part = dt.label2node['slot0_partition']
    size = part.props['reg'].to_nums()[1]
    sector_size = part.parent.parent.props['erase-block-size'].to_num()

    # nCS Partition Manager defines the partitions differently
    try:
        import yaml
        with open(os.path.join(os.path.dirname(os.path.dirname(dts_filename)), 'partitions.yml'),
                  'r') as f:
            partitions = yaml.load(f, yaml.SafeLoader)
        size = partitions['mcuboot_primary']['size']
    except FileNotFoundError:
        pass

    return PartitionParams(size=size, sector_size=sector_size)


def main(argv):
    assert len(argv) > 0
    if len(argv) != 2:
        sys.stderr.write(f'Usage: {argv[0]} FOTA_BINARY_FILE\n')
        return -1

    fota_binary_file = os.path.realpath(argv[1])
    fota_binary_file_path = _split_path_into_components(fota_binary_file)

    if len(fota_binary_file_path) < 2 or fota_binary_file_path[-2] != 'zephyr':
        sys.stderr.write(f'The binary file is not in a Zephyr build tree\n')
        return -1

    zephyr_build_tree = os.path.join(*fota_binary_file_path[:-2])

    with open(os.path.join(zephyr_build_tree, 'mcuboot', 'zephyr', '.config'), 'r') as f:
        lines = [line for line in f.readlines() if 'CONFIG_BOOT_SWAP_USING_MOVE' in line]
        mcuboot_uses_move = (
                len(lines) == 1 and lines[0].strip() == 'CONFIG_BOOT_SWAP_USING_MOVE=y')

    if not mcuboot_uses_move:
        # Only the move algorithm requires free space; return success otherwise
        return 0

    # Find ZEPHYR_BASE
    with open(os.path.join(zephyr_build_tree, 'CMakeCache.txt'), 'r') as f:
        lines = [line for line in f.readlines() if line.startswith('ZEPHYR_BASE')]
        if len(lines) != 1:
            sys.stderr.write(f'Could not determine ZEPHYR_BASE')
            return -1
        zephyr_base = lines[0].strip().split('=', 1)[1]

    sys.path.append(os.path.join(zephyr_base, 'scripts', 'dts', 'python-devicetree', 'src'))
    params = get_partition_params(os.path.join(zephyr_build_tree, 'zephyr', 'zephyr.dts'))

    # At least one empty sector and space for the trailer is required for a successful FOTA
    max_allowed_size = params.size - params.sector_size
    max_allowed_size -= params.sector_size * (
            (MCUBOOT_TRAILER_SIZE - 1 + params.sector_size) // params.sector_size)

    fota_binary_size = os.stat(fota_binary_file).st_size
    if fota_binary_size > max_allowed_size:
        sys.stderr.write(
            f'{argv[1]} is {fota_binary_size} bytes, but {max_allowed_size} is the maximum size with which FOTA using the move algorithm is possible (overflow by {fota_binary_size - max_allowed_size} bytes)')
        return -1

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
