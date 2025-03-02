/*
 * Copyright 2020-2025 AVSystem <avsystem@avsystem.com>
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

#include <zephyr/devicetree.h>

#define WATER_PUMP_0_NODE DT_ALIAS(water_pump_0)
#define WATER_PUMP_0_AVAILABLE DT_NODE_HAS_STATUS(WATER_PUMP_0_NODE, okay)

#define BUTTON_0_NODE DT_ALIAS(push_button_0)
#define BUTTON_0_AVAILABLE DT_NODE_HAS_STATUS(BUTTON_0_NODE, okay)

#if WATER_PUMP_0_AVAILABLE
int water_pump_initialize(void);

const anjay_dm_object_def_t **power_control_object_create(void);
void power_control_object_release(const anjay_dm_object_def_t **def);

#endif // WATER_PUMP_0_AVAILABLE
