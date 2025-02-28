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

#include "status_led.h"

#if STATUS_LED_AVAILABLE

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(status_led);

static const struct gpio_dt_spec status_led_spec = GPIO_DT_SPEC_GET(STATUS_LED_NODE, gpios);

void status_led_init(void)
{
	if (!device_is_ready(status_led_spec.port) ||
	    gpio_pin_configure_dt(&status_led_spec, GPIO_OUTPUT_INACTIVE)) {
		LOG_WRN("failed to initialize status led");
	}
}

static void status_led_set(int value)
{
	if (device_is_ready(status_led_spec.port)) {
		gpio_pin_set_dt(&status_led_spec, value);
	}
}

void status_led_on(void)
{
	status_led_set(1);
}

void status_led_off(void)
{
	status_led_set(0);
}

void status_led_toggle(void)
{
	if (device_is_ready(status_led_spec.port)) {
		gpio_pin_toggle_dt(&status_led_spec);
	}
}
#else // STATUS_LED_AVAILABLE

void status_led_init(void)
{
}

void status_led_on(void)
{
}

void status_led_off(void)
{
}

void status_led_toggle(void)
{
}
#endif // STATUS_LED_AVAILABLE
