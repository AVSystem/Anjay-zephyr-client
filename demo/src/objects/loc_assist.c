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
#include <logging/log.h>

#include <anjay/anjay.h>
#include <anjay/lwm2m_send.h>
#include <anjay/server.h>

#include <avsystem/commons/avs_defs.h>
#include <avsystem/commons/avs_memory.h>

#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS
#include <net/nrf_cloud_agps.h>
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS

#include "objects.h"
#include "../nrf_lc_info.h"

LOG_MODULE_REGISTER(loc_assist);

/**
 * Assistance type: R, Single, Mandatory
 * type: integer, range: 0..6, unit: N/A
 * 0 = Idle, not waiting for assistance  1 = single cell location inform
 * 2 = multi cell location inform  3 = single cell request, send location
 * back  4 = multi cell request, send location back  5 = A-GPS  6 = P-GPS
 */
#define RID_ASSISTANCE_TYPE 0

/**
 * A-GPS assistance mask: R, Single, Optional
 * type: integer, range: N/A, unit: N/A
 * Bitfield to define what kind of assistance the A-GPS module needs bit
 * 0 = needs UTC parameters  bit 1 = requires ephemeries data bit 2 =
 * requires almanac data ... To be defined later
 */
#define RID_A_GPS_ASSISTANCE_MASK 1

/**
 * P-GPS predictionCount: R, Single, Optional
 * type: integer, range: N/A, unit: N/A
 * How many GPS predictions are needed
 */
#define RID_P_GPS_PREDICTIONCOUNT 2

/**
 * P-GPS predictionIntervalMinutes: R, Single, Optional
 * type: integer, range: N/A, unit: min
 * The time between predictions (in minutes).
 */
#define RID_P_GPS_PREDICTIONINTERVALMINUTES 3

/**
 * P-GPS startGpsDay: R, Single, Optional
 * type: integer, range: N/A, unit: N/A
 * The start day of the prediction set as GPS Day
 */
#define RID_P_GPS_STARTGPSDAY 4

/**
 * P-GPS startGpsTimeOfDaySeconds: R, Single, Optional
 * type: integer, range: N/A, unit: N/A
 * The start time of the prediction set as seconds in day.
 */
#define RID_P_GPS_STARTGPSTIMEOFDAYSECONDS 5

/**
 * assistance_data: W, Single, Optional
 * type: opaque, range: N/A, unit: N/A
 * Binary assistance data which is going to be written to GNSS module
 */
#define RID_ASSISTANCE_DATA 6

/**
 * result code: W, Single, Mandatory
 * type: string, range: N/A, unit: N/A
 * Description.
 */
#define RID_RESULT_CODE 7

/**
 * latitude: W, Single, Optional
 * type: float, range: N/A, unit: N/A
 * Response latitude
 */
#define RID_LATITUDE 8

/**
 * longitude: W, Single, Optional
 * type: float, range: N/A, unit: N/A
 * Response longitude
 */
#define RID_LONGITUDE 9

/**
 * altitude: W, Single, Optional
 * type: float, range: N/A, unit: N/A
 * Response altitude
 */
#define RID_ALTITUDE 10

/**
 * accuracy: W, Single, Optional
 * type: float, range: N/A, unit: N/A
 * Response accuracy
 */
#define RID_ACCURACY 11

#define ASSISTANCE_DATA_BUF_SIZE 4096

#define SEND_RES_PATH(Oid, Iid, Rid)                                                               \
	{                                                                                          \
		.oid = (Oid), .iid = (Iid), .rid = (Rid)                                           \
	}

#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
struct loc_assist_location {
	double latitude;
	double longitude;
	double altitude;
	double accuracy;
};
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED

struct loc_assist_object {
	const anjay_dm_object_def_t *def;
#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS
	uint8_t assistance_data_buf[ASSISTANCE_DATA_BUF_SIZE];
	size_t assistance_data_len;
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS
#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
	struct loc_assist_location location_current;
	struct loc_assist_location location_backup;
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
};

static inline struct loc_assist_object *get_obj(const anjay_dm_object_def_t *const *obj_ptr)
{
	assert(obj_ptr);
	return AVS_CONTAINER_OF(obj_ptr, struct loc_assist_object, def);
}

static int instance_reset(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid)
{
	(void)anjay;
	(void)iid;

	struct loc_assist_object *obj = get_obj(obj_ptr);

	assert(obj);
	assert(iid == 0);

#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
	memset(&obj->location_current, 0, sizeof(obj->location_current));
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED

	return 0;
}

static int list_resources(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid, anjay_dm_resource_list_ctx_t *ctx)
{
	(void)anjay;
	(void)obj_ptr;
	(void)iid;

	anjay_dm_emit_res(ctx, RID_ASSISTANCE_TYPE, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);

#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS
	anjay_dm_emit_res(ctx, RID_A_GPS_ASSISTANCE_MASK, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS

#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF_P_GPS
#error "P-GPS not implemented yet"
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF_P_GPS

#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_ASSISTANCE
	anjay_dm_emit_res(ctx, RID_ASSISTANCE_DATA, ANJAY_DM_RES_W, ANJAY_DM_RES_PRESENT);
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERIVCES_ASSISTANCE

	anjay_dm_emit_res(ctx, RID_RESULT_CODE, ANJAY_DM_RES_W, ANJAY_DM_RES_PRESENT);

#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
	anjay_dm_emit_res(ctx, RID_LATITUDE, ANJAY_DM_RES_W, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_LONGITUDE, ANJAY_DM_RES_W, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_ALTITUDE, ANJAY_DM_RES_W, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_ACCURACY, ANJAY_DM_RES_W, ANJAY_DM_RES_PRESENT);
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED

	return 0;
}

static int resource_write(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid, anjay_rid_t rid, anjay_riid_t riid,
			  anjay_input_ctx_t *ctx)
{
	(void)anjay;
	(void)iid;

	struct loc_assist_object *obj = get_obj(obj_ptr);

	assert(obj);
	assert(iid == 0);

	switch (rid) {
#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_ASSISTANCE
	case RID_ASSISTANCE_DATA: {
		assert(riid == ANJAY_ID_INVALID);

		bool finished;
		int err =
			anjay_get_bytes(ctx, &obj->assistance_data_len, &finished,
					obj->assistance_data_buf, sizeof(obj->assistance_data_buf));

		if (err) {
			return ANJAY_ERR_INTERNAL;
		}

		if (!finished) {
			return ANJAY_ERR_BAD_REQUEST;
		}

		return 0;
	}
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_ASSISTANCE
	case RID_RESULT_CODE: {
		assert(riid == ANJAY_ID_INVALID);

		char value[64];
		int err = anjay_get_string(ctx, value, sizeof(value));

		if (err == ANJAY_BUFFER_TOO_SHORT) {
			return ANJAY_ERR_BAD_REQUEST;
		}
		if (err) {
			return ANJAY_ERR_INTERNAL;
		}
		LOG_WRN("Received result code: %s", value);
		return 0;
	}

#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
	case RID_LATITUDE: {
		assert(riid == ANJAY_ID_INVALID);
		return anjay_get_double(ctx, &obj->location_current.latitude);
	}

	case RID_LONGITUDE: {
		assert(riid == ANJAY_ID_INVALID);
		return anjay_get_double(ctx, &obj->location_current.longitude);
	}

	case RID_ALTITUDE: {
		assert(riid == ANJAY_ID_INVALID);
		return anjay_get_double(ctx, &obj->location_current.altitude);
	}

	case RID_ACCURACY: {
		assert(riid == ANJAY_ID_INVALID);
		return anjay_get_double(ctx, &obj->location_current.accuracy);
	}
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED

	default:
		return ANJAY_ERR_METHOD_NOT_ALLOWED;
	}
}

static int transaction_begin(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr)
{
	(void)anjay;

#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
	struct loc_assist_object *obj = get_obj(obj_ptr);

	memcpy(&obj->location_backup, &obj->location_current, sizeof(obj->location_current));
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED

	return 0;
}

static inline int angle_validate(double angle)
{
	return isfinite(angle) && angle >= -180.0 && angle <= 180.0 ? 0 : -1;
}

static inline int accuracy_validate(double accuracy)
{
	return isfinite(accuracy) && accuracy >= 0 ? 0 : -1;
}

static int transaction_validate(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr)
{
	(void)anjay;

	struct loc_assist_object *obj = get_obj(obj_ptr);

#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
	if (angle_validate(obj->location_current.latitude) ||
	    angle_validate(obj->location_current.longitude) ||
	    !isfinite(obj->location_current.altitude) ||
	    accuracy_validate(obj->location_current.accuracy)) {
		return ANJAY_ERR_BAD_REQUEST;
	}
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
	return 0;
}

static int transaction_commit(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr)
{
	(void)anjay;

	struct loc_assist_object *obj = get_obj(obj_ptr);

#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS
	if (obj->assistance_data_len > 0) {
		LOG_INF("Received %zu bytes of A-GPS data", obj->assistance_data_len);

		int err =
			nrf_cloud_agps_process(obj->assistance_data_buf, obj->assistance_data_len);
		obj->assistance_data_len = 0;

		if (err) {
			LOG_ERR("Unable to process A-GPS data, error: %d", err);
			return ANJAY_ERR_INTERNAL;
		}

		LOG_INF("A-GPS data processed");
	}
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS

#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED

	if (obj->location_current.latitude != obj->location_backup.latitude ||
	    obj->location_current.longitude != obj->location_backup.longitude ||
	    obj->location_current.altitude != obj->location_backup.altitude ||
	    obj->location_current.accuracy != obj->location_backup.accuracy) {
		LOG_INF("Updated cell-based location"
			", lat: %.3f deg, lon: %.3f deg, alt: %.3f m, acc: %.3f m",
			obj->location_current.latitude, obj->location_current.longitude,
			obj->location_current.altitude, obj->location_current.accuracy);
	}
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED

	return 0;
}

static int transaction_rollback(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr)
{
	(void)anjay;

	struct loc_assist_object *obj = get_obj(obj_ptr);

#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS
	obj->assistance_data_len = 0;
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS

#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
	memcpy(&obj->location_current, &obj->location_backup, sizeof(obj->location_backup));
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED

	return 0;
}

static const anjay_dm_object_def_t OBJ_DEF = {
	.oid = 50001,
	.handlers = { .list_instances = anjay_dm_list_instances_SINGLE,
		      .instance_reset = instance_reset,

		      .list_resources = list_resources,
		      .resource_write = resource_write,

		      .transaction_commit = transaction_commit,
		      .transaction_begin = transaction_begin,
		      .transaction_validate = transaction_validate,
		      .transaction_rollback = transaction_rollback }
};

const anjay_dm_object_def_t **loc_assist_object_create(void)
{
	struct loc_assist_object *obj =
		(struct loc_assist_object *)avs_calloc(1, sizeof(struct loc_assist_object));

	if (!obj) {
		return NULL;
	}
	obj->def = &OBJ_DEF;

	return &obj->def;
}

static inline bool is_deferrable_condition(anjay_send_result_t condition)
{
	return condition == ANJAY_SEND_ERR_OFFLINE || condition == ANJAY_SEND_ERR_BOOTSTRAP;
}

static int batch_compile_and_send(anjay_t *anjay, anjay_send_batch_builder_t **builder_ptr,
				  const char *req_kind)
{
	int result = -1;
	anjay_send_batch_t *batch = anjay_send_batch_builder_compile(builder_ptr);

	if (!batch) {
		LOG_ERR("Batch compilation failed");
		return result;
	}

	// best effort - we're sending the request to all servers, although some of them
	// might not answer with correct location at all
	AVS_LIST(const anjay_ssid_t) ssids = anjay_server_get_ssids(anjay);

	if (!ssids) {
		LOG_ERR("No servers to send the batch to");
	}

	AVS_LIST(const anjay_ssid_t) ssid;

	AVS_LIST_FOREACH(ssid, ssids)
	{
		anjay_send_result_t send_result = anjay_send(anjay, *ssid, batch, NULL, NULL);

		if (is_deferrable_condition(send_result)) {
			LOG_WRN("Target SSID=%" PRIu16 " is offline, attempting "
				"deferred send",
				*ssid);
			send_result = anjay_send_deferrable(anjay, *ssid, batch, NULL, NULL);
		}

		if (send_result) {
			LOG_ERR("Couldn't send the %s request to SSID=%" PRIu16 ", err: %d",
				req_kind, *ssid, send_result);
		} else {
			LOG_INF("Sent the %s request to SSID=%" PRIu16, req_kind, *ssid);
			result = 0;
		}
	}

	anjay_send_batch_release(&batch);
	return result;
}

static int add_conn_mon_to_batch(anjay_t *anjay, anjay_send_batch_builder_t *builder)
{
	static const anjay_send_resource_path_t conn_mon_paths[] = {
		SEND_RES_PATH(OID_CONN_MON, 0, RID_CONN_MON_RSS),
		SEND_RES_PATH(OID_CONN_MON, 0, RID_CONN_MON_LINK_QUALITY),
		SEND_RES_PATH(OID_CONN_MON, 0, RID_CONN_MON_CELL_ID),
		SEND_RES_PATH(OID_CONN_MON, 0, RID_CONN_MON_SMNC),
		SEND_RES_PATH(OID_CONN_MON, 0, RID_CONN_MON_SMCC),
		SEND_RES_PATH(OID_CONN_MON, 0, RID_CONN_MON_LAC)
	};
	int result = anjay_send_batch_data_add_current_multiple(builder, anjay, conn_mon_paths,
								AVS_ARRAY_SIZE(conn_mon_paths));
	if (result) {
		LOG_ERR("Failed to add Connectivity Monitoring "
			"required resources to batch, err: %d",
			result);
	}
	return result;
}

#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS
void loc_assist_object_send_agps_request(anjay_t *anjay,
					 const anjay_dm_object_def_t *const *obj_def,
					 uint32_t request_mask)
{
	if (!anjay || !obj_def) {
		return;
	}
	assert(*obj_def == &OBJ_DEF);

	static const struct {
		uint32_t req_flag;
		const char *name;
	} agps_flag_names[] = {
		{ .req_flag = LOC_ASSIST_A_GPS_MASK_UTC, .name = "UTC parameters" },
		{ .req_flag = LOC_ASSIST_A_GPS_MASK_KLOBUCHAR,
		  .name = "Klobuchar ionospheric correction parameters" },
		{ .req_flag = LOC_ASSIST_A_GPS_MASK_NEQUICK,
		  .name = "NeQuick ionospheric correction parameters" },
		{ .req_flag = LOC_ASSIST_A_GPS_MASK_TOW, .name = "SV time of week" },
		{ .req_flag = LOC_ASSIST_A_GPS_MASK_CLOCK, .name = "GPS system time" },
		{ .req_flag = LOC_ASSIST_A_GPS_MASK_LOCATION,
		  .name = "Position assistance parameters" },
		{ .req_flag = LOC_ASSIST_A_GPS_MASK_INTEGRITY,
		  .name = "Integrity assistance parameters" },
		{ .req_flag = LOC_ASSIST_A_GPS_MASK_EPHEMERIS, .name = "GPS ephemeris" },
		{ .req_flag = LOC_ASSIST_A_GPS_MASK_ALMANAC, .name = "GPS almanac" }
	};

	LOG_INF("Requesting following types of A-GPS data:");
	for (size_t i = 0; i < AVS_ARRAY_SIZE(agps_flag_names); i++) {
		if (agps_flag_names[i].req_flag & request_mask) {
			LOG_INF("%s", agps_flag_names[i].name);
		}
	}

	anjay_send_batch_builder_t *builder = anjay_send_batch_builder_new();

	if (!builder) {
		LOG_ERR("Failed to allocate batch builder");
		return;
	}

	int result = anjay_send_batch_add_int(builder, OBJ_DEF.oid, 0, RID_ASSISTANCE_TYPE,
					      UINT16_MAX, avs_time_real_now(), 5);
	if (result) {
		LOG_ERR("Failed to add assistance type to batch, err: %d", result);
		goto finalize_batch;
	}

	// FIXME: current spec uses int for this resource, but uint is more appropriate
	result = anjay_send_batch_add_int(builder, OBJ_DEF.oid, 0, RID_A_GPS_ASSISTANCE_MASK,
					  UINT16_MAX, avs_time_real_now(), request_mask);
	if (result) {
		LOG_ERR("Failed to add assistance type to batch, err: %d", result);
		goto finalize_batch;
	}

	result = add_conn_mon_to_batch(anjay, builder);
	if (result) {
		goto finalize_batch;
	}

finalize_batch:
	if (result) {
		anjay_send_batch_builder_cleanup(&builder);
		return;
	}

	batch_compile_and_send(anjay, &builder, "A-GPS");
	anjay_send_batch_builder_cleanup(&builder);
}
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS

#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
void loc_assist_object_send_cell_request(anjay_t *anjay,
					 const anjay_dm_object_def_t *const *loc_assist_def,
					 const anjay_dm_object_def_t *const *ecid_def,
					 enum loc_assist_cell_request_type request_type)
{
	if (!anjay || !loc_assist_def) {
		return;
	}

	anjay_send_batch_builder_t *builder = anjay_send_batch_builder_new();

	if (!builder) {
		LOG_ERR("Failed to allocate batch builder");
		return;
	}

	int result = anjay_send_batch_add_int(builder, OBJ_DEF.oid, 0, RID_ASSISTANCE_TYPE,
					      UINT16_MAX, avs_time_real_now(), request_type);
	if (result) {
		LOG_ERR("Failed to add assistance type to batch, err: %d", result);
		goto finalize_batch;
	}

	result = add_conn_mon_to_batch(anjay, builder);
	if (result) {
		goto finalize_batch;
	}

	if (request_type == LOC_ASSIST_CELL_REQUEST_INFORM_MULTI ||
	    request_type == LOC_ASSIST_CELL_REQUEST_REQUEST_MULTI) {
		uint8_t ecid_instance_count = ecid_object_instance_count(ecid_def);

		for (anjay_iid_t iid = 0; iid < ecid_instance_count; iid++) {
			const anjay_send_resource_path_t ecid_paths[] = {
				SEND_RES_PATH(OID_ECID, iid, RID_ECID_PHYSCELLID),
				SEND_RES_PATH(OID_ECID, iid, RID_ECID_ARFCNEUTRA),
				SEND_RES_PATH(OID_ECID, iid, RID_ECID_RSRP_RESULT),
				SEND_RES_PATH(OID_ECID, iid, RID_ECID_RSRQ_RESULT),
				SEND_RES_PATH(OID_ECID, iid, RID_ECID_UE_RXTXTIMEDIFF)
			};

			result = anjay_send_batch_data_add_current_multiple(
				builder, anjay, ecid_paths, AVS_ARRAY_SIZE(ecid_paths));
			if (result) {
				LOG_ERR("Failed to add ECID required resources, iid: %" PRIu16
					", err: %d",
					iid, result);
				goto finalize_batch;
			}
		}
	}

finalize_batch:
	if (result) {
		anjay_send_batch_builder_cleanup(&builder);
		return;
	}

	batch_compile_and_send(anjay, &builder, "cell-based location");
	anjay_send_batch_builder_cleanup(&builder);
}
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED

void loc_assist_object_release(const anjay_dm_object_def_t ***out_def)
{
	if (out_def && *out_def) {
		struct loc_assist_object *obj = get_obj(*out_def);

		avs_free(obj);
		*out_def = NULL;
	}
}
