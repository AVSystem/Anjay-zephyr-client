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

/**
 * Generated by anjay_codegen.py on 2021-08-02 12:50:18
 *
 * LwM2M Object: Pattern detector
 * ID: 33650, URN: urn:oma:lwm2m:ext:33650, Optional, Multiple
 *
 * This object is used to report the pattern detected by the ML-based
 * classification algorithms and to count the number of times it has been
 * detected.
 */
#include <assert.h>
#include <stdbool.h>

#include <anjay/anjay.h>
#include <avsystem/commons/avs_defs.h>
#include <avsystem/commons/avs_list.h>
#include <avsystem/commons/avs_memory.h>

#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

#include "../led.h"
#include "objects.h"
#include <ei_wrapper.h>

LOG_MODULE_REGISTER(pattern_detector);

/**
 * Detector State: R, Single, Mandatory
 * type: boolean, range: N/A, unit: N/A
 * The current state of a detector.
 */
#define RID_DETECTOR_STATE 2000

/**
 * Detector Counter: R, Single, Mandatory
 * type: integer, range: N/A, unit: N/A
 * The cumulative value of patterns detected.
 */
#define RID_DETECTOR_COUNTER 2001

/**
 * Pattern Name: R, Single, Mandatory
 * type: string, range: N/A, unit: N/A
 * Name of the pattern being detected.
 */
#define RID_PATTERN_NAME 2002

#define ACCELEROMETER_NODE DT_INST(0, adi_adxl362)

#if !DT_NODE_HAS_STATUS(ACCELEROMETER_NODE, okay)
#error "ADXL362 not available"
#endif // !DT_NODE_HAS_STATUS(ACCELEROMETER_NODE, okay)

#define SENSOR_CHANNEL SENSOR_CHAN_ACCEL_XYZ
#define CH_COUNT 3

struct pattern_detector_instance_state {
	bool detector_state;
	int32_t detector_counter;
};

struct pattern_detector_instance {
	anjay_iid_t iid;

	struct pattern_detector_instance_state curr_state;
	struct pattern_detector_instance_state cached_state;
	const char *pattern_name;
};

struct pattern_detector_object {
	const anjay_dm_object_def_t *def;
	const struct device *dev;

	struct pattern_detector_instance *instances;
	struct k_mutex instance_state_mtx;

	struct k_work_delayable measure_accel_dwork;
	struct k_work_sync sync;
	int64_t last_run_timestamp;
};

static const struct pattern_detector_instance_state initial_state;

#define SYNCHRONIZED(Mtx)                                                                          \
	for (int _synchronized_exit = k_mutex_lock(&(Mtx), K_FOREVER); !_synchronized_exit;        \
	     _synchronized_exit = -1, k_mutex_unlock(&(Mtx)))

static struct pattern_detector_object *installed_obj;
static bool wrapper_initialized;

static void schedule_next_measure(struct pattern_detector_object *obj)
{
	const int64_t next_run_timestamp =
		obj->last_run_timestamp + 1000 / ei_wrapper_get_classifier_frequency();
	const int64_t curr_time = k_uptime_get();
	k_timeout_t next_run_delay;

	if (next_run_timestamp <= curr_time) {
		LOG_DBG("Pattern detector's timer has slipped");

		next_run_delay = K_NO_WAIT;
		obj->last_run_timestamp = curr_time;
	} else {
		next_run_delay = K_MSEC(next_run_timestamp - curr_time);
		obj->last_run_timestamp = next_run_timestamp;
	}

	k_work_schedule(&obj->measure_accel_dwork, next_run_delay);
}

static struct pattern_detector_instance *find_instance(const struct pattern_detector_object *obj,
						       anjay_iid_t iid)
{
	return iid < ei_wrapper_get_classifier_label_count() ? &obj->instances[iid] : NULL;
}

static void result_ready_cb(int err)
{
	assert(installed_obj);
	if (err) {
		LOG_ERR("Edge Impulse Result ready callback returned error (err: %d)", err);
		return;
	}

	const char *label;
	float value;
	size_t res;

	// Results are ordered based on descending classification value.
	// First and the most probable classification result is used.
	err = ei_wrapper_get_next_classification_result(&label, &value, &res);

	if (err == 0) {
		SYNCHRONIZED(installed_obj->instance_state_mtx)
		{
			LOG_INF("Edge Impulse classified: %.2f, Label: %s", value, label);

			for (size_t i = 0; i < ei_wrapper_get_classifier_label_count(); i++) {
				struct pattern_detector_instance *it = &installed_obj->instances[i];

				if (it->iid == res) {
					it->curr_state.detector_state = true;
					it->curr_state.detector_counter++;
					led_on(i);
				} else {
					it->curr_state.detector_state = false;
					led_off(i);
				}
			}
		}
	} else {
		LOG_ERR("Edge Impulse cannot get classification results (err: %d)", err);
	}

	// Invocation of ei_wrapper_start_prediction restarts prediction results.
	err = ei_wrapper_start_prediction(1, 0);
	if (err) {
		LOG_INF("Edge Impulse cannot start prediction (err: %d)", err);
	} else {
		LOG_INF("Edge Impulse prediction started...");
	}
}

static void measure_accel_handler(struct k_work *work)
{
	assert(ei_wrapper_get_frame_size() == CH_COUNT);

	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct pattern_detector_object *obj =
		AVS_CONTAINER_OF(dwork, struct pattern_detector_object, measure_accel_dwork);

	struct sensor_value values[CH_COUNT];

	if (sensor_sample_fetch_chan(obj->dev, SENSOR_CHANNEL) ||
	    sensor_channel_get(obj->dev, SENSOR_CHANNEL, values)) {
		LOG_ERR("Failed to fetch accelerometer data");
		return;
	}

	float fvalues[CH_COUNT];

	for (size_t i = 0; i < CH_COUNT; i++) {
		fvalues[i] = (float)sensor_value_to_double(&values[i]);
	}

	int err = ei_wrapper_add_data(fvalues, CH_COUNT);

	if (err) {
		LOG_ERR("Cannot provide input data (err: %d)", err);
		LOG_ERR("Increase CONFIG_EI_WRAPPER_DATA_BUF_SIZE");
		return;
	}

	schedule_next_measure(obj);
}

static inline struct pattern_detector_object *get_obj(const anjay_dm_object_def_t *const *obj_ptr)
{
	assert(obj_ptr);
	return AVS_CONTAINER_OF(obj_ptr, struct pattern_detector_object, def);
}

static int list_instances(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_dm_list_ctx_t *ctx)
{
	(void)anjay;

	for (size_t i = 0; i < ei_wrapper_get_classifier_label_count(); i++) {
		anjay_dm_emit(ctx, i);
	}

	return 0;
}

static int init_instance(struct pattern_detector_instance *inst, anjay_iid_t iid)
{
	assert(iid != ANJAY_ID_INVALID);
	assert(iid < ei_wrapper_get_classifier_label_count());

	inst->iid = iid;
	inst->curr_state = initial_state;
	inst->cached_state = initial_state;
	inst->pattern_name = ei_wrapper_get_classifier_label(iid);

	return 0;
}

static struct pattern_detector_instance *add_instance(struct pattern_detector_object *obj,
						      anjay_iid_t iid)
{
	struct pattern_detector_instance *created = &obj->instances[iid];

	return init_instance(created, iid) ? NULL : created;
}

static int list_resources(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid, anjay_dm_resource_list_ctx_t *ctx)
{
	(void)anjay;
	(void)obj_ptr;
	(void)iid;

	anjay_dm_emit_res(ctx, RID_DETECTOR_STATE, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_DETECTOR_COUNTER, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_PATTERN_NAME, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	return 0;
}

static int resource_read(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			 anjay_iid_t iid, anjay_rid_t rid, anjay_riid_t riid,
			 anjay_output_ctx_t *ctx)
{
	(void)anjay;

	struct pattern_detector_object *obj = get_obj(obj_ptr);

	assert(obj);

	struct pattern_detector_instance *inst = find_instance(obj, iid);

	assert(inst);

	switch (rid) {
	case RID_DETECTOR_STATE:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_bool(ctx, inst->cached_state.detector_state);

	case RID_DETECTOR_COUNTER:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_i32(ctx, inst->cached_state.detector_counter);

	case RID_PATTERN_NAME:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_string(ctx, inst->pattern_name);

	default:
		return ANJAY_ERR_METHOD_NOT_ALLOWED;
	}
}

static const anjay_dm_object_def_t obj_def = { .oid = 33650,
					       .handlers = {
						       .list_instances = list_instances,

						       .list_resources = list_resources,
						       .resource_read = resource_read,
					       } };

const anjay_dm_object_def_t **pattern_detector_object_create(void)
{
	assert(installed_obj == NULL);

	static const struct device *dev = DEVICE_DT_GET(ACCELEROMETER_NODE);

	if (!device_is_ready(dev)) {
		return NULL;
	}

	int err;

	if (!wrapper_initialized) {
		err = ei_wrapper_init(result_ready_cb);
		if (err) {
			LOG_INF("Edge Impulse wrapper failed to initialize (err: %d)", err);
			return NULL;
		}

		LOG_INF("Edge Impulse wrapper initialized.");
		LOG_INF("FRAME SIZE: %d", ei_wrapper_get_frame_size());
		LOG_INF("WINDOW SIZE: %d", ei_wrapper_get_window_size());
		LOG_INF("FREQUENCY: %d", ei_wrapper_get_classifier_frequency());
		LOG_INF("LABELS: %d", ei_wrapper_get_classifier_label_count());
		wrapper_initialized = true;
	}

	// conterintuitively it's called upfront to simplify cleanup
	err = ei_wrapper_start_prediction(0, 0);
	if (err) {
		LOG_INF("Edge Impulse cannot schedule prediction (err: %d)", err);
	}
	LOG_INF("Edge Impulse prediction scheduled...");

	struct pattern_detector_object *obj = (struct pattern_detector_object *)avs_calloc(
		1, sizeof(struct pattern_detector_object));
	if (!obj) {
		return NULL;
	}
	obj->def = &obj_def;
	obj->dev = dev;

	obj->instances = (struct pattern_detector_instance *)avs_calloc(
		ei_wrapper_get_classifier_label_count(), sizeof(struct pattern_detector_instance));
	if (!obj->instances) {
		avs_free(obj);
		return NULL;
	}

	for (size_t i = 0; i < ei_wrapper_get_classifier_label_count(); i++) {
		if (!add_instance(obj, i)) {
			avs_free(obj->instances);
			avs_free(obj);
			return NULL;
		}
	}

	// Edge Impulse wrapper is connected to one object
	installed_obj = obj;

	k_mutex_init(&obj->instance_state_mtx);

	k_work_init_delayable(&obj->measure_accel_dwork, measure_accel_handler);
	obj->last_run_timestamp = k_uptime_get();
	schedule_next_measure(obj);

	return &obj->def;
}

void pattern_detector_object_update(anjay_t *anjay, const anjay_dm_object_def_t *const *def)
{
	if (!anjay || !def) {
		return;
	}

	struct pattern_detector_object *obj = get_obj(def);

	SYNCHRONIZED(obj->instance_state_mtx)
	{
		for (size_t i = 0; i < ei_wrapper_get_classifier_label_count(); i++) {
			struct pattern_detector_instance *it = &obj->instances[i];

			if (it->cached_state.detector_state != it->curr_state.detector_state) {
				it->cached_state.detector_state = it->curr_state.detector_state;
				anjay_notify_changed(anjay, obj->def->oid, i, RID_DETECTOR_STATE);
			}

			if (it->cached_state.detector_counter != it->curr_state.detector_counter) {
				it->cached_state.detector_counter = it->curr_state.detector_counter;
				anjay_notify_changed(anjay, obj->def->oid, i, RID_DETECTOR_COUNTER);
			}
		}
	}
}

void pattern_detector_object_release(const anjay_dm_object_def_t **def)
{
	if (def) {
		struct pattern_detector_object *obj = get_obj(def);

		k_work_cancel_delayable_sync(&obj->measure_accel_dwork, &obj->sync);

		bool cancelled;

		while (ei_wrapper_clear_data(&cancelled) == -EBUSY) {
			k_sleep(K_MSEC(25));
		}

		installed_obj = NULL;

		avs_free(obj->instances);
		avs_free(obj);
	}
}
