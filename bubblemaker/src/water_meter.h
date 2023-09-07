/*
 * Copyright 2020-2023 AVSystem <avsystem@avsystem.com>
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

#define WATER_METER_0_NODE DT_ALIAS(water_meter_0)
#define WATER_METER_1_NODE DT_ALIAS(water_meter_1)

#define WATER_METER_0_AVAILABLE DT_NODE_HAS_STATUS(WATER_METER_0_NODE, okay)
#define WATER_METER_1_AVAILABLE DT_NODE_HAS_STATUS(WATER_METER_1_NODE, okay)

#if !WATER_METER_0_AVAILABLE && !WATER_METER_1_AVAILABLE
#error "No water meter has been found in the devicetree"
#endif // !WATER_METER_0_AVAILABLE && !WATER_METER_1_AVAILABLE

int water_meter_init(void);
void water_meter_instances_reset(void);
bool water_meter_is_null(void);
#if WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE
void water_meter_get_cumulated_volumes(double *out_result);
#endif // WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE

const anjay_dm_object_def_t **water_meter_object_create(void);
void water_meter_object_release(const anjay_dm_object_def_t **def);
