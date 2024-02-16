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
#include <zephyr/drivers/gpio.h>

#include "water_meter.h"
#include "bubblemaker.h"

LOG_MODULE_REGISTER(water_meter);

/**
 * Cumulated water volume: R, Single, Mandatory
 * type: float, range: N/A, unit: m3
 * Number of cubic meters of water distributed through the meter since
 * last reset.
 */
#define RID_CUMULATED_WATER_VOLUME 1

/**
 * Cumulated water meter value reset: E, Single, Optional
 * type: N/A, range: N/A, unit: N/A
 * Reset the cumulated meter value.
 */
#define RID_CUMULATED_WATER_METER_VALUE_RESET 2

/**
 * Current flow: R, Single, Mandatory
 * type: float, range: N/A, unit: m3
 * Current flow rate calculated in one second
 */
#define RID_CURRENT_FLOW 7

/**
 * Maximum flow rate: R, Single, Optional
 * type: float, range: N/A, unit: m3/s
 * Maximum flow rate since last metering value.
 */
#define RID_MAXIMUM_FLOW_RATE 8

#define SYNCHRONIZED(Mtx)                                                                          \
	for (int _synchronized_exit = k_mutex_lock(&(Mtx), K_FOREVER); !_synchronized_exit;        \
	     _synchronized_exit = -1, k_mutex_unlock(&(Mtx)))

#define WM_TIMER_CYCLE K_SECONDS(1)

static struct k_thread water_meter_thread;
static K_THREAD_STACK_DEFINE(water_meter_stack, 1024);
static K_MUTEX_DEFINE(water_meter_mutex);

struct water_meter_instance {
	anjay_iid_t iid;
	double cumulated_volume;
	double temp_volume;
	double curr_flow;
	double max_flow;
};

struct water_meter_object {
	const anjay_dm_object_def_t *def;

	AVS_LIST(struct water_meter_instance) instances;
};

#if WATER_METER_0_AVAILABLE
struct water_meter_instance *wm_inst_0_ptr;
atomic_t water_meter_0_irq_count = ATOMIC_INIT(0);
#endif // WATER_METER_0_AVAILABLE
#if WATER_METER_1_AVAILABLE
struct water_meter_instance *wm_inst_1_ptr;
atomic_t water_meter_1_irq_count = ATOMIC_INIT(0);
#endif // WATER_METER_1_AVAILABLE

void water_meter_instances_reset(void)
{
	SYNCHRONIZED(water_meter_mutex)
	{
#if WATER_METER_0_AVAILABLE
		wm_inst_0_ptr->cumulated_volume = 0;
		wm_inst_0_ptr->max_flow = 0;
		wm_inst_0_ptr->curr_flow = 0;
#endif // WATER_METER_0_AVAILABLE

#if WATER_METER_1_AVAILABLE
		wm_inst_1_ptr->cumulated_volume = 0;
		wm_inst_1_ptr->max_flow = 0;
		wm_inst_1_ptr->curr_flow = 0;
#endif // WATER_METER_1_AVAILABLE
	}
}

bool water_meter_is_null(void)
{
#if WATER_METER_0_AVAILABLE
	if (!wm_inst_0_ptr) {
		return true;
	}
#endif // WATER_METER_0_AVAILABLE
#if WATER_METER_1_AVAILABLE
	if (!wm_inst_1_ptr) {
		return true;
	}
#endif // WATER_METER_1_AVAILABLE

	return false;
}

#if WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE
void water_meter_get_cumulated_volumes(double *out_result)
{
	SYNCHRONIZED(water_meter_mutex)
	{
		out_result[0] = wm_inst_0_ptr->cumulated_volume;
		out_result[1] = wm_inst_1_ptr->cumulated_volume;
	}
}
#endif // WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE

static inline struct water_meter_object *get_obj(const anjay_dm_object_def_t *const *obj_ptr)
{
	assert(obj_ptr);
	return AVS_CONTAINER_OF(obj_ptr, struct water_meter_object, def);
}

static struct water_meter_instance *find_instance(const struct water_meter_object *obj,
						  anjay_iid_t iid)
{
	AVS_LIST(struct water_meter_instance) it;
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

	AVS_LIST(struct water_meter_instance) it;
	AVS_LIST_FOREACH(it, get_obj(obj_ptr)->instances)
	{
		anjay_dm_emit(ctx, it->iid);
	}

	return 0;
}

static int init_instance(struct water_meter_instance *inst, anjay_iid_t iid)
{
	assert(iid != ANJAY_ID_INVALID);

	SYNCHRONIZED(water_meter_mutex)
	{
		inst->iid = iid;
		inst->cumulated_volume = 0;
		inst->temp_volume = 0;
		inst->curr_flow = 0;
		inst->max_flow = 0;
	}
	return 0;
}

static void release_instance(struct water_meter_instance *inst)
{
	(void)inst;
}

static struct water_meter_instance *add_instance(struct water_meter_object *obj, anjay_iid_t iid)
{
	assert(find_instance(obj, iid) == NULL);

	AVS_LIST(struct water_meter_instance)
	created = AVS_LIST_NEW_ELEMENT(struct water_meter_instance);
	if (!created) {
		return NULL;
	}

	int result;

	result = init_instance(created, iid);
	if (result) {
		AVS_LIST_CLEAR(&created);
		return NULL;
	}

	AVS_LIST(struct water_meter_instance) * ptr;
	AVS_LIST_FOREACH_PTR(ptr, &obj->instances)
	{
		if ((*ptr)->iid > created->iid) {
			break;
		}
	}

	AVS_LIST_INSERT(ptr, created);
	return created;
}

static int instance_reset(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid)
{
	(void)anjay;

	struct water_meter_object *obj = get_obj(obj_ptr);

	assert(obj);
	struct water_meter_instance *inst = find_instance(obj, iid);

	assert(inst);
	SYNCHRONIZED(water_meter_mutex)
	{
		inst->cumulated_volume = 0;
		inst->max_flow = 0;
		inst->temp_volume = 0;
		inst->curr_flow = 0;
	}
	return 0;
}

static int list_resources(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid, anjay_dm_resource_list_ctx_t *ctx)
{
	(void)anjay;
	(void)obj_ptr;
	(void)iid;

	anjay_dm_emit_res(ctx, RID_CUMULATED_WATER_VOLUME, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_CUMULATED_WATER_METER_VALUE_RESET, ANJAY_DM_RES_E,
			  ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_CURRENT_FLOW, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_MAXIMUM_FLOW_RATE, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	return 0;
}

static int resource_read(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			 anjay_iid_t iid, anjay_rid_t rid, anjay_riid_t riid,
			 anjay_output_ctx_t *ctx)
{
	(void)anjay;

	struct water_meter_object *obj = get_obj(obj_ptr);

	assert(obj);
	struct water_meter_instance *inst = find_instance(obj, iid);

	assert(inst);

	switch (rid) {
	case RID_CUMULATED_WATER_VOLUME:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_double(ctx, inst->cumulated_volume);

	case RID_CURRENT_FLOW:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_double(ctx, inst->curr_flow);

	case RID_MAXIMUM_FLOW_RATE:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_double(ctx, inst->max_flow);

	default:
		return ANJAY_ERR_METHOD_NOT_ALLOWED;
	}
}

static int resource_execute(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			    anjay_iid_t iid, anjay_rid_t rid, anjay_execute_ctx_t *arg_ctx)
{
	(void)anjay;
	(void)arg_ctx;

	struct water_meter_object *obj = get_obj(obj_ptr);

	assert(obj);
	struct water_meter_instance *inst = find_instance(obj, iid);

	assert(inst);

	switch (rid) {
	case RID_CUMULATED_WATER_METER_VALUE_RESET:
		if (bm_state == BUBBLEMAKER_IDLE) {
			water_meter_instances_reset();
			bm_state = BUBBLEMAKER_START_RED_LIGHT;
		}
		return 0;
	default:
		return ANJAY_ERR_METHOD_NOT_ALLOWED;
	}
}

static const anjay_dm_object_def_t OBJ_DEF = { .oid = 3424,
					       .handlers = { .list_instances = list_instances,
							     .instance_reset = instance_reset,

							     .list_resources = list_resources,
							     .resource_read = resource_read,
							     .resource_execute =
								     resource_execute } };

const anjay_dm_object_def_t **water_meter_object_create(void)
{
	struct water_meter_object *obj =
		(struct water_meter_object *)avs_calloc(1, sizeof(struct water_meter_object));
	if (!obj) {
		return NULL;
	}
	obj->def = &OBJ_DEF;

	static int instance_counter;

#if WATER_METER_0_AVAILABLE
	wm_inst_0_ptr = add_instance(obj, instance_counter++);

	if (!wm_inst_0_ptr) {
		avs_free(obj);
		return NULL;
	}
#endif // WATER_METER_0_AVAILABLE

#if WATER_METER_1_AVAILABLE
	wm_inst_1_ptr = add_instance(obj, instance_counter++);

	if (!wm_inst_1_ptr) {
		avs_free(obj);
		return NULL;
	}
#endif // WATER_METER_1_AVAILABLE

	return &obj->def;
}

void water_meter_object_release(const anjay_dm_object_def_t **def)
{
	if (def) {
		struct water_meter_object *obj = get_obj(def);

		AVS_LIST_CLEAR(&obj->instances)
		{
			release_instance(obj->instances);
		}
		avs_free(obj);
	}
}

#if WATER_METER_0_AVAILABLE
static void water_meter_0_callback_handler(const struct device *port, struct gpio_callback *cb,
					   gpio_port_pins_t pins)
{
	atomic_inc(&water_meter_0_irq_count);
}
#endif // WATER_METER_0_AVAILABLE

#if WATER_METER_1_AVAILABLE
static void water_meter_1_callback_handler(const struct device *port, struct gpio_callback *cb,
					   gpio_port_pins_t pins)
{
	atomic_inc(&water_meter_1_irq_count);
}
#endif // WATER_METER_1_AVAILABLE

static void water_meter_update_values(struct water_meter_instance *wm_instance,
				      atomic_val_t *multiplier)
{
	// based on: https://forum.seeedstudio.com/t/tutorial-reading-water-flow-rate-with-water-flow-sensor/647
	SYNCHRONIZED(water_meter_mutex)
	{
		wm_instance->curr_flow = (float)*multiplier * 60.0f / 7.5f; // L/h
		wm_instance->curr_flow = wm_instance->curr_flow / 3600.0f / 1000.0f; // m^3/s
		wm_instance->temp_volume = wm_instance->curr_flow; // readings every second
		wm_instance->cumulated_volume += wm_instance->temp_volume;
		if (wm_instance->max_flow < wm_instance->curr_flow) {
			wm_instance->max_flow = wm_instance->curr_flow;
		}
	}
}

static void water_meter_periodic(void *arg1, void *arg2, void *arg3)
{
	while (water_meter_is_null()) {
		k_sleep(K_SECONDS(1));
	}

	while (1) {
#if WATER_METER_0_AVAILABLE
		atomic_val_t water_meter_0_multiplier = atomic_get(&water_meter_0_irq_count);

		atomic_clear(&water_meter_0_irq_count);
		water_meter_update_values(wm_inst_0_ptr, &water_meter_0_multiplier);
#endif // WATER_METER_0_AVAILABLE

#if WATER_METER_1_AVAILABLE
		atomic_val_t water_meter_1_multiplier = atomic_get(&water_meter_1_irq_count);

		atomic_clear(&water_meter_1_irq_count);
		water_meter_update_values(wm_inst_1_ptr, &water_meter_1_multiplier);
#endif // WATER_METER_1_AVAILABLE
		k_sleep(WM_TIMER_CYCLE);
	}
}

int water_meter_init(void)
{
#if WATER_METER_0_AVAILABLE
	static const struct gpio_dt_spec water_meter_0_spec =
		GPIO_DT_SPEC_GET(WATER_METER_0_NODE, gpios);

	if (!device_is_ready(water_meter_0_spec.port)) {
		LOG_ERR("Water meter 0 is not ready");
		return -1;
	}

	static struct gpio_callback water_meter_0_callback;

	gpio_pin_configure_dt(&water_meter_0_spec, GPIO_INPUT);
	gpio_init_callback(&water_meter_0_callback, water_meter_0_callback_handler,
			   BIT(water_meter_0_spec.pin));
	gpio_add_callback(water_meter_0_spec.port, &water_meter_0_callback);
	gpio_pin_interrupt_configure_dt(&water_meter_0_spec, GPIO_INT_EDGE_RISING);
#endif // WATER_METER_0_AVAILABLE

#if WATER_METER_1_AVAILABLE
	static const struct gpio_dt_spec water_meter_1_spec =
		GPIO_DT_SPEC_GET(WATER_METER_1_NODE, gpios);

	if (!device_is_ready(water_meter_1_spec.port)) {
		LOG_ERR("Water meter 1 is not ready");
		return -1;
	}

	static struct gpio_callback water_meter_1_callback;

	gpio_pin_configure_dt(&water_meter_1_spec, GPIO_INPUT);
	gpio_init_callback(&water_meter_1_callback, water_meter_1_callback_handler,
			   BIT(water_meter_1_spec.pin));
	gpio_add_callback(water_meter_1_spec.port, &water_meter_1_callback);
	gpio_pin_interrupt_configure_dt(&water_meter_1_spec, GPIO_INT_EDGE_RISING);
#endif // WATER_METER_1_AVAILABLE

	if (!k_thread_create(&water_meter_thread, water_meter_stack,
			     K_THREAD_STACK_SIZEOF(water_meter_stack), water_meter_periodic, NULL,
			     NULL, NULL, 1, 0, K_NO_WAIT)) {
		LOG_ERR("Failed to create water_meter thread");
		return -1;
	}

	return 0;
}
