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
#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_mgmt.h>

#include <wifi/eswifi/eswifi.h>

#include "network.h"
#include "network_internal.h"

#include "../config.h"
#include "../utils.h"

LOG_MODULE_REGISTER(network_wifi);

static struct wifi_connect_req_params wifi_params;

static void eswifi_reconnect_work_cb(struct k_work *work);
static void eswifi_keepalive_work_cb(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(eswifi_reconnect_work, eswifi_reconnect_work_cb);
static K_WORK_DELAYABLE_DEFINE(eswifi_keepalive_work, eswifi_keepalive_work_cb);

static void eswifi_reconnect_work_cb(struct k_work *work)
{
	net_mgmt(NET_REQUEST_WIFI_CONNECT, eswifi_by_iface_idx(0)->iface, &wifi_params,
		 sizeof(struct wifi_connect_req_params));
}

static K_SEM_DEFINE(eswifi_disconnect_sync_result_cb_sem, 0, 1);

static void eswifi_disconnect_sync_result_cb(struct net_mgmt_event_callback *cb,
					     uint32_t mgmt_event, struct net_if *iface)
{
	k_sem_give(&eswifi_disconnect_sync_result_cb_sem);
}

static void eswifi_disconnect_sync(struct net_if *iface)
{
	struct net_mgmt_event_callback cb = { 0 };

	net_mgmt_init_event_callback(&cb, eswifi_disconnect_sync_result_cb,
				     NET_EVENT_WIFI_DISCONNECT_RESULT);
	net_mgmt_add_event_callback(&cb);

	int ret = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);

	if (ret >= 0 || ret == -EINPROGRESS) {
		k_sem_take(&eswifi_disconnect_sync_result_cb_sem, Z_FOREVER);
	}

	net_mgmt_del_event_callback(&cb);
}

static void eswifi_keepalive_work_cb(struct k_work *work)
{
	char *result = NULL;
	struct eswifi_dev *eswifi = eswifi_by_iface_idx(0);

	eswifi_lock(eswifi);

	bool connected =
		(eswifi_at_cmd_rsp(eswifi, "CS\r", &result) >= 0 && result && result[0] == '1');

	eswifi_unlock(eswifi);

	if (!connected) {
		// Lost connection, let's try reconnecting
		eswifi_disconnect_sync(eswifi->iface);

		// Issuing NET_REQUEST_WIFI_CONNECT locks the eswifi mutex for 30 seconds.
		// This blocks e.g. poll() in the event loop - so let's call it via k_work_delayable
		// to give the event loop a bit of time to perform all the close actions.
		k_work_schedule(&eswifi_reconnect_work, K_SECONDS(5));
	} else {
		k_work_schedule(&eswifi_keepalive_work,
				K_SECONDS(CONFIG_ANJAY_CLIENT_NETWORK_KEEPALIVE_RATE));
	}
}

static void eswifi_disconnect_work_cb(struct k_work *work)
{
	struct eswifi_dev *eswifi = eswifi_by_iface_idx(0);

	while (true) {
		struct in_addr *addr =
			net_if_ipv4_get_global_addr(eswifi->iface, NET_ADDR_ANY_STATE);

		if (!addr) {
			break;
		}
		net_if_ipv4_addr_rm(eswifi->iface, addr);
	}

	while (true) {
		struct in_addr *addr = net_if_ipv4_get_ll(eswifi->iface, NET_ADDR_ANY_STATE);

		if (!addr) {
			break;
		}
		net_if_ipv4_addr_rm(eswifi->iface, addr);
	}
}

static K_WORK_DEFINE(eswifi_disconnect_work, eswifi_disconnect_work_cb);

static struct net_mgmt_event_callback eswifi_mgmt_cb_obj;

static void eswifi_mgmt_cb(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
			   struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
		struct k_work_delayable *dwork = &eswifi_keepalive_work;
		const struct wifi_status *status = (const struct wifi_status *)cb->info;

		if (status->status < 0) {
			// Connect error, retry
			LOG_WRN("Could not connect to WiFi, retrying...");
			dwork = &eswifi_reconnect_work;
		}

		k_work_schedule(dwork, K_SECONDS(CONFIG_ANJAY_CLIENT_NETWORK_KEEPALIVE_RATE));
	} else if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
		// eswifi driver doesn't clear IP addrs on disconnect, let's remove them manually
		// Nested network event handling is explicitly disabled, so let's do that via k_work
		k_work_submit(&eswifi_disconnect_work);
	}
}

int network_internal_platform_initialize(void)
{
	struct eswifi_dev *eswifi = eswifi_by_iface_idx(0);

	assert(eswifi);
	eswifi_lock(eswifi);
	// Set regulatory domain to "World Wide (passive Ch12-14)"; eS-WiFi defaults
	// to "US" which prevents connecting to networks that use channels 12-14.
	if (eswifi_at_cmd(eswifi, "CN=XV\r") < 0) {
		LOG_WRN("Failed to set Wi-Fi regulatory domain");
	}

	net_mgmt_init_event_callback(&eswifi_mgmt_cb_obj, eswifi_mgmt_cb,
				     NET_EVENT_WIFI_CONNECT_RESULT |
					     NET_EVENT_WIFI_DISCONNECT_RESULT);
	net_mgmt_add_event_callback(&eswifi_mgmt_cb_obj);

	eswifi_unlock(eswifi);

	return 0;
}

int network_connect_async(void)
{
	wifi_params = config_get_wifi_params();

	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, eswifi_by_iface_idx(0)->iface, &wifi_params,
			   sizeof(struct wifi_connect_req_params));

	if (ret > 0 || ret == -EALREADY || ret == -EINPROGRESS) {
		ret = 0;
	}
	if (ret) {
		LOG_ERR("Failed to configure Wi-Fi");
	}
	return ret;
}

void network_disconnect(void)
{
	struct k_work_sync sync;

	k_work_cancel_delayable_sync(&eswifi_reconnect_work, &sync);
	k_work_cancel_delayable_sync(&eswifi_keepalive_work, &sync);

	net_mgmt(NET_REQUEST_WIFI_DISCONNECT, eswifi_by_iface_idx(0)->iface, NULL, 0);
}
