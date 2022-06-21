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
#include <stdbool.h>

#include <anjay/anjay.h>
#include <avsystem/commons/avs_defs.h>
#include <avsystem/commons/avs_list.h>
#include <avsystem/commons/avs_memory.h>

#include <devicetree.h>
#include <drivers/gpio.h>

#include "objects.h"

/**
 * Digital Input State: R, Single, Mandatory
 * type: boolean, range: N/A, unit: N/A
 * The current state of a digital input.
 */
#define RID_DIGITAL_INPUT_STATE 5500

#if SWITCH_AVAILABLE_ANY
struct switch_instance {
	anjay_iid_t iid;

	const struct device *dev;
	int gpio_pin;

	bool digital_input_state;
};

struct switch_object {
	const anjay_dm_object_def_t *def;

	AVS_LIST(struct switch_instance) instances;
};

static inline struct switch_object *get_obj(const anjay_dm_object_def_t *const *obj_ptr)
{
	assert(obj_ptr);
	return AVS_CONTAINER_OF(obj_ptr, struct switch_object, def);
}

static struct switch_instance *find_instance(const struct switch_object *obj, anjay_iid_t iid)
{
	AVS_LIST(struct switch_instance) it;
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

	AVS_LIST(struct switch_instance) it;
	AVS_LIST_FOREACH(it, get_obj(obj_ptr)->instances)
	{
		anjay_dm_emit(ctx, it->iid);
	}

	return 0;
}

static int init_instance(struct switch_instance *inst, anjay_iid_t iid)
{
	assert(iid != ANJAY_ID_INVALID);

	inst->iid = iid;
	return 0;
}

static int list_resources(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid, anjay_dm_resource_list_ctx_t *ctx)
{
	(void)anjay;
	(void)obj_ptr;
	(void)iid;

	anjay_dm_emit_res(ctx, RID_DIGITAL_INPUT_STATE, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	return 0;
}

static int resource_read(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			 anjay_iid_t iid, anjay_rid_t rid, anjay_riid_t riid,
			 anjay_output_ctx_t *ctx)
{
	(void)anjay;

	struct switch_object *obj = get_obj(obj_ptr);

	assert(obj);

	struct switch_instance *inst = find_instance(obj, iid);

	assert(inst);

	switch (rid) {
	case RID_DIGITAL_INPUT_STATE:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_bool(ctx, inst->digital_input_state);

	default:
		return ANJAY_ERR_METHOD_NOT_ALLOWED;
	}
}

static struct switch_instance *add_instance(struct switch_object *obj, anjay_iid_t iid)
{
	assert(find_instance(obj, iid) == NULL);

	AVS_LIST(struct switch_instance)
	created = AVS_LIST_NEW_ELEMENT(struct switch_instance);
	if (!created) {
		return NULL;
	}

	int result = init_instance(created, iid);

	if (result) {
		AVS_LIST_CLEAR(&created);
		return NULL;
	}

	AVS_LIST(struct switch_instance) * ptr;
	AVS_LIST_FOREACH_PTR(ptr, &obj->instances)
	{
		if ((*ptr)->iid > created->iid) {
			break;
		}
	}

	AVS_LIST_INSERT(ptr, created);
	return created;
}

static int configure_switch(struct switch_object *obj, const struct device *dev, int gpio_pin,
			    int gpio_flags, anjay_iid_t iid)
{
	if (!device_is_ready(dev) || gpio_pin_configure(dev, gpio_pin, gpio_flags)) {
		return -1;
	}

	struct switch_instance *inst = add_instance(obj, iid);

	if (inst) {
		inst->dev = dev;
		inst->digital_input_state = gpio_pin_get(inst->dev, gpio_pin);
		inst->gpio_pin = gpio_pin;
	} else {
		return -1;
	}

	return 0;
}

static const anjay_dm_object_def_t obj_def = { .oid = 3342,
					       .handlers = { .list_instances = list_instances,
							     .list_resources = list_resources,
							     .resource_read = resource_read } };

static const anjay_dm_object_def_t *obj_def_ptr = &obj_def;

void switch_object_release(const anjay_dm_object_def_t ***out_def)
{
	const anjay_dm_object_def_t **def = *out_def;

	if (def) {
		struct switch_object *obj = get_obj(def);

		AVS_LIST_CLEAR(&obj->instances);
		avs_free(obj);
		*out_def = NULL;
	}
}

const anjay_dm_object_def_t **switch_object_create(void)
{
	struct switch_object *obj =
		(struct switch_object *)avs_calloc(1, sizeof(struct switch_object));
	if (!obj) {
		return NULL;
	}
	obj->def = obj_def_ptr;

#define CONFIGURE(idx)                                                                             \
	configure_switch(obj, DEVICE_DT_GET(DT_GPIO_CTLR(SWITCH_NODE(idx), gpios)),                \
			 DT_GPIO_PIN(SWITCH_NODE(idx), gpios),                                     \
			 (GPIO_INPUT | DT_GPIO_FLAGS(SWITCH_NODE(idx), gpios)), idx)

#if SWITCH_AVAILABLE(0)
	CONFIGURE(0);
#endif // SWITCH_AVAILABLE(0)
#if SWITCH_AVAILABLE(1)
	CONFIGURE(1);
#endif // SWITCH_AVAILABLE(1)
#if SWITCH_AVAILABLE(2)
	CONFIGURE(2);
#endif // SWITCH_AVAILABLE(2)

#undef CONFIGURE

	if (!obj->instances) {
		const anjay_dm_object_def_t **out_def = &obj_def_ptr;

		switch_object_release(&out_def);
		return NULL;
	}

	return &obj->def;
}

void switch_object_update(anjay_t *anjay, const anjay_dm_object_def_t *const *def)
{
	if (!anjay || !def) {
		return;
	}

	struct switch_object *obj = get_obj(def);

	AVS_LIST(struct switch_instance) inst;
	AVS_LIST_FOREACH(inst, obj->instances)
	{
		int switch_state = gpio_pin_get(inst->dev, inst->gpio_pin);

		if (switch_state >= 0 && switch_state != inst->digital_input_state) {
			inst->digital_input_state = switch_state;
			(void)anjay_notify_changed(anjay, obj->def->oid, inst->iid,
						   RID_DIGITAL_INPUT_STATE);
		}
	}
}
#else // SWITCH_AVAILABLE_ANY
const anjay_dm_object_def_t **switch_object_create(void)
{
	return NULL;
}

void switch_object_release(const anjay_dm_object_def_t ***out_def)
{
	(void)out_def;
}

void switch_object_update(anjay_t *anjay, const anjay_dm_object_def_t *const *def)
{
	(void)anjay;
	(void)def;
}
#endif // SWITCH_AVAILABLE_ANY
