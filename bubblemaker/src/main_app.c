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

#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <anjay_zephyr/config.h>
#include <anjay_zephyr/lwm2m.h>
#include <anjay_zephyr/objects.h>

#include "peripherals.h"
#include "status_led.h"
#include "sensors.h"
#include "bubblemaker.h"
#include "water_pump.h"

LOG_MODULE_REGISTER(main_app);

static const anjay_dm_object_def_t **water_meter_obj;
#if WATER_PUMP_0_AVAILABLE
static const anjay_dm_object_def_t **power_control_obj;
#endif // WATER_PUMP_0_AVAILABLE
#if LED_COLOR_LIGHT_AVAILABLE
static const anjay_dm_object_def_t **led_color_light_obj;
#endif // LED_COLOR_LIGHT_AVAILABLE
#if SWITCH_AVAILABLE_ANY
static const anjay_dm_object_def_t **switch_obj;
#endif // SWITCH_AVAILABLE_ANY
static avs_sched_handle_t update_objects_handle;

#if PUSH_BUTTON_AVAILABLE_ANY
static struct anjay_zephyr_ipso_button_instance buttons[] = {
#if PUSH_BUTTON_AVAILABLE(0) && !WATER_PUMP_0_AVAILABLE
	PUSH_BUTTON_GLUE_ITEM(0),
#endif // PUSH_BUTTON_AVAILABLE(0) && !WATER_PUMP_0_AVAILABLE
#if PUSH_BUTTON_AVAILABLE(1)
	PUSH_BUTTON_GLUE_ITEM(1),
#endif // PUSH_BUTTON_AVAILABLE(1)
#if PUSH_BUTTON_AVAILABLE(2)
	PUSH_BUTTON_GLUE_ITEM(2),
#endif // PUSH_BUTTON_AVAILABLE(2)
#if PUSH_BUTTON_AVAILABLE(3)
	PUSH_BUTTON_GLUE_ITEM(3),
#endif // PUSH_BUTTON_AVAILABLE(3)
};
#endif // PUSH_BUTTON_AVAILABLE_ANY

#if SWITCH_AVAILABLE_ANY
static struct anjay_zephyr_switch_instance switches[] = {
#if SWITCH_AVAILABLE(0)
	SWITCH_BUTTON_GLUE_ITEM(0),
#endif // SWITCH_AVAILABLE(0)
#if SWITCH_AVAILABLE(1)
	SWITCH_BUTTON_GLUE_ITEM(1),
#endif // SWITCH_AVAILABLE(1)
#if SWITCH_AVAILABLE(2)
	SWITCH_BUTTON_GLUE_ITEM(2),
#endif // SWITCH_AVAILABLE(2)
};
#endif // SWITCH_AVAILABLE_ANY

static int register_objects(anjay_t *anjay)
{
	water_meter_obj = water_meter_object_create();
	if (water_meter_obj) {
		anjay_register_object(anjay, water_meter_obj);
	} else {
		LOG_ERR("water_meter object could not be created");
		return -1;
	}

#if WATER_PUMP_0_AVAILABLE
	power_control_obj = power_control_object_create();
	if (power_control_obj) {
		anjay_register_object(anjay, power_control_obj);
	}
#endif // WATER_PUMP_0_AVAILABLE

	basic_sensor_objects_install(anjay);
#if PUSH_BUTTON_AVAILABLE_ANY
	anjay_zephyr_ipso_push_button_object_install(anjay, buttons, AVS_ARRAY_SIZE(buttons));
#endif // PUSH_BUTTON_AVAILABLE_ANY

#if LED_COLOR_LIGHT_AVAILABLE
	led_color_light_obj = anjay_zephyr_led_color_light_object_create(DEVICE_DT_GET(RGB_NODE));
	if (led_color_light_obj) {
		anjay_register_object(anjay, led_color_light_obj);
	}
#endif // LED_COLOR_LIGHT_AVAILABLE

#if SWITCH_AVAILABLE_ANY
	switch_obj = anjay_zephyr_switch_object_create(switches, AVS_ARRAY_SIZE(switches));
	if (switch_obj) {
		anjay_register_object(anjay, switch_obj);
	}
#endif // SWITCH_AVAILABLE_ANY
	return 0;
}

static void update_objects_frequent(anjay_t *anjay)
{
#if SWITCH_AVAILABLE_ANY
	anjay_zephyr_switch_object_update(anjay, switch_obj);
#endif // SWITCH_AVAILABLE_ANY
	basic_sensor_objects_update(anjay);
}

static void update_objects(avs_sched_t *sched, const void *anjay_ptr)
{
	anjay_t *anjay = *(anjay_t *const *)anjay_ptr;

	update_objects_frequent(anjay);

	status_led_toggle();

	AVS_SCHED_DELAYED(sched, &update_objects_handle,
			  avs_time_duration_from_scalar(1, AVS_TIME_S), update_objects, &anjay,
			  sizeof(anjay));
}

static int init_update_objects(anjay_t *anjay)
{
	avs_sched_t *sched = anjay_get_scheduler(anjay);

	update_objects(sched, &anjay);

	status_led_init();

	return 0;
}

static int clean_before_anjay_destroy(anjay_t *anjay)
{
	avs_sched_del(&update_objects_handle);

	return 0;
}

static int release_objects(void)
{
	water_meter_object_release(water_meter_obj);
#if WATER_PUMP_0_AVAILABLE
	power_control_object_release(power_control_obj);
#endif // WATER_PUMP_0_AVAILABLE
#if SWITCH_AVAILABLE_ANY
	anjay_zephyr_switch_object_release(&switch_obj);
#endif // SWITCH_AVAILABLE_ANY
#if LED_COLOR_LIGHT_AVAILABLE
	anjay_zephyr_led_color_light_object_release(&led_color_light_obj);
#endif // LED_COLOR_LIGHT_AVAILABLE

	return 0;
}

int lwm2m_callback(anjay_t *anjay, enum anjay_zephyr_lwm2m_callback_reasons reason)
{
	switch (reason) {
	case ANJAY_ZEPHYR_LWM2M_CALLBACK_REASON_INIT:
		return register_objects(anjay);
	case ANJAY_ZEPHYR_LWM2M_CALLBACK_REASON_ANJAY_READY:
		return init_update_objects(anjay);
	case ANJAY_ZEPHYR_LWM2M_CALLBACK_REASON_ANJAY_SHUTTING_DOWN:
		return clean_before_anjay_destroy(anjay);
	case ANJAY_ZEPHYR_LWM2M_CALLBACK_REASON_CLEANUP:
		return release_objects();
	default:
		return -1;
	}
}

int main(void)
{
	LOG_INF("Initializing Anjay-zephyr-client Bubblemaker " CONFIG_ANJAY_ZEPHYR_VERSION);

	anjay_zephyr_lwm2m_set_user_callback(lwm2m_callback);
	anjay_zephyr_lwm2m_init_from_settings();
	anjay_zephyr_lwm2m_start();
	bubblemaker_init();

	// Anjay runs in a separate thread and preceding function doesn't block
	// add your own code here
	return 0;
}
