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

#include "led.h"

#include <drivers/gpio.h>

#include <avsystem/commons/avs_log.h>

#define LED1_GPIO_PORT DT_GPIO_LABEL(DT_ALIAS(led1), gpios)
#define LED1_GPIO_PIN DT_GPIO_PIN(DT_ALIAS(led1), gpios)
#define LED1_GPIO_FLAGS DT_GPIO_FLAGS(DT_ALIAS(led1), gpios)

#define LED2_GPIO_PORT DT_GPIO_LABEL(DT_ALIAS(led0), gpios)
#define LED2_GPIO_PIN DT_GPIO_PIN(DT_ALIAS(led0), gpios)
#define LED2_GPIO_FLAGS DT_GPIO_FLAGS(DT_ALIAS(led0), gpios)

static struct device *LED1_DEVICE;
static struct device *LED2_DEVICE;

void disco_led_init(void) {
    LED1_DEVICE = device_get_binding(LED1_GPIO_PORT);
    if (!LED1_DEVICE
            || gpio_pin_configure(LED1_DEVICE, LED1_GPIO_PIN,
                                  LED1_GPIO_FLAGS | GPIO_OUTPUT_INACTIVE)) {
        LED1_DEVICE = NULL;
        avs_log(led, WARNING, "failed to initialize LED1");
    }

    LED2_DEVICE = device_get_binding(LED2_GPIO_PORT);
    if (!LED2_DEVICE
            || gpio_pin_configure(LED2_DEVICE, LED2_GPIO_PIN,
                                  LED2_GPIO_FLAGS | GPIO_OUTPUT_INACTIVE)) {
        LED2_DEVICE = NULL;
        avs_log(led, WARNING, "failed to initialize LED2");
    }
}

static void led_set(disco_led_t led, int value) {
    switch (led) {
    case DISCO_LED1:
        if (LED1_DEVICE) {
            gpio_pin_set(LED1_DEVICE, LED1_GPIO_PIN, value);
        }
        break;
    case DISCO_LED2:
        if (LED2_DEVICE) {
            gpio_pin_set(LED2_DEVICE, LED2_GPIO_PIN, value);
        }
        break;
    }
}

void disco_led_on(disco_led_t led) {
    led_set(led, 1);
}

void disco_led_off(disco_led_t led) {
    led_set(led, 0);
}

void disco_led_toggle(disco_led_t led) {
    switch (led) {
    case DISCO_LED1:
        if (LED1_DEVICE) {
            gpio_pin_toggle(LED1_DEVICE, LED1_GPIO_PIN);
        }
        break;
    case DISCO_LED2:
        if (LED2_DEVICE) {
            gpio_pin_toggle(LED2_DEVICE, LED2_GPIO_PIN);
        }
        break;
    }
}
