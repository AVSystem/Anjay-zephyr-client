/*
 * Copyright 2020-2022 AVSystem <avsystem@avsystem.com>
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

#include <devicetree.h>

#define STATUS_LED_NODE DT_ALIAS(status_led)
#define STATUS_LED_AVAILABLE DT_NODE_HAS_STATUS(STATUS_LED_NODE, okay)

void status_led_init(void);

void status_led_on(void);

void status_led_off(void);

void status_led_toggle(void);
