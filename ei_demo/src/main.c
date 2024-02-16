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

#include <anjay_zephyr/config.h>
#include <anjay_zephyr/lwm2m.h>
#include <anjay_zephyr/objects.h>

#include "objects/objects.h"
#include "led.h"

static const anjay_dm_object_def_t **pattern_detector_obj;

static avs_sched_handle_t update_objects_handle;

static int register_objects(anjay_t *anjay)
{
	pattern_detector_obj = pattern_detector_object_create();
	if (pattern_detector_obj) {
		anjay_register_object(anjay, pattern_detector_obj);
	}

	return 0;
}

static void update_objects(avs_sched_t *sched, const void *anjay_ptr)
{
	anjay_t *anjay = *(anjay_t *const *)anjay_ptr;

	pattern_detector_object_update(anjay, pattern_detector_obj);

	AVS_SCHED_DELAYED(sched, &update_objects_handle,
			  avs_time_duration_from_scalar(1, AVS_TIME_S), update_objects, &anjay,
			  sizeof(anjay));
}

static int release_objects(void)
{
	pattern_detector_object_release(pattern_detector_obj);

	return 0;
}

static int init_update_objects(anjay_t *anjay)
{
	avs_sched_t *sched = anjay_get_scheduler(anjay);

	update_objects(sched, &anjay);

	return 0;
}

static int clean_before_anjay_destroy(anjay_t *anjay)
{
	avs_sched_del(&update_objects_handle);

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
	led_init();

	anjay_zephyr_lwm2m_set_user_callback(lwm2m_callback);

	anjay_zephyr_lwm2m_init_from_settings();
	anjay_zephyr_lwm2m_start();

	// Anjay runs in a separate thread and preceding function doesn't block
	// add your own code here
	return 0;
}
