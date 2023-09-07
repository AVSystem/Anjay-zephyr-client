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

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#define RGB_NODE DT_ALIAS(rgb_pwm)
#define LED_COLOR_LIGHT_AVAILABLE DT_NODE_HAS_STATUS(RGB_NODE, okay)

#define PUSH_BUTTON_NODE(idx) DT_ALIAS(push_button_##idx)
#define PUSH_BUTTON_AVAILABLE(idx) DT_NODE_HAS_STATUS(PUSH_BUTTON_NODE(idx), okay)
#define PUSH_BUTTON_AVAILABLE_ANY                                                                  \
	(PUSH_BUTTON_AVAILABLE(0) || PUSH_BUTTON_AVAILABLE(1) || PUSH_BUTTON_AVAILABLE(2) ||       \
	 PUSH_BUTTON_AVAILABLE(3))

#define SWITCH_NODE(idx) DT_ALIAS(switch_##idx)
#define SWITCH_AVAILABLE(idx) DT_NODE_HAS_STATUS(SWITCH_NODE(idx), okay)
#define SWITCH_AVAILABLE_ANY (SWITCH_AVAILABLE(0) || SWITCH_AVAILABLE(1) || SWITCH_AVAILABLE(2))

#define PUSH_BUTTON_GLUE_ITEM(num)                                                                 \
	{                                                                                          \
		.device = DEVICE_DT_GET(DT_GPIO_CTLR(PUSH_BUTTON_NODE(num), gpios)),               \
		.gpio_pin = DT_GPIO_PIN(PUSH_BUTTON_NODE(num), gpios),                             \
		.gpio_flags = (GPIO_INPUT | DT_GPIO_FLAGS(PUSH_BUTTON_NODE(num), gpios))           \
	}

#define SWITCH_BUTTON_GLUE_ITEM(num)                                                               \
	{                                                                                          \
		.device = DEVICE_DT_GET(DT_GPIO_CTLR(SWITCH_NODE(num), gpios)),                    \
		.gpio_pin = DT_GPIO_PIN(SWITCH_NODE(num), gpios),                                  \
		.gpio_flags = (GPIO_INPUT | DT_GPIO_FLAGS(SWITCH_NODE(num), gpios))                \
	}

#define TEMPERATURE_0_NODE DT_ALIAS(temperature_0)
#define TEMPERATURE_0_AVAILABLE DT_NODE_HAS_STATUS(TEMPERATURE_0_NODE, okay)

#define TEMPERATURE_1_NODE DT_ALIAS(temperature_1)
#define TEMPERATURE_1_AVAILABLE DT_NODE_HAS_STATUS(TEMPERATURE_1_NODE, okay)
