/*
 * Copyright 2020 AVSystem <avsystem@avsystem.com>
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

const anjay_dm_object_def_t **temperature_object_create(void);
void temperature_object_release(const anjay_dm_object_def_t **def);
void temperature_object_update(anjay_t *anjay,
                               const anjay_dm_object_def_t *const *def);

const anjay_dm_object_def_t **humidity_object_create(void);
void humidity_object_release(const anjay_dm_object_def_t **def);
void humidity_object_update(anjay_t *anjay,
                            const anjay_dm_object_def_t *const *def);

const anjay_dm_object_def_t **distance_object_create(void);
void distance_object_release(const anjay_dm_object_def_t **def);
void distance_object_update(anjay_t *anjay,
                            const anjay_dm_object_def_t *const *def);

const anjay_dm_object_def_t **barometer_object_create(void);
void barometer_object_release(const anjay_dm_object_def_t **def);
void barometer_object_update(anjay_t *anjay,
                             const anjay_dm_object_def_t *const *def);

const anjay_dm_object_def_t **accelerometer_object_create(void);
void accelerometer_object_release(const anjay_dm_object_def_t **def);
void accelerometer_object_update(anjay_t *anjay,
                                 const anjay_dm_object_def_t *const *def);

const anjay_dm_object_def_t **gyrometer_object_create(void);
void gyrometer_object_release(const anjay_dm_object_def_t **def);
void gyrometer_object_update(anjay_t *anjay,
                             const anjay_dm_object_def_t *const *def);

const anjay_dm_object_def_t **magnetometer_object_create(void);
void magnetometer_object_release(const anjay_dm_object_def_t **def);
void magnetometer_object_update(anjay_t *anjay,
                                const anjay_dm_object_def_t *const *def);

const anjay_dm_object_def_t **push_button_object_create(void);
void push_button_object_release(const anjay_dm_object_def_t **def);
void push_button_object_update(anjay_t *anjay,
                               const anjay_dm_object_def_t *const *def);

const anjay_dm_object_def_t **switch_object_create(void);
void switch_object_release(const anjay_dm_object_def_t **def);
void switch_object_update(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *def);

const anjay_dm_object_def_t **device_object_create(void);
void device_object_release(const anjay_dm_object_def_t **def);
void device_object_update(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *def);
