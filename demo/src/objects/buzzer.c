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
#include <assert.h>
#include <float.h>
#include <stdbool.h>

#include <anjay/anjay.h>
#include <avsystem/commons/avs_defs.h>
#include <avsystem/commons/avs_list.h>
#include <avsystem/commons/avs_memory.h>

#include <devicetree.h>
#include <drivers/pwm.h>
#include <zephyr.h>

#include "../utils.h"
#include "objects.h"

/**
 * Delay Duration: RW, Single, Optional
 * type: float, range: N/A, unit: s
 * The duration of the time delay.
 */
#define RID_DELAY_DURATION 5521

/**
 * Minimum Off-time: RW, Single, Mandatory
 * type: float, range: N/A, unit: s
 * The duration of the rearm delay (i.e. the delay from the end of one
 * cycle until the beginning of the next, the inhibit time).
 */
#define RID_MINIMUM_OFF_TIME 5525

/**
 * Application Type: RW, Single, Optional
 * type: string, range: N/A, unit: N/A
 * The application type of the sensor or actuator as a string depending
 * on the use case.
 */
#define RID_APPLICATION_TYPE 5750

/**
 * On/Off: RW, Single, Mandatory
 * type: boolean, range: N/A, unit: N/A
 * On/off control. Boolean value where True is On and False is Off.
 */
#define RID_ON_OFF 5850

#if BUZZER_AVAILABLE
#define BUZZER_FREQ 2000
#define APPLICATION_TYPE_BUFLEN 64
#define TIMESTAMP_NONE -1

#define BUZZER_PIN DT_PROP(BUZZER_NODE, ch0_pin)

struct buzzer_state {
	bool on_off;
	double delay_duration;
	double minimum_off_time;
	char application_type[APPLICATION_TYPE_BUFLEN];
};

struct buzzer_object {
	const anjay_dm_object_def_t *def;

	const struct device *dev;

	struct k_work_delayable disable_buzzer_dwork;
	struct k_work_sync sync;
	int64_t last_run_end_timestamp;
	bool running_infinitely;

	// used to defer setting state.on_off to false until the
	// buzzer_object_update call
	// the concurring threads are:
	// - update_objects thread calling buzzer_object_update
	// - sysworkq thread calling buzzer_disable_work_handler
	struct k_mutex run_finished_mtx;
	bool run_finished;

	struct buzzer_state state;
	struct buzzer_state backup;
};

static const struct buzzer_state default_state;

static inline struct buzzer_object *get_obj(const anjay_dm_object_def_t *const *obj_ptr)
{
	assert(obj_ptr);
	return AVS_CONTAINER_OF(obj_ptr, struct buzzer_object, def);
}

static int buzzer_enable(struct buzzer_object *obj)
{
	static const uint32_t period = USEC_PER_SEC / BUZZER_FREQ;
	static const uint32_t pulse = period / 2;

	return pwm_pin_set_usec(obj->dev, BUZZER_PIN, period, pulse, 0);
}

static int buzzer_disable(struct buzzer_object *obj)
{
	return pwm_pin_set_usec(obj->dev, BUZZER_PIN, 0, 0, 0);
}

static void buzzer_disable_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct buzzer_object *obj =
		AVS_CONTAINER_OF(dwork, struct buzzer_object, disable_buzzer_dwork);
	buzzer_disable(obj);

	SYNCHRONIZED(obj->run_finished_mtx)
	{
		obj->run_finished = true;
	}
}

static int buzzer_reschedule(struct buzzer_object *obj)
{
	bool was_scheduled = k_work_cancel_delayable_sync(&obj->disable_buzzer_dwork, &obj->sync);

	SYNCHRONIZED(obj->run_finished_mtx)
	{
		obj->run_finished = false;
	}

	if (obj->state.on_off) {
		if (buzzer_enable(obj)) {
			return -1;
		}

		if (obj->state.delay_duration == 0.0) {
			obj->running_infinitely = true;
		} else {
			int64_t delay_duration_msecs = MSEC_PER_SEC * obj->state.delay_duration;

			obj->last_run_end_timestamp = k_uptime_get() + delay_duration_msecs;
			k_work_schedule(&obj->disable_buzzer_dwork, K_MSEC(delay_duration_msecs));
		}
	} else {
		if (buzzer_disable(obj)) {
			return -1;
		}

		// This serves two purposes:
		// - if the buzzer has ended running before it was scheduled to,
		//   update the end_timestamp so it can be started earlier
		// - if the buzzer was running infinitely, update the
		//   end_timestamp so the buzzer won't be run again too early
		if (was_scheduled || obj->running_infinitely) {
			obj->last_run_end_timestamp = k_uptime_get();
			obj->running_infinitely = false;
		}
	}
	return 0;
}

static int instance_reset(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid)
{
	(void)anjay;

	struct buzzer_object *obj = get_obj(obj_ptr);

	assert(obj);
	assert(iid == 0);

	obj->state = default_state;
	return buzzer_reschedule(obj);
}

static int list_resources(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid, anjay_dm_resource_list_ctx_t *ctx)
{
	(void)anjay;
	(void)obj_ptr;
	(void)iid;

	anjay_dm_emit_res(ctx, RID_DELAY_DURATION, ANJAY_DM_RES_RW, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_MINIMUM_OFF_TIME, ANJAY_DM_RES_RW, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_APPLICATION_TYPE, ANJAY_DM_RES_RW, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_ON_OFF, ANJAY_DM_RES_RW, ANJAY_DM_RES_PRESENT);
	return 0;
}

static int resource_read(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			 anjay_iid_t iid, anjay_rid_t rid, anjay_riid_t riid,
			 anjay_output_ctx_t *ctx)
{
	(void)anjay;

	struct buzzer_object *obj = get_obj(obj_ptr);

	assert(obj);
	assert(iid == 0);

	switch (rid) {
	case RID_DELAY_DURATION:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_double(ctx, obj->state.delay_duration);

	case RID_MINIMUM_OFF_TIME:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_double(ctx, obj->state.minimum_off_time);

	case RID_APPLICATION_TYPE:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_string(ctx, obj->state.application_type);

	case RID_ON_OFF:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_bool(ctx, obj->state.on_off);

	default:
		return ANJAY_ERR_METHOD_NOT_ALLOWED;
	}
}

static int resource_write(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid, anjay_rid_t rid, anjay_riid_t riid,
			  anjay_input_ctx_t *ctx)
{
	(void)anjay;

	struct buzzer_object *obj = get_obj(obj_ptr);

	assert(obj);
	assert(iid == 0);

	switch (rid) {
	case RID_DELAY_DURATION: {
		assert(riid == ANJAY_ID_INVALID);
		return anjay_get_double(ctx, &obj->state.delay_duration);
	}

	case RID_MINIMUM_OFF_TIME: {
		assert(riid == ANJAY_ID_INVALID);
		return anjay_get_double(ctx, &obj->state.minimum_off_time);
	}

	case RID_APPLICATION_TYPE: {
		assert(riid == ANJAY_ID_INVALID);

		int err =
			anjay_get_string(ctx, obj->state.application_type, APPLICATION_TYPE_BUFLEN);
		if (err == ANJAY_BUFFER_TOO_SHORT) {
			return ANJAY_ERR_BAD_REQUEST;
		}
		if (err) {
			return ANJAY_ERR_INTERNAL;
		}
		return 0;
	}

	case RID_ON_OFF: {
		assert(riid == ANJAY_ID_INVALID);
		return anjay_get_bool(ctx, &obj->state.on_off);
	}

	default:
		return ANJAY_ERR_METHOD_NOT_ALLOWED;
	}
}

static int transaction_begin(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr)
{
	(void)anjay;

	struct buzzer_object *obj = get_obj(obj_ptr);

	memcpy(&obj->backup, &obj->state, sizeof(obj->state));

	return 0;
}

static inline int duration_validate(double duration)
{
	return (isfinite(duration) && duration >= 0.0) ? 0 : -1;
}

static int transaction_validate(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr)
{
	(void)anjay;

	struct buzzer_object *obj = get_obj(obj_ptr);

	if (duration_validate(obj->state.delay_duration)) {
		return ANJAY_ERR_BAD_REQUEST;
	}
	if (duration_validate(obj->state.minimum_off_time)) {
		return ANJAY_ERR_BAD_REQUEST;
	}
	if (obj->state.on_off && obj->last_run_end_timestamp != TIMESTAMP_NONE) {
		int64_t minimum_off_time_msecs = MSEC_PER_SEC * obj->state.minimum_off_time;
		int64_t next_run_timestamp = obj->last_run_end_timestamp + minimum_off_time_msecs;

		if (k_uptime_get() < next_run_timestamp) {
			return ANJAY_ERR_BAD_REQUEST;
		}
	}
	return 0;
}

static int transaction_commit(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr)
{
	(void)anjay;

	struct buzzer_object *obj = get_obj(obj_ptr);

	return buzzer_reschedule(obj) ? ANJAY_ERR_INTERNAL : 0;
}

static int transaction_rollback(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr)
{
	(void)anjay;

	struct buzzer_object *obj = get_obj(obj_ptr);

	memcpy(&obj->state, &obj->backup, sizeof(obj->state));

	return 0;
}

static const anjay_dm_object_def_t obj_def = {
	.oid = 3338,
	.handlers = { .list_instances = anjay_dm_list_instances_SINGLE,
		      .instance_reset = instance_reset,

		      .list_resources = list_resources,
		      .resource_read = resource_read,
		      .resource_write = resource_write,

		      .transaction_begin = transaction_begin,
		      .transaction_validate = transaction_validate,
		      .transaction_commit = transaction_commit,
		      .transaction_rollback = transaction_rollback }
};

const anjay_dm_object_def_t **buzzer_object_create(void)
{
	const struct device *dev = DEVICE_DT_GET(BUZZER_NODE);

	if (!device_is_ready(dev)) {
		return NULL;
	}

	struct buzzer_object *obj =
		(struct buzzer_object *)avs_calloc(1, sizeof(struct buzzer_object));
	if (!obj) {
		return NULL;
	}
	obj->def = &obj_def;
	obj->dev = dev;

	obj->run_finished = false;
	k_mutex_init(&obj->run_finished_mtx);

	k_work_init_delayable(&obj->disable_buzzer_dwork, buzzer_disable_work_handler);
	obj->last_run_end_timestamp = TIMESTAMP_NONE;
	obj->running_infinitely = false;

	obj->state = default_state;
	if (buzzer_reschedule(obj)) {
		const anjay_dm_object_def_t **out_def = &obj->def;

		buzzer_object_release(&out_def);
		return NULL;
	}

	return &obj->def;
}

void buzzer_object_update(anjay_t *anjay, const anjay_dm_object_def_t *const *def)
{
	if (!anjay || !def) {
		return;
	}

	struct buzzer_object *obj = get_obj(def);

	SYNCHRONIZED(obj->run_finished_mtx)
	{
		if (obj->run_finished) {
			obj->run_finished = false;
			obj->state.on_off = false;
			anjay_notify_changed(anjay, obj->def->oid, 0, RID_ON_OFF);
		}
	}
}

void buzzer_object_release(const anjay_dm_object_def_t ***out_def)
{
	const anjay_dm_object_def_t **def = *out_def;

	if (def) {
		struct buzzer_object *obj = get_obj(def);

		obj->state = default_state;
		buzzer_reschedule(obj);
		avs_free(obj);
		*out_def = NULL;
	}
}
#else // BUZZER_AVAILABLE
const anjay_dm_object_def_t **buzzer_object_create(void)
{
	return NULL;
}

void buzzer_object_update(anjay_t *anjay, const anjay_dm_object_def_t *const *def)
{
	(void)anjay;
	(void)def;
}

void buzzer_object_release(const anjay_dm_object_def_t ***out_def)
{
	(void)out_def;
}
#endif // BUZZER_AVAILABLE
