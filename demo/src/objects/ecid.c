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
#include <avsystem/commons/avs_memory.h>

#include <zephyr.h>
#include <modem/lte_lc.h>

#include "objects.h"
#include "../nrf_lc_info.h"
#include "../utils.h"

struct ecid_object {
	const anjay_dm_object_def_t *def;

	uint8_t ncells_count_cached;
	struct lte_lc_ncell neighbor_cells_cached[CONFIG_LTE_NEIGHBOR_CELLS_MAX];
};

static inline struct ecid_object *get_obj(const anjay_dm_object_def_t *const *obj_ptr)
{
	assert(obj_ptr);
	return AVS_CONTAINER_OF(obj_ptr, struct ecid_object, def);
}

static int list_instances(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_dm_list_ctx_t *ctx)
{
	(void)anjay;

	struct ecid_object *obj = get_obj(obj_ptr);

	for (anjay_iid_t iid = 0; iid < obj->ncells_count_cached; iid++) {
		anjay_dm_emit(ctx, iid);
	}

	return 0;
}

static int list_resources(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			  anjay_iid_t iid, anjay_dm_resource_list_ctx_t *ctx)
{
	(void)anjay;
	(void)obj_ptr;
	(void)iid;

	anjay_dm_emit_res(ctx, RID_ECID_PHYSCELLID, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_ECID_ARFCNEUTRA, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_ECID_RSRP_RESULT, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_ECID_RSRQ_RESULT, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	anjay_dm_emit_res(ctx, RID_ECID_UE_RXTXTIMEDIFF, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
	return 0;
}

static int resource_read(anjay_t *anjay, const anjay_dm_object_def_t *const *obj_ptr,
			 anjay_iid_t iid, anjay_rid_t rid, anjay_riid_t riid,
			 anjay_output_ctx_t *ctx)
{
	(void)anjay;

	struct ecid_object *obj = get_obj(obj_ptr);

	assert(obj);
	assert(iid < obj->ncells_count_cached);
	struct lte_lc_ncell *inst = &obj->neighbor_cells_cached[iid];

	switch (rid) {
	case RID_ECID_PHYSCELLID:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_i32(ctx, inst->phys_cell_id);

	case RID_ECID_ARFCNEUTRA:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_i32(ctx, inst->earfcn);

	case RID_ECID_RSRP_RESULT:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_i32(ctx, inst->rsrp);

	case RID_ECID_RSRQ_RESULT:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_i32(ctx, inst->rsrq);

	case RID_ECID_UE_RXTXTIMEDIFF:
		assert(riid == ANJAY_ID_INVALID);
		return anjay_ret_i32(ctx, inst->time_diff);

	default:
		return ANJAY_ERR_METHOD_NOT_ALLOWED;
	}
}

static const anjay_dm_object_def_t OBJ_DEF = { .oid = OID_ECID,
					       .handlers = { .list_instances = list_instances,

							     .list_resources = list_resources,
							     .resource_read = resource_read } };

const anjay_dm_object_def_t **ecid_object_create(const struct nrf_lc_info *nrf_lc_info)
{
	struct ecid_object *obj = (struct ecid_object *)avs_calloc(1, sizeof(struct ecid_object));

	if (!obj) {
		return NULL;
	}
	obj->def = &OBJ_DEF;

	assert(nrf_lc_info->cells.ncells_count < CONFIG_LTE_NEIGHBOR_CELLS_MAX);

	memcpy(&obj->neighbor_cells_cached, &nrf_lc_info->cells.neighbor_cells,
	       nrf_lc_info->cells.ncells_count * sizeof(*nrf_lc_info->cells.neighbor_cells));
	obj->ncells_count_cached = nrf_lc_info->cells.ncells_count;

	return &obj->def;
}

void ecid_object_update(anjay_t *anjay, const anjay_dm_object_def_t *const *def,
			const struct nrf_lc_info *nrf_lc_info)
{
	if (!anjay || !def) {
		return;
	}

	struct ecid_object *obj = get_obj(def);

	assert(nrf_lc_info->cells.ncells_count < CONFIG_LTE_NEIGHBOR_CELLS_MAX);

	// overwrite previously reported instances
	for (anjay_iid_t iid = 0;
	     iid < AVS_MIN(obj->ncells_count_cached, nrf_lc_info->cells.ncells_count); iid++) {
		struct lte_lc_ncell *cached = &obj->neighbor_cells_cached[iid];
		struct lte_lc_ncell *last = &nrf_lc_info->cells.neighbor_cells[iid];

		if (cached->phys_cell_id != last->phys_cell_id) {
			cached->phys_cell_id = last->phys_cell_id;
			anjay_notify_changed(anjay, obj->def->oid, iid, RID_ECID_PHYSCELLID);
		}

		if (cached->earfcn != last->earfcn) {
			cached->earfcn = last->earfcn;
			anjay_notify_changed(anjay, obj->def->oid, iid, RID_ECID_ARFCNEUTRA);
		}

		if (cached->rsrp != last->rsrp) {
			cached->rsrp = last->rsrp;
			anjay_notify_changed(anjay, obj->def->oid, iid, RID_ECID_RSRP_RESULT);
		}

		if (cached->rsrq != last->rsrq) {
			cached->rsrq = last->rsrq;
			anjay_notify_changed(anjay, obj->def->oid, iid, RID_ECID_RSRQ_RESULT);
		}

		if (cached->time_diff != last->time_diff) {
			cached->time_diff = last->time_diff;
			anjay_notify_changed(anjay, obj->def->oid, iid, RID_ECID_UE_RXTXTIMEDIFF);
		}
	}

	// if new instance count is higher, write the rest without any notifications
	for (anjay_iid_t iid = obj->ncells_count_cached; iid < nrf_lc_info->cells.ncells_count;
	     iid++) {
		obj->neighbor_cells_cached[iid] = nrf_lc_info->cells.neighbor_cells[iid];
	}

	if (obj->ncells_count_cached != nrf_lc_info->cells.ncells_count) {
		obj->ncells_count_cached = nrf_lc_info->cells.ncells_count;
		anjay_notify_instances_changed(anjay, obj->def->oid);
	}
}

uint8_t ecid_object_instance_count(const anjay_dm_object_def_t *const *def)
{
	return def ? get_obj(def)->ncells_count_cached : 0;
}

void ecid_object_release(const anjay_dm_object_def_t ***out_def)
{
	if (out_def && *out_def) {
		struct ecid_object *obj = get_obj(*out_def);

		avs_free(obj);
		*out_def = NULL;
	}
}
