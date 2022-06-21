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

#include <stdbool.h>

#include <drivers/gpio.h>

#include "led.h"

void led_init(void)
{
	if (device_is_ready(LED_DEV(0))) {
		gpio_pin_configure(LED_DEV(0), LED_PIN(0), LED_FLAGS(0));
	}
	if (device_is_ready(LED_DEV(1))) {
		gpio_pin_configure(LED_DEV(1), LED_PIN(1), LED_FLAGS(1));
	}
	if (device_is_ready(LED_DEV(2))) {
		gpio_pin_configure(LED_DEV(2), LED_PIN(2), LED_FLAGS(2));
	}
}

static void led_set(int led, bool state)
{
	switch (led) {
	case 0:
		if (device_is_ready(LED_DEV(0))) {
			gpio_pin_set(LED_DEV(0), LED_PIN(0), state);
		}
		break;
	case 1:
		if (device_is_ready(LED_DEV(1))) {
			gpio_pin_set(LED_DEV(1), LED_PIN(1), state);
		}
		break;
	case 2:
		if (device_is_ready(LED_DEV(2))) {
			gpio_pin_set(LED_DEV(2), LED_PIN(2), state);
		}
		break;
	default:
		break;
	}
}

void led_on(int led)
{
	led_set(led, true);
}

void led_off(int led)
{
	led_set(led, false);
}
