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

#include <anjay/anjay.h>

#include <drivers/sensor.h>
#include <zephyr.h>

#include "impl/three_axis_sensor_impl.h"
#include "objects.h"

const anjay_dm_object_def_t **gyrometer_object_create(void) {
    return three_axis_sensor_object_create(DT_LABEL(DT_INST(0, st_lsm6dsl)),
                                           SENSOR_CHAN_GYRO_XYZ, "deg/s", 3334);
}

void gyrometer_object_release(const anjay_dm_object_def_t **def) {
    three_axis_sensor_object_release(def);
}

void gyrometer_object_update(anjay_t *anjay,
                             const anjay_dm_object_def_t *const *def) {
    three_axis_sensor_object_update(anjay, def);
}
