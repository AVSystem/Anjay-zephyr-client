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
#include <device.h>
#include <devicetree.h>

const anjay_dm_object_def_t **location_object_create(void);
void location_object_release(const anjay_dm_object_def_t ***out_def);
void location_object_update(anjay_t *anjay,
                            const anjay_dm_object_def_t *const *def);

#define TEMPERATURE_NODE DT_ALIAS(temperature)
#define TEMPERATURE_AVAILABLE DT_NODE_HAS_STATUS(TEMPERATURE_NODE, okay)
const anjay_dm_object_def_t **temperature_object_create(void);
void temperature_object_release(const anjay_dm_object_def_t ***out_def);
void temperature_object_update(anjay_t *anjay,
                               const anjay_dm_object_def_t *const *def);

#define HUMIDITY_NODE DT_ALIAS(humidity)
#define HUMIDITY_AVAILABLE DT_NODE_HAS_STATUS(HUMIDITY_NODE, okay)
const anjay_dm_object_def_t **humidity_object_create(void);
void humidity_object_release(const anjay_dm_object_def_t ***out_def);
void humidity_object_update(anjay_t *anjay,
                            const anjay_dm_object_def_t *const *def);

#define DISTANCE_NODE DT_ALIAS(distance)
#define DISTANCE_AVAILABLE DT_NODE_HAS_STATUS(DISTANCE_AVAILABLE, okay)
const anjay_dm_object_def_t **distance_object_create(void);
void distance_object_release(const anjay_dm_object_def_t ***out_def);
void distance_object_update(anjay_t *anjay,
                            const anjay_dm_object_def_t *const *def);

#define BAROMETER_NODE DT_ALIAS(barometer)
#define BAROMETER_AVAILABLE DT_NODE_HAS_STATUS(BAROMETER_NODE, okay)
const anjay_dm_object_def_t **barometer_object_create(void);
void barometer_object_release(const anjay_dm_object_def_t ***out_def);
void barometer_object_update(anjay_t *anjay,
                             const anjay_dm_object_def_t *const *def);

#define ACCELEROMETER_NODE DT_ALIAS(accelerometer)
#define ACCELEROMETER_AVAILABLE DT_NODE_HAS_STATUS(ACCELEROMETER_NODE, okay)
const anjay_dm_object_def_t **accelerometer_object_create(void);
void accelerometer_object_release(const anjay_dm_object_def_t ***out_def);
void accelerometer_object_update(anjay_t *anjay,
                                 const anjay_dm_object_def_t *const *def);

#define GYROMETER_NODE DT_ALIAS(gyrometer)
#define GYROMETER_AVAILABLE DT_NODE_HAS_STATUS(GYROMETER_NODE, okay)
const anjay_dm_object_def_t **gyrometer_object_create(void);
void gyrometer_object_release(const anjay_dm_object_def_t ***out_def);
void gyrometer_object_update(anjay_t *anjay,
                             const anjay_dm_object_def_t *const *def);

#define MAGNETOMETER_NODE DT_ALIAS(magnetometer)
#define MAGNETOMETER_AVAILABLE DT_NODE_HAS_STATUS(MAGNETOMETER_NODE, okay)
const anjay_dm_object_def_t **magnetometer_object_create(void);
void magnetometer_object_release(const anjay_dm_object_def_t ***out_def);
void magnetometer_object_update(anjay_t *anjay,
                                const anjay_dm_object_def_t *const *def);

#define PUSH_BUTTON_NODE(idx) DT_ALIAS(push_button_##idx)
#define PUSH_BUTTON_AVAILABLE(idx) \
    DT_NODE_HAS_STATUS(PUSH_BUTTON_NODE(idx), okay)
#define PUSH_BUTTON_AVAILABLE_ANY                         \
    (PUSH_BUTTON_AVAILABLE(0) || PUSH_BUTTON_AVAILABLE(1) \
     || PUSH_BUTTON_AVAILABLE(2))
const anjay_dm_object_def_t **push_button_object_create(void);
void push_button_object_release(const anjay_dm_object_def_t ***out_def);
void push_button_object_update(anjay_t *anjay,
                               const anjay_dm_object_def_t *const *def);

#define SWITCH_NODE(idx) DT_ALIAS(switch_##idx)
#define SWITCH_AVAILABLE(idx) DT_NODE_HAS_STATUS(SWITCH_NODE(idx), okay)
#define SWITCH_AVAILABLE_ANY \
    (SWITCH_AVAILABLE(0) || SWITCH_AVAILABLE(1) || SWITCH_AVAILABLE(2))
const anjay_dm_object_def_t **switch_object_create(void);
void switch_object_release(const anjay_dm_object_def_t ***out_def);
void switch_object_update(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *def);

#define BUZZER_NODE DT_ALIAS(buzzer_pwm)
#define BUZZER_AVAILABLE DT_NODE_HAS_STATUS(BUZZER_NODE, okay)
const anjay_dm_object_def_t **buzzer_object_create(void);
void buzzer_object_release(const anjay_dm_object_def_t ***out_def);
void buzzer_object_update(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *def);

#define RGB_NODE DT_ALIAS(rgb_pwm)
#define LED_COLOR_LIGHT_AVAILABLE DT_NODE_HAS_STATUS(RGB_NODE, okay)
const anjay_dm_object_def_t **led_color_light_object_create(void);
void led_color_light_object_release(const anjay_dm_object_def_t ***out_def);

const anjay_dm_object_def_t **device_object_create(void);
void device_object_release(const anjay_dm_object_def_t ***out_def);
void device_object_update(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *def);
