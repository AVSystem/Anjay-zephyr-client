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

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bubblemaker.h"
#include "water_pump.h"
#include "led_strip.h"
#include "water_meter.h"

LOG_MODULE_REGISTER(bubblemaker);

#define IDLE_STATE_DURATION K_MSEC(500)
#define RED_LIGHT_DURATION K_SECONDS(2)
#define YELLOW_LIGHT_DURATION K_SECONDS(1)
#define MEASURE_STATE_DURATION K_SECONDS(10)
#define END_STATE_DURATION K_SECONDS(3)

static struct k_thread bubblemaker_thread;
static K_THREAD_STACK_DEFINE(bubblemaker_stack, 1024);

enum bubblemaker_state bm_state = BUBBLEMAKER_IDLE;

static void run_bubblemaker(void *arg1, void *arg2, void *arg3)
{
	LOG_INF("Waiting for Water meter instances to initialize...");

	while (water_meter_is_null()) {
		k_sleep(K_SECONDS(1));
	}

	while (1) {
		switch (bm_state) {
		case BUBBLEMAKER_IDLE:
			k_sleep(IDLE_STATE_DURATION);
			break;
		case BUBBLEMAKER_START_RED_LIGHT:
			k_sleep(RED_LIGHT_DURATION);
			bm_state = BUBBLEMAKER_START_YELLOW_LIGHT;
			break;
		case BUBBLEMAKER_START_YELLOW_LIGHT:
			k_sleep(YELLOW_LIGHT_DURATION);
			bm_state = BUBBLEMAKER_MEASURE;
			break;
		case BUBBLEMAKER_MEASURE:
			water_meter_instances_reset();
			k_sleep(MEASURE_STATE_DURATION);
#if WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE
			double out_volumes[2];

			water_meter_get_cumulated_volumes(out_volumes);
			bm_state = out_volumes[0] > out_volumes[1] ? BUBBLEMAKER_END_P1_WON :
								     BUBBLEMAKER_END_P2_WON;
#else // WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE
			bm_state = BUBBLEMAKER_END;
#endif // WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE
			break;
#if WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE
		case BUBBLEMAKER_END_P1_WON:
		case BUBBLEMAKER_END_P2_WON:
			water_meter_instances_reset();
			k_sleep(END_STATE_DURATION);
			bm_state = BUBBLEMAKER_IDLE;
			break;
#else // WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE
		case BUBBLEMAKER_END:
			k_sleep(END_STATE_DURATION);
			bm_state = BUBBLEMAKER_IDLE;
			break;
#endif // WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE
		default:
			AVS_UNREACHABLE("Invalid enum value");
			break;
		}
	}
}

int bubblemaker_init(void)
{
	LOG_INF("Initializing Bubblemaker");

	if (led_strip_init() || water_meter_init()
#if WATER_PUMP_0_AVAILABLE
	    || water_pump_initialize()
#endif // WATER_PUMP_0_AVAILABLE
	) {
		return -1;
	}

	if (!k_thread_create(&bubblemaker_thread, bubblemaker_stack,
			     K_THREAD_STACK_SIZEOF(bubblemaker_stack), run_bubblemaker, NULL, NULL,
			     NULL, 2, 0, K_NO_WAIT)) {
		LOG_ERR("Failed to create bubblemaker thread");
		return -1;
	}

	return 0;
}
