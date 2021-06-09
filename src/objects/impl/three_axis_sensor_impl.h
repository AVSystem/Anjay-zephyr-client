/*
 * Copyright 2020-2021 AVSystem <avsystem@avsystem.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <anjay/dm.h>

#include <drivers/sensor.h>

const anjay_dm_object_def_t **
three_axis_sensor_object_create(const char *name,
                                enum sensor_channel channel,
                                const char *unit,
                                anjay_oid_t oid);

void three_axis_sensor_object_release(const anjay_dm_object_def_t **def);

void three_axis_sensor_object_update(anjay_t *anjay,
                                     const anjay_dm_object_def_t *const *def);
