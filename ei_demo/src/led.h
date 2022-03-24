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

#include <stdint.h>

#include <device.h>
#include <devicetree.h>

#define LED_NODE(n) DT_ALIAS(led##n)
#define LED_AVAILABLE(n) DT_NODE_HAS_STATUS(LED_NODE(n), okay)

#define LED_DEV(n) DEVICE_DT_GET(DT_GPIO_CTLR(LED_NODE(n), gpios))
#define LED_PIN(n) DT_GPIO_PIN(LED_NODE(n), gpios)
#define LED_FLAGS(n) (GPIO_OUTPUT_INACTIVE | DT_GPIO_FLAGS(LED_NODE(n), gpios))

void led_init(void);
void led_on(int led);
void led_off(int led);
