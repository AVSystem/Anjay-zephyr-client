/*
 * Copyright 2020-2024 AVSystem <avsystem@avsystem.com>
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

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/drivers/led_strip.h>

#include "led_strip.h"
#include "bubblemaker.h"
#include "water_meter.h"

#if LED_STRIP_AVAILABLE

LOG_MODULE_REGISTER(led_strip);

#define STRIP_NODE DT_ALIAS(led_strip)
#define STRIP_NUM_PIXELS DT_PROP(STRIP_NODE, chain_length)
#define RGB(_r, _g, _b) { .r = (_r), .g = (_g), .b = (_b) }

static struct k_thread led_strip_thread;
static K_THREAD_STACK_DEFINE(led_strip_stack, 1024);
static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);
struct led_rgb pixels[STRIP_NUM_PIXELS];

enum uniform_colors {
	STRIP_COLOR_RED,
	STRIP_COLOR_GREEN,
	STRIP_COLOR_BLUE,
	STRIP_COLOR_YELLOW,
#if WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE
	STRIP_COLOR_P1,
	STRIP_COLOR_P2,
#endif // WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE
	STRIP_COLOR_NONE
};

static const struct led_rgb colors[] = {
	[STRIP_COLOR_RED] = RGB(0xff, 0x00, 0x00),  [STRIP_COLOR_GREEN] = RGB(0x00, 0xff, 0x00),
	[STRIP_COLOR_BLUE] = RGB(0x00, 0x00, 0xff), [STRIP_COLOR_YELLOW] = RGB(0xff, 0xff, 0x00),
#if WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE
	[STRIP_COLOR_P1] = RGB(0x62, 0x09, 0xff),   [STRIP_COLOR_P2] = RGB(0x00, 0xb9, 0xe2),
#endif // WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE
	[STRIP_COLOR_NONE] = RGB(0x00, 0x00, 0x00),
};

static void ws2812_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint8_t *r, uint8_t *g,
				 uint8_t *b)
{
	h %= 360;
	uint32_t rgb_max = v * 255 / 100;
	uint32_t rgb_min = rgb_max * (100 - s) / 100;

	uint32_t i = h / 60;
	uint32_t diff = h % 60;

	uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

	switch (i) {
	case 0:
		*r = rgb_max;
		*g = rgb_min + rgb_adj;
		*b = rgb_min;
		break;
	case 1:
		*r = rgb_max - rgb_adj;
		*g = rgb_max;
		*b = rgb_min;
		break;
	case 2:
		*r = rgb_min;
		*g = rgb_max;
		*b = rgb_min + rgb_adj;
		break;
	case 3:
		*r = rgb_min;
		*g = rgb_max - rgb_adj;
		*b = rgb_max;
		break;
	case 4:
		*r = rgb_min + rgb_adj;
		*g = rgb_min;
		*b = rgb_max;
		break;
	case 5:
		*r = rgb_max;
		*g = rgb_min;
		*b = rgb_max - rgb_adj;
		break;
	default:
		AVS_UNREACHABLE("Invalid value");
		break;
	}
}

static void ws2812_strip_update(void)
{
	led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
}

static void ws2812_strip_set_color(enum uniform_colors color)
{
	for (size_t i = 0; i < STRIP_NUM_PIXELS; i++) {
		pixels[i] = colors[color];
	}
	ws2812_strip_update();
}

static void ws2812_strip_display_rainbow(void)
{
	static uint32_t rgb_increase;

	for (int i = 0; i < 3; i++) {
		for (int j = i; j < STRIP_NUM_PIXELS; j++) {
			uint32_t hue = j * 360 / STRIP_NUM_PIXELS + rgb_increase;

			ws2812_strip_hsv2rgb(hue, 100, 100, &pixels[j].r, &pixels[j].g,
					     &pixels[j].b);
		}
		ws2812_strip_update();
		k_sleep(K_MSEC(10));
	}
	rgb_increase += 5;
}

static void led_strip_task(void *arg1, void *arg2, void *arg3)
{
	ws2812_strip_set_color(STRIP_COLOR_NONE);

	while (1) {
		switch (bm_state) {
		case BUBBLEMAKER_IDLE:
			ws2812_strip_display_rainbow();
			break;
		case BUBBLEMAKER_START_RED_LIGHT:
			ws2812_strip_set_color(STRIP_COLOR_RED);
			break;
		case BUBBLEMAKER_START_YELLOW_LIGHT:
			ws2812_strip_set_color(STRIP_COLOR_YELLOW);
			break;
		case BUBBLEMAKER_MEASURE:
			ws2812_strip_set_color(STRIP_COLOR_GREEN);
			break;
#if WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE
		case BUBBLEMAKER_END_P1_WON:
			ws2812_strip_set_color(STRIP_COLOR_P1);
			break;
		case BUBBLEMAKER_END_P2_WON:
			ws2812_strip_set_color(STRIP_COLOR_P2);
			break;
#else // WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE
		case BUBBLEMAKER_END:
			ws2812_strip_set_color(STRIP_COLOR_RED);
			break;
#endif // WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE
		default:
			AVS_UNREACHABLE("Invalid enum value");
		}
	}
}

int led_strip_init(void)
{
	LOG_INF("Initializing led_strip");

	if (device_is_ready(strip)) {
		LOG_INF("Found LED strip device %s", strip->name);
	} else {
		LOG_ERR("LED strip device %s is not ready", strip->name);
		return -1;
	}

	if (!k_thread_create(&led_strip_thread, led_strip_stack,
			     K_THREAD_STACK_SIZEOF(led_strip_stack), led_strip_task, NULL, NULL,
			     NULL, 2, 0, K_NO_WAIT)) {
		LOG_ERR("Failed to create led_strip thread");
		return -1;
	}

	return 0;
}

#endif // LED_STRIP_AVAILABLE
