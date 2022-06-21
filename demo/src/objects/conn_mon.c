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
#include <string.h>

#include <anjay/anjay.h>
#include <avsystem/commons/avs_defs.h>
#include <avsystem/commons/avs_list.h>
#include <avsystem/commons/avs_memory.h>

#include <devicetree.h>
#include <drivers/pwm.h>
#include <zephyr.h>

#include "objects.h"
#include "../nrf_lc_info.h"

#define LTE_FDD_BEARER 6
#define NB_IOT_BEARER 7

/* Modem returns RSRP as a index value which requires
 * a conversion to dBm. See modem AT command reference
 * guide for more information.
 */
#define RSRP_ADJ(rsrp) ((rsrp) - (((rsrp) <= 0) ? 140 : 141))

struct conn_mon_object {
	const anjay_dm_object_def_t *def;

	enum lte_lc_lte_mode lte_mode;
	int mcc;
	int mnc;
	int16_t rsrp;
	int16_t rsrq;
	uint32_t cell_id;
	uint32_t area_code;
	char ip_address[MODEM_INFO_MAX_RESPONSE_SIZE];
};

static inline struct conn_mon_object *get_obj(const anjay_dm_object_def_t *const *obj_ptr)
{
	assert(obj_ptr);
	return AVS_CONTAINER_OF(obj_ptr, struct conn_mon_object, def);
}

static int list_resources(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid, anjay_dm_resource_list_ctx_t *ctx)
{
	(void)anjay;
	(void)obj_ptr;
	(void)iid;

	anjay_dm_emit_res(ctx, RID_CONN_MON_NETWORK_BEARER, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_CONN_MON_AVAILABLE_NETWORK_BEARER, ANJAY_DM_RES_RM,
			  ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_CONN_MON_RSS, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_CONN_MON_LINK_QUALITY, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_CONN_MON_IP_ADDRESSES, ANJAY_DM_RES_RM, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_CONN_MON_CELL_ID, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_CONN_MON_SMNC, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_CONN_MON_SMCC, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_CONN_MON_LAC, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	return 0;
}

static int resource_read(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			 anjay_iid_t iid, anjay_rid_t rid, anjay_riid_t riid,
			 anjay_output_ctx_t *ctx)
{
	(void)anjay;

	struct conn_mon_object *obj = get_obj(obj_ptr);

	assert(obj);
	assert(iid == 0);

	switch (rid) {
	case RID_CONN_MON_NETWORK_BEARER:
		assert(riid == ANJAY_ID_INVALID);
		switch (obj->lte_mode) {
		case LTE_LC_LTE_MODE_LTEM:
			return anjay_ret_i32(ctx, LTE_FDD_BEARER);
		case LTE_LC_LTE_MODE_NBIOT:
			return anjay_ret_i32(ctx, NB_IOT_BEARER);
		default:
			return anjay_ret_i32(ctx, 0);
		}

	case RID_CONN_MON_AVAILABLE_NETWORK_BEARER:
		switch (riid) {
		case 0:
			return anjay_ret_i32(ctx, LTE_FDD_BEARER);
		case 1:
			return anjay_ret_i32(ctx, NB_IOT_BEARER);
		default:
			return ANJAY_ERR_METHOD_NOT_ALLOWED;
		}

	case RID_CONN_MON_RSS:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_i32(ctx, RSRP_ADJ(obj->rsrp));

	case RID_CONN_MON_LINK_QUALITY:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_i32(ctx, obj->rsrq);

	case RID_CONN_MON_IP_ADDRESSES:
		return anjay_ret_string(ctx, obj->ip_address);

	case RID_CONN_MON_CELL_ID:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_i32(ctx, obj->cell_id);

	case RID_CONN_MON_SMNC:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_i32(ctx, obj->mnc);

	case RID_CONN_MON_SMCC:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_i32(ctx, obj->mcc);

	case RID_CONN_MON_LAC:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_i32(ctx, obj->area_code);

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
	case RID_CONN_MON_AVAILABLE_NETWORK_BEARER:
		anjay_dm_emit(ctx, 0);
		anjay_dm_emit(ctx, 1);
		return 0;

	case RID_CONN_MON_IP_ADDRESSES:
		anjay_dm_emit(ctx, 0);
		return 0;

	default:
		return ANJAY_ERR_METHOD_NOT_ALLOWED;
	}
}

static const anjay_dm_object_def_t obj_def = {
	.oid = OID_CONN_MON,
	.version = "1.2",
	.handlers = { .list_instances = anjay_dm_list_instances_SINGLE,
		      .list_resources = list_resources,
		      .resource_read = resource_read,
		      .list_resource_instances = list_resource_instances }
};

const anjay_dm_object_def_t **conn_mon_object_create(const struct nrf_lc_info *nrf_lc_info)
{
	struct conn_mon_object *obj =
		(struct conn_mon_object *)avs_calloc(1, sizeof(struct conn_mon_object));
	if (!obj) {
		return NULL;
	}

	obj->def = &obj_def;

	struct lte_lc_cell const *cell_info = &nrf_lc_info->cells.current_cell;

	/* write only data to obj if it's valid */
	if (cell_info->id != LTE_LC_CELL_EUTRAN_ID_INVALID) {
		obj->lte_mode = nrf_lc_info->lte_mode;
		obj->rsrp = cell_info->rsrp;
		obj->rsrq = cell_info->rsrq;
		obj->cell_id = cell_info->id;
		obj->mnc = cell_info->mnc;
		obj->mcc = cell_info->mcc;
		obj->area_code = cell_info->tac;

		AVS_STATIC_ASSERT(sizeof(obj->ip_address) == sizeof(nrf_lc_info->ip_addr),
				  ip_address_fields_compatible);
		memcpy(obj->ip_address, nrf_lc_info->ip_addr, sizeof(obj->ip_address));
	}

	return &obj->def;
}

void conn_mon_object_update(anjay_t *anjay, const anjay_dm_object_def_t *const *def,
			    const struct nrf_lc_info *nrf_lc_info)
{
	if (!anjay || !def) {
		return;
	}

	struct conn_mon_object *obj = get_obj(def);

	struct lte_lc_cell const *cell_info = &nrf_lc_info->cells.current_cell;

	if (cell_info->id != LTE_LC_CELL_EUTRAN_ID_INVALID) {
		if (nrf_lc_info->lte_mode != obj->lte_mode) {
			obj->lte_mode = nrf_lc_info->lte_mode;
			anjay_notify_changed(anjay, obj->def->oid, 0, RID_CONN_MON_NETWORK_BEARER);
		}

		if (cell_info->rsrp != obj->rsrp) {
			obj->rsrp = cell_info->rsrp;
			anjay_notify_changed(anjay, obj->def->oid, 0, RID_CONN_MON_RSS);
		}

		if (cell_info->rsrq != obj->rsrq) {
			obj->rsrq = cell_info->rsrq;
			anjay_notify_changed(anjay, obj->def->oid, 0, RID_CONN_MON_LINK_QUALITY);
		}

		if (strcmp(obj->ip_address, nrf_lc_info->ip_addr)) {
			memcpy(obj->ip_address, nrf_lc_info->ip_addr, sizeof(obj->ip_address));
			anjay_notify_changed(anjay, obj->def->oid, 0, RID_CONN_MON_IP_ADDRESSES);
		}

		if (cell_info->id != obj->cell_id) {
			obj->cell_id = cell_info->id;
			anjay_notify_changed(anjay, obj->def->oid, 0, RID_CONN_MON_CELL_ID);
		}

		if (cell_info->mnc != obj->mnc) {
			obj->mnc = cell_info->mnc;
			anjay_notify_changed(anjay, obj->def->oid, 0, RID_CONN_MON_SMNC);
		}

		if (cell_info->mcc != obj->mcc) {
			obj->mcc = cell_info->mcc;
			anjay_notify_changed(anjay, obj->def->oid, 0, RID_CONN_MON_SMCC);
		}

		if (cell_info->tac != obj->area_code) {
			obj->area_code = cell_info->tac;
			anjay_notify_changed(anjay, obj->def->oid, 0, RID_CONN_MON_LAC);
		}
	}
}

void conn_mon_object_release(const anjay_dm_object_def_t ***out_def)
{
	if (out_def && *out_def) {
		struct conn_mon_object *obj = get_obj(*out_def);

		avs_free(obj);
		*out_def = NULL;
	}
}
