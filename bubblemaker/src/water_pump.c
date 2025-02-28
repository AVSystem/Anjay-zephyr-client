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

#include "water_pump.h"

#if WATER_PUMP_0_AVAILABLE
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(water_pump);

/**
 * Application Type: RW, Single, Optional
 * type: string, range: N/A, unit: N/A
 * The application type of the sensor or actuator as a string depending
 * on the use case.
 */
#define RID_APPLICATION_TYPE 5750

/**
 * Cumulative active power: R, Single, Optional
 * type: float, range: N/A, unit: Wh
 * The cumulative active power since the last cumulative energy reset or
 * device start.
 */
#define RID_CUMULATIVE_ACTIVE_POWER 5805

/**
 * Power factor: R, Single, Optional
 * type: float, range: N/A, unit: N/A
 * If applicable, the power factor of the current consumption.
 */
#define RID_POWER_FACTOR 5820

/**
 * On/Off: RW, Single, Mandatory
 * type: boolean, range: N/A, unit: N/A
 * On/off control. Boolean value where True is On and False is Off.
 */
#define RID_ON_OFF 5850

/**
 * Dimmer: RW, Single, Optional
 * type: integer, range: 0..100, unit: /100
 * This resource represents a dimmer setting, which has an Integer value
 * between 0 and 100 as a percentage.
 */
#define RID_DIMMER 5851

/**
 * On time: RW, Single, Optional
 * type: integer, range: N/A, unit: s
 * The time in seconds that the device has been on. Writing a value of 0
 * resets the counter.
 */
#define RID_ON_TIME 5852

#define SYNCHRONIZED(Mtx)                                                                          \
	for (int _synchronized_exit = k_mutex_lock(&(Mtx), K_FOREVER); !_synchronized_exit;        \
	     _synchronized_exit = -1, k_mutex_unlock(&(Mtx)))

static K_MUTEX_DEFINE(water_pump_mutex);

static struct k_work gpio_toggle_work;

static const struct gpio_dt_spec water_pump_0_spec = GPIO_DT_SPEC_GET(WATER_PUMP_0_NODE, gpios);
static const struct gpio_dt_spec button_0_spec = GPIO_DT_SPEC_GET(BUTTON_0_NODE, gpios);

static struct gpio_callback button_0_callback;

struct power_control_instance {
	anjay_iid_t iid;
	char application_type[64];
	bool state;
};

struct power_control_object {
	const anjay_dm_object_def_t *def;

	AVS_LIST(struct power_control_instance) instances;
};

static inline struct power_control_object *get_obj(const anjay_dm_object_def_t *const *obj_ptr)
{
	assert(obj_ptr);
	return AVS_CONTAINER_OF(obj_ptr, struct power_control_object, def);
}

static struct power_control_instance *find_instance(const struct power_control_object *obj,
						    anjay_iid_t iid)
{
	AVS_LIST(struct power_control_instance) it;
	AVS_LIST_FOREACH(it, obj->instances)
	{
		if (it->iid == iid) {
			return it;
		} else if (it->iid > iid) {
			break;
		}
	}

	return NULL;
}

static int list_instances(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_dm_list_ctx_t *ctx)
{
	(void)anjay;

	AVS_LIST(struct power_control_instance) it;
	AVS_LIST_FOREACH(it, get_obj(obj_ptr)->instances)
	{
		anjay_dm_emit(ctx, it->iid);
	}

	return 0;
}

static int init_instance(struct power_control_instance *inst, anjay_iid_t iid)
{
	assert(iid != ANJAY_ID_INVALID);

	inst->iid = iid;
	strncpy(inst->application_type, "Water pump", sizeof(inst->application_type));
	SYNCHRONIZED(water_pump_mutex)
	{
		inst->state = gpio_pin_get_dt(&water_pump_0_spec);
	}

	return 0;
}

static void release_instance(struct power_control_instance *inst)
{
	(void)inst;
}

static struct power_control_instance *add_instance(struct power_control_object *obj,
						   anjay_iid_t iid)
{
	assert(find_instance(obj, iid) == NULL);

	AVS_LIST(struct power_control_instance)
	created = AVS_LIST_NEW_ELEMENT(struct power_control_instance);
	if (!created) {
		return NULL;
	}

	int result = init_instance(created, iid);

	if (result) {
		AVS_LIST_CLEAR(&created);
		return NULL;
	}

	AVS_LIST(struct power_control_instance) * ptr;
	AVS_LIST_FOREACH_PTR(ptr, &obj->instances)
	{
		if ((*ptr)->iid > created->iid) {
			break;
		}
	}

	AVS_LIST_INSERT(ptr, created);
	return created;
}

static int instance_create(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			   anjay_iid_t iid)
{
	(void)anjay;
	struct power_control_object *obj = get_obj(obj_ptr);

	return add_instance(obj, iid) ? 0 : ANJAY_ERR_INTERNAL;
}

static int instance_remove(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			   anjay_iid_t iid)
{
	(void)anjay;
	struct power_control_object *obj = get_obj(obj_ptr);

	AVS_LIST(struct power_control_instance) * it;
	AVS_LIST_FOREACH_PTR(it, &obj->instances)
	{
		if ((*it)->iid == iid) {
			release_instance(*it);
			AVS_LIST_DELETE(it);
			return 0;
		} else if ((*it)->iid > iid) {
			break;
		}
	}

	assert(0);
	return ANJAY_ERR_NOT_FOUND;
}

static int instance_reset(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid)
{
	(void)anjay;

	struct power_control_object *obj = get_obj(obj_ptr);
	struct power_control_instance *inst = find_instance(obj, iid);

	assert(inst);

	return 0;
}

static int list_resources(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid, anjay_dm_resource_list_ctx_t *ctx)
{
	(void)anjay;
	(void)obj_ptr;
	(void)iid;

	anjay_dm_emit_res(ctx, RID_APPLICATION_TYPE, ANJAY_DM_RES_RW, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_CUMULATIVE_ACTIVE_POWER, ANJAY_DM_RES_R, ANJAY_DM_RES_ABSENT);
	anjay_dm_emit_res(ctx, RID_POWER_FACTOR, ANJAY_DM_RES_R, ANJAY_DM_RES_ABSENT);
	anjay_dm_emit_res(ctx, RID_ON_OFF, ANJAY_DM_RES_RW, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_DIMMER, ANJAY_DM_RES_RW, ANJAY_DM_RES_ABSENT);
	anjay_dm_emit_res(ctx, RID_ON_TIME, ANJAY_DM_RES_RW, ANJAY_DM_RES_ABSENT);
	return 0;
}

static int resource_read(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			 anjay_iid_t iid, anjay_rid_t rid, anjay_riid_t riid,
			 anjay_output_ctx_t *ctx)
{
	(void)anjay;

	struct power_control_object *obj = get_obj(obj_ptr);
	struct power_control_instance *inst = find_instance(obj, iid);

	assert(inst);

	switch (rid) {
	case RID_APPLICATION_TYPE:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_string(ctx, inst->application_type);

	case RID_ON_OFF:
		assert(riid == ANJAY_ID_INVALID);
		SYNCHRONIZED(water_pump_mutex)
		{
			inst->state = gpio_pin_get_dt(&water_pump_0_spec);
		}
		return anjay_ret_bool(ctx, inst->state);

	default:
		return ANJAY_ERR_METHOD_NOT_ALLOWED;
	}
}

static int resource_write(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid, anjay_rid_t rid, anjay_riid_t riid,
			  anjay_input_ctx_t *ctx)
{
	(void)anjay;

	struct power_control_object *obj = get_obj(obj_ptr);
	struct power_control_instance *inst = find_instance(obj, iid);

	assert(inst);

	switch (rid) {
	case RID_APPLICATION_TYPE: {
		assert(riid == ANJAY_ID_INVALID);
		return anjay_get_string(ctx, inst->application_type,
					sizeof(inst->application_type));
	}

	case RID_ON_OFF: {
		assert(riid == ANJAY_ID_INVALID);
		bool value;
		int ret = anjay_get_bool(ctx, &value);

		SYNCHRONIZED(water_pump_mutex)
		{
			inst->state = value;
			gpio_pin_set_dt(&water_pump_0_spec, value);
		}
		return ret;
	}

	default:
		return ANJAY_ERR_METHOD_NOT_ALLOWED;
	}
}

static const anjay_dm_object_def_t OBJ_DEF = {
	.oid = 3312,
	.handlers = { .list_instances = list_instances,
		      .instance_create = instance_create,
		      .instance_remove = instance_remove,
		      .instance_reset = instance_reset,

		      .list_resources = list_resources,
		      .resource_read = resource_read,
		      .resource_write = resource_write,

		      .transaction_begin = anjay_dm_transaction_NOOP,
		      .transaction_validate = anjay_dm_transaction_NOOP,
		      .transaction_commit = anjay_dm_transaction_NOOP,
		      .transaction_rollback = anjay_dm_transaction_NOOP }
};

const anjay_dm_object_def_t **power_control_object_create(void)
{
	struct power_control_object *obj =
		(struct power_control_object *)avs_calloc(1, sizeof(struct power_control_object));
	if (!obj) {
		return NULL;
	}
	obj->def = &OBJ_DEF;

	if (!add_instance(obj, 0)) {
		avs_free(obj);
		return NULL;
	}

	return &obj->def;
}

void power_control_object_release(const anjay_dm_object_def_t **def)
{
	if (def) {
		struct power_control_object *obj = get_obj(def);

		AVS_LIST_CLEAR(&obj->instances)
		{
			release_instance(obj->instances);
		}
		avs_free(obj);
	}
}

static void button_0_callback_handler(const struct device *port, struct gpio_callback *cb,
				      gpio_port_pins_t pins)
{
	k_work_submit(&gpio_toggle_work);
}

void gpio_toggle_work_handler(struct k_work *work)
{
	SYNCHRONIZED(water_pump_mutex)
	{
		gpio_pin_toggle_dt(&water_pump_0_spec);
	}
}

int water_pump_initialize(void)
{
	if (!device_is_ready(water_pump_0_spec.port)) {
		LOG_ERR("Water Pump 0 is not ready");
		return -1;
	}

	gpio_pin_configure_dt(&water_pump_0_spec, GPIO_OUTPUT_ACTIVE | GPIO_INPUT);

	if (!device_is_ready(button_0_spec.port)) {
		LOG_ERR("Button 0 is not ready");
		return -1;
	}

	gpio_pin_configure_dt(&button_0_spec, GPIO_INPUT);
	gpio_init_callback(&button_0_callback, button_0_callback_handler, BIT(button_0_spec.pin));
	gpio_add_callback(button_0_spec.port, &button_0_callback);
	gpio_pin_interrupt_configure_dt(&button_0_spec, GPIO_INT_EDGE_RISING);

	k_work_init(&gpio_toggle_work, gpio_toggle_work_handler);

	return 0;
}
#endif // WATER_PUMP_0_AVAILABLE
