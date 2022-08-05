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
#include <time.h>

#include <anjay/anjay.h>
#include <avsystem/commons/avs_defs.h>
#include <avsystem/commons/avs_memory.h>

#include <sys/reboot.h>
#include <zephyr.h>

#include "../default_config.h"
#include "../utils.h"

#define SUPPORTED_BINDING_MODES "UQ"

/**
 * Manufacturer: R, Single, Optional
 * type: string, range: N/A, unit: N/A
 * Human readable manufacturer name
 */
#define RID_MANUFACTURER 0

/**
 * Model Number: R, Single, Optional
 * type: string, range: N/A, unit: N/A
 * A model identifier (manufacturer specified string)
 */
#define RID_MODEL_NUMBER 1

/**
 * Serial Number: R, Single, Optional
 * type: string, range: N/A, unit: N/A
 * Serial Number
 */
#define RID_SERIAL_NUMBER 2

/**
 * Firmware Version: R, Single, Optional
 * type: string, range: N/A, unit: N/A
 * Current firmware version of the Device.The Firmware Management
 * function could rely on this resource.
 */
#define RID_FIRMWARE_VERSION 3

/**
 * Reboot: E, Single, Mandatory
 * type: N/A, range: N/A, unit: N/A
 * Reboot the LwM2M Device to restore the Device from unexpected firmware
 * failure.
 */
#define RID_REBOOT 4

/**
 * Error Code: R, Multiple, Mandatory
 * type: integer, range: 0..8, unit: N/A
 * 0=No error 1=Low battery power 2=External power supply off 3=GPS
 * module failure 4=Low received signal strength 5=Out of memory 6=SMS
 * failure 7=IP connectivity failure 8=Peripheral malfunction  When the
 * single Device Object Instance is initiated, there is only one error
 * code Resource Instance whose value is equal to 0 that means no error.
 * When the first error happens, the LwM2M Client changes error code
 * Resource Instance to any non-zero value to indicate the error type.
 * When any other error happens, a new error code Resource Instance is
 * created. When an error associated with a Resource Instance is no
 * longer present, that Resource Instance is deleted. When the single
 * existing error is no longer present, the LwM2M Client returns to the
 * original no error state where Instance 0 has value 0. This error code
 * Resource MAY be observed by the LwM2M Server. How to deal with LwM2M
 * Client’s error report depends on the policy of the LwM2M Server.
 */
#define RID_ERROR_CODE 11

/**
 * Current Time: RW, Single, Optional
 * type: time, range: N/A, unit: N/A
 * Current UNIX time of the LwM2M Client. The LwM2M Client should be
 * responsible to increase this time value as every second elapses. The
 * LwM2M Server is able to write this Resource to make the LwM2M Client
 * synchronized with the LwM2M Server.
 */
#define RID_CURRENT_TIME 13

/**
 * Supported Binding and Modes: R, Single, Mandatory
 * type: string, range: N/A, unit: N/A
 * Indicates which bindings and modes are supported in the LwM2M Client.
 * The possible values are those listed in the LwM2M Core Specification.
 */
#define RID_SUPPORTED_BINDING_AND_MODES 16

/**
 * Software Version: R, Single, Optional
 * type: string, range: N/A, unit: N/A
 * Current software version of the device (manufacturer specified
 * string). On elaborated LwM2M device, SW could be split in 2 parts: a
 * firmware one and a higher level software on top. Both pieces of
 * Software are together managed by LwM2M Firmware Update Object (Object
 * ID 5)
 */
#define RID_SOFTWARE_VERSION 19

struct device_object {
	const anjay_dm_object_def_t *def;

	struct device_id serial_number;
#ifdef CONFIG_ANJAY_CLIENT_FOTA
	char fw_version[BOOT_IMG_VER_STRLEN_MAX];
#endif // CONFIG_ANJAY_CLIENT_FOTA
	bool do_reboot;
};

static inline struct device_object *get_obj(const anjay_dm_object_def_t *const *obj_ptr)
{
	assert(obj_ptr);
	return AVS_CONTAINER_OF(obj_ptr, struct device_object, def);
}

static int list_resources(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid, anjay_dm_resource_list_ctx_t *ctx)
{
	(void)anjay;
	(void)obj_ptr;
	(void)iid;

	anjay_dm_emit_res(ctx, RID_MANUFACTURER, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_MODEL_NUMBER, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_SERIAL_NUMBER, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_FIRMWARE_VERSION, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_REBOOT, ANJAY_DM_RES_E, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_ERROR_CODE, ANJAY_DM_RES_RM, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_CURRENT_TIME, ANJAY_DM_RES_RW, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_SUPPORTED_BINDING_AND_MODES, ANJAY_DM_RES_R,
			  ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_SOFTWARE_VERSION, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	return 0;
}

static int resource_read(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			 anjay_iid_t iid, anjay_rid_t rid, anjay_riid_t riid,
			 anjay_output_ctx_t *ctx)
{
	(void)anjay;

	struct device_object *obj = get_obj(obj_ptr);

	assert(obj);
	assert(iid == 0);

	switch (rid) {
	case RID_MANUFACTURER:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_string(ctx, CONFIG_ANJAY_CLIENT_DEVICE_MANUFACTURER);

	case RID_MODEL_NUMBER:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_string(ctx, CONFIG_ANJAY_CLIENT_MODEL_NUMBER);

	case RID_SERIAL_NUMBER:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_string(ctx, obj->serial_number.value);

	case RID_FIRMWARE_VERSION:
		assert(riid == ANJAY_ID_INVALID);
#ifdef CONFIG_ANJAY_CLIENT_FOTA
		return anjay_ret_string(ctx, obj->fw_version);
#else // CONFIG_ANJAY_CLIENT_FOTA
		return anjay_ret_string(ctx, CLIENT_VERSION);
#endif // CONFIG_ANJAY_CLIENT_FOTA

	case RID_ERROR_CODE:
		assert(riid == 0);
		return anjay_ret_i32(ctx, 0);

	case RID_CURRENT_TIME:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_i64(ctx, avs_time_real_now().since_real_epoch.seconds);

	case RID_SUPPORTED_BINDING_AND_MODES:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_string(ctx, SUPPORTED_BINDING_MODES);

	case RID_SOFTWARE_VERSION:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_string(ctx, anjay_get_version());

	default:
		return ANJAY_ERR_METHOD_NOT_ALLOWED;
	}
}

static int resource_write(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid, anjay_rid_t rid, anjay_riid_t riid,
			  anjay_input_ctx_t *ctx)
{
	(void)anjay;
	(void)obj_ptr;

	assert(iid == 0);

	switch (rid) {
	case RID_CURRENT_TIME: {
#ifdef CONFIG_POSIX_API
		struct timespec ts;

		ts.tv_nsec = 0;
		if (anjay_get_i64(ctx, &ts.tv_sec) || clock_settime(CLOCK_REALTIME, &ts)) {
			return -1;
		}
		return 0;
#else // CONFIG_POSIX_API
		return ANJAY_ERR_NOT_IMPLEMENTED;
#endif // CONFIG_POSIX_API
	}
	default:
		return ANJAY_ERR_METHOD_NOT_ALLOWED;
	}
}

static int resource_execute(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			    anjay_iid_t iid, anjay_rid_t rid, anjay_execute_ctx_t *arg_ctx)
{
	(void)arg_ctx;

	struct device_object *obj = get_obj(obj_ptr);

	assert(obj);
	assert(iid == 0);

	switch (rid) {
	case RID_REBOOT:
		obj->do_reboot = true;
		return 0;

	default:
		return ANJAY_ERR_METHOD_NOT_ALLOWED;
	}
}

static int list_resource_instances(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
				   anjay_iid_t iid, anjay_rid_t rid, anjay_dm_list_ctx_t *ctx)
{
	(void)anjay;
	(void)obj_ptr;

	assert(iid == 0);

	switch (rid) {
	case RID_ERROR_CODE:
		anjay_dm_emit(ctx, 0);
		return 0;
	default:
		return ANJAY_ERR_METHOD_NOT_ALLOWED;
	}
}

static const anjay_dm_object_def_t obj_def = {
	.oid = 3,
	.handlers = { .list_instances = anjay_dm_list_instances_SINGLE,

		      .list_resources = list_resources,
		      .resource_read = resource_read,
		      .resource_write = resource_write,
		      .resource_execute = resource_execute,
		      .list_resource_instances = list_resource_instances,

		      .transaction_begin = anjay_dm_transaction_NOOP,
		      .transaction_validate = anjay_dm_transaction_NOOP,
		      .transaction_commit = anjay_dm_transaction_NOOP,
		      .transaction_rollback = anjay_dm_transaction_NOOP }
};

const anjay_dm_object_def_t **device_object_create(void)
{
	struct device_object *obj =
		(struct device_object *)avs_calloc(1, sizeof(struct device_object));
	if (!obj) {
		return NULL;
	}
	obj->def = &obj_def;

	if (get_device_id(&obj->serial_number)) {
		obj->serial_number.value[0] = '\0';
	}

#ifdef CONFIG_ANJAY_CLIENT_FOTA
	if (get_fw_version_image_0(obj->fw_version, sizeof(obj->fw_version))) {
		obj->fw_version[0] = '\0';
	}
#endif // CONFIG_ANJAY_CLIENT_FOTA

	return &obj->def;
}

void device_object_release(const anjay_dm_object_def_t ***out_def)
{
	const anjay_dm_object_def_t **def = *out_def;

	if (def) {
		struct device_object *obj = get_obj(def);

		avs_free(obj);
		*out_def = NULL;
	}
}

void device_object_update(anjay_t *anjay, const anjay_dm_object_def_t *const *def)
{
	if (!anjay || !def) {
		return;
	}

	struct device_object *obj = get_obj(def);

	if (obj->do_reboot) {
		sys_reboot(SYS_REBOOT_WARM);
	}
}
