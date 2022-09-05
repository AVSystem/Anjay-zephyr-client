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

#include <zephyr/logging/log.h>

#include <modem/lte_lc.h>

#include "network.h"
#include "network_internal.h"

#include "../common.h"
#include "../config.h"
#include "../gps.h"
#include "../utils.h"

LOG_MODULE_REGISTER(network_nrf91);

static volatile atomic_int lte_nw_reg_status; // enum lte_lc_nw_reg_status
static volatile atomic_int lte_mode; // enum lte_lc_lte_mode

static void lte_evt_handler(const struct lte_lc_evt *const evt)
{
	if (evt) {
		if (evt->type == LTE_LC_EVT_NW_REG_STATUS) {
			atomic_store(&lte_nw_reg_status, (int)evt->nw_reg_status);
		} else if (evt->type == LTE_LC_EVT_LTE_MODE_UPDATE) {
			atomic_store(&lte_mode, (int)evt->lte_mode);
		}
	}
	network_internal_connection_state_changed();
}

int network_internal_platform_initialize(void)
{
	int ret = lte_lc_init();

	if (!ret) {
		lte_lc_register_handler(lte_evt_handler);
	}

	return ret;
}

int network_connect_async(void)
{
	int ret = 0;

	// Note: this is supposed to be handled by lte_lc_connect_async(),
	// but there is a nasty bug in in_progress flag handling
	if (!network_is_connected()) {
		ret = lte_lc_connect_async(lte_evt_handler);
	}

	if (ret > 0 || ret == -EALREADY || ret == -EINPROGRESS) {
		ret = 0;
	}
	if (ret) {
		LOG_ERR("LTE link could not be established.");
	}
	return ret;
}

enum network_bearer_t network_current_bearer(void)
{
#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF
	if (atomic_load(&gps_prio_mode)) {
		return NETWORK_BEARER_LIMIT;
	}
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF

	if (atomic_load(&lte_mode) == LTE_LC_LTE_MODE_NONE) {
		return NETWORK_BEARER_LIMIT;
	}

	int status = atomic_load(&lte_nw_reg_status);

	if (status == LTE_LC_NW_REG_REGISTERED_HOME || status == LTE_LC_NW_REG_REGISTERED_ROAMING) {
		return NETWORK_BEARER_CELLULAR;
	} else {
		return NETWORK_BEARER_LIMIT;
	}
}

void network_disconnect(void)
{
	int ret = lte_lc_offline();

	if (ret) {
		LOG_WRN("LTE link could not be disconnected: %d", ret);
	}
}
