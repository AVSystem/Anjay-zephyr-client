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
#include <string.h>

#include <anjay/anjay.h>
#include <avsystem/commons/avs_defs.h>
#include <avsystem/commons/avs_list.h>
#include <avsystem/commons/avs_memory.h>
#include <avsystem/commons/avs_utils.h>

#include <devicetree.h>
#include <drivers/pwm.h>
#include <zephyr.h>

#include "objects.h"

/**
 * RGB value: RW, Single, Mandatory
 * type: string, range: N/A, unit: N/A
 * Text string according to the RGB hexadecimal format with # (e.g.
 * #FF0000 for 100% red).
 */
#define RID_RGB_VALUE 1

#if LED_COLOR_LIGHT_AVAILABLE
#define RGB_VALUE_STR_BUFLEN sizeof("#RRGGBB")
#define RGB_COLOR_COUNT 3

struct led_color_light_object {
	const anjay_dm_object_def_t *def;

	const struct device *dev;
	char rgb_value_str[RGB_VALUE_STR_BUFLEN];
	uint8_t rgb_value[RGB_COLOR_COUNT];

	char rgb_value_str_backup[RGB_VALUE_STR_BUFLEN];
};

static inline struct led_color_light_object *get_obj(const anjay_dm_object_def_t *const *obj_ptr)
{
	assert(obj_ptr);
	return AVS_CONTAINER_OF(obj_ptr, struct led_color_light_object, def);
}

static int rgb_value_str_parse(struct led_color_light_object *obj)
{
	if (strlen(obj->rgb_value_str) != RGB_VALUE_STR_BUFLEN - 1) {
		return -1;
	}
	if (obj->rgb_value_str[0] != '#') {
		return -1;
	}

	// skip '#' char
	const char *rgb_value_hexstring = obj->rgb_value_str + 1;
	static const size_t rgb_value_hexstring_strlen = RGB_VALUE_STR_BUFLEN - 2;

	size_t out_bytes_written;

	if (avs_unhexlify(&out_bytes_written, obj->rgb_value, sizeof(obj->rgb_value),
			  rgb_value_hexstring, rgb_value_hexstring_strlen) ||
	    out_bytes_written != sizeof(obj->rgb_value)) {
		return -1;
	}

	return 0;
}

static int rgb_led_set(struct led_color_light_object *obj)
{
#define SET_COLOR(pin) pwm_set_cycles(obj->dev, pin, 255, obj->rgb_value[pin], 0)

	if (SET_COLOR(0) || SET_COLOR(1) || SET_COLOR(2)) {
		return -1;
	}

#undef SET_COLOR

	return 0;
}

static void rgb_value_reset(struct led_color_light_object *obj)
{
	static const char *const rgb_color_black = "#000000";

	memcpy(obj->rgb_value_str, rgb_color_black, RGB_VALUE_STR_BUFLEN);
	rgb_value_str_parse(obj);
}

static int instance_reset(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid)
{
	(void)anjay;

	struct led_color_light_object *obj = get_obj(obj_ptr);

	assert(obj);
	assert(iid == 0);

	rgb_value_reset(obj);
	return rgb_led_set(obj) ? ANJAY_ERR_INTERNAL : 0;
}

static int list_resources(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid, anjay_dm_resource_list_ctx_t *ctx)
{
	(void)anjay;
	(void)obj_ptr;
	(void)iid;

	anjay_dm_emit_res(ctx, RID_RGB_VALUE, ANJAY_DM_RES_RW, ANJAY_DM_RES_PRESENT);
	return 0;
}

static int resource_read(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			 anjay_iid_t iid, anjay_rid_t rid, anjay_riid_t riid,
			 anjay_output_ctx_t *ctx)
{
	(void)anjay;

	struct led_color_light_object *obj = get_obj(obj_ptr);

	assert(obj);
	assert(iid == 0);

	switch (rid) {
	case RID_RGB_VALUE:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_string(ctx, obj->rgb_value_str);

	default:
		return ANJAY_ERR_METHOD_NOT_ALLOWED;
	}
}

static int resource_write(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid, anjay_rid_t rid, anjay_riid_t riid,
			  anjay_input_ctx_t *ctx)
{
	(void)anjay;

	struct led_color_light_object *obj = get_obj(obj_ptr);

	assert(obj);
	assert(iid == 0);

	switch (rid) {
	case RID_RGB_VALUE: {
		assert(riid == ANJAY_ID_INVALID);

		int err = anjay_get_string(ctx, obj->rgb_value_str, RGB_VALUE_STR_BUFLEN);

		if (err == ANJAY_BUFFER_TOO_SHORT) {
			return ANJAY_ERR_BAD_REQUEST;
		}
		if (err) {
			return ANJAY_ERR_INTERNAL;
		}
		return 0;
	}

	default:
		return ANJAY_ERR_METHOD_NOT_ALLOWED;
	}
}

static int transaction_begin(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr)
{
	(void)anjay;

	struct led_color_light_object *obj = get_obj(obj_ptr);

	memcpy(obj->rgb_value_str_backup, obj->rgb_value_str, RGB_VALUE_STR_BUFLEN);

	return 0;
}

static int transaction_validate(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr)
{
	(void)anjay;

	struct led_color_light_object *obj = get_obj(obj_ptr);

	return rgb_value_str_parse(obj) ? ANJAY_ERR_BAD_REQUEST : 0;
}

static int transaction_commit(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr)
{
	(void)anjay;

	struct led_color_light_object *obj = get_obj(obj_ptr);

	return rgb_led_set(obj) ? ANJAY_ERR_INTERNAL : 0;
}

static int transaction_rollback(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr)
{
	(void)anjay;

	struct led_color_light_object *obj = get_obj(obj_ptr);

	memcpy(obj->rgb_value_str, obj->rgb_value_str_backup, RGB_VALUE_STR_BUFLEN);

	return rgb_value_str_parse(obj) ? ANJAY_ERR_INTERNAL : 0;
}

static const anjay_dm_object_def_t obj_def = {
	.oid = 3420,
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

const anjay_dm_object_def_t **led_color_light_object_create(void)
{
	const struct device *dev = DEVICE_DT_GET(RGB_NODE);

	if (!device_is_ready(dev)) {
		return NULL;
	}

	struct led_color_light_object *obj = (struct led_color_light_object *)avs_calloc(
		1, sizeof(struct led_color_light_object));
	if (!obj) {
		return NULL;
	}
	obj->def = &obj_def;
	obj->dev = dev;

	rgb_value_reset(obj);
	if (rgb_led_set(obj)) {
		const anjay_dm_object_def_t **out_def = &obj->def;

		led_color_light_object_release(&out_def);
		return NULL;
	}

	return &obj->def;
}

void led_color_light_object_release(const anjay_dm_object_def_t ***out_def)
{
	const anjay_dm_object_def_t **def = *out_def;

	if (def) {
		struct led_color_light_object *obj = get_obj(def);

		rgb_value_reset(obj);
		rgb_led_set(obj);
		avs_free(obj);
		*out_def = NULL;
	}
}
#else // LED_COLOR_LIGHT_AVAILABLE
const anjay_dm_object_def_t **led_color_light_object_create(void)
{
	return NULL;
}

void led_color_light_object_release(const anjay_dm_object_def_t ***out_def)
{
	(void)out_def;
}
#endif // LED_COLOR_LIGHT_AVAILABLE
