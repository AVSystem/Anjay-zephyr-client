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

#include <stdatomic.h>
#include <string.h>

#include <logging/log.h>
#include <modem/lte_lc.h>
#include <modem/modem_info.h>
#include <zephyr.h>

#include "common.h"
#include "nrf_lc_info.h"
#include "utils.h"

LOG_MODULE_REGISTER(nrf_lc_info);

static struct k_work_delayable periodic_search_dwork;
static struct nrf_lc_info nrf_lc_info = {
	.cells = { .current_cell.id = LTE_LC_CELL_EUTRAN_ID_INVALID,
		   .neighbor_cells = nrf_lc_info.storage.neighbor_cells }
};
static bool nrf_lc_info_changed;
static K_MUTEX_DEFINE(nrf_lc_info_mtx);

static void lte_lc_evt_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
	case LTE_LC_EVT_LTE_MODE_UPDATE: {
		LOG_INF("LTE mode updated: %d", evt->lte_mode);

		SYNCHRONIZED(nrf_lc_info_mtx)
		{
			nrf_lc_info.lte_mode = evt->lte_mode;
			nrf_lc_info_changed = true;
		}
		break;
	}
	case LTE_LC_EVT_NEIGHBOR_CELL_MEAS: {
		LOG_INF("Neighbor cells measurement received");
		const struct lte_lc_cells_info *cells = &evt->cells_info;

		if (cells->current_cell.id == LTE_LC_CELL_EUTRAN_ID_INVALID) {
			LOG_INF("Not connected to any cell");
		} else {
			LOG_INF("Connected to cell E-UTRAN ID %" PRIu32, cells->current_cell.id);
		}

		static char addr_buf[MODEM_INFO_MAX_RESPONSE_SIZE];
		int err = modem_info_string_get(MODEM_INFO_IP_ADDRESS, addr_buf, sizeof(addr_buf));

		SYNCHRONIZED(nrf_lc_info_mtx)
		{
			// treat empty string also as an error
			if (err <= 0) {
				LOG_WRN("Couldn't retrieve the IP address, error: %d", err);
				nrf_lc_info.ip_addr[0] = '\0';
			} else {
				strcpy(nrf_lc_info.ip_addr, addr_buf);
			}

			memcpy(&nrf_lc_info.cells.current_cell, &cells->current_cell,
			       sizeof(cells->current_cell));

			if (cells->ncells_count > 0 && cells->neighbor_cells) {
				assert(cells->ncells_count <= CONFIG_LTE_NEIGHBOR_CELLS_MAX);

				memcpy(nrf_lc_info.storage.neighbor_cells, cells->neighbor_cells,
				       cells->ncells_count * sizeof(*cells->neighbor_cells));
				nrf_lc_info.cells.ncells_count = cells->ncells_count;
			} else {
				nrf_lc_info.cells.ncells_count = 0;
			}
			nrf_lc_info_changed = true;
		}

		LOG_INF("Found %d neighbor cells", nrf_lc_info.cells.ncells_count);
		break;
	}
	default:
		break;
	}
}

static void periodic_search_work_handler(struct k_work *work)
{
	// TODO: consider using other search types, which work on nRF9160's with FW 1.3.1 and up
	int err = lte_lc_neighbor_cell_measurement(LTE_LC_NEIGHBOR_SEARCH_TYPE_DEFAULT);

	if (err) {
		LOG_ERR("Can't search for neighbor cells, error: %d", err);
	}

	k_work_schedule(k_work_delayable_from_work(work),
			K_SECONDS(CONFIG_ANJAY_CLIENT_NRF_LC_INFO_CELL_POLL_RATE));
}

int initialize_nrf_lc_info_listener(void)
{
	int res;

	res = modem_info_init();
	if (res) {
		return res;
	}

	enum lte_lc_lte_mode lte_mode;

	res = lte_lc_lte_mode_get(&lte_mode);
	if (res) {
		return res;
	}

	SYNCHRONIZED(nrf_lc_info_mtx)
	{
		nrf_lc_info.lte_mode = lte_mode;
	}

	lte_lc_register_handler(lte_lc_evt_handler);
	k_work_init_delayable(&periodic_search_dwork, periodic_search_work_handler);

	res = k_work_schedule(&periodic_search_dwork, K_NO_WAIT);
	if (res < 0) {
		return res;
	}

	return 0;
}

bool nrf_lc_info_get_if_changed(struct nrf_lc_info *out)
{
	bool changed = false;

	SYNCHRONIZED(nrf_lc_info_mtx)
	{
		changed = nrf_lc_info_changed;
		if (changed) {
			nrf_lc_info_changed = false;

			*out = nrf_lc_info;
			out->cells.neighbor_cells = out->storage.neighbor_cells;
		}
	}
	return changed;
}

void nrf_lc_info_get(struct nrf_lc_info *out)
{
	SYNCHRONIZED(nrf_lc_info_mtx)
	{
		*out = nrf_lc_info;
		out->cells.neighbor_cells = out->storage.neighbor_cells;
	}
}
