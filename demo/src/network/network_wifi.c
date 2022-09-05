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

#include "network.h"
#include "network_internal.h"

#include "../config.h"
#include "../utils.h"

LOG_MODULE_REGISTER(network_wifi);

int network_internal_platform_initialize(void)
{
	return 0;
}

int network_connect_async(void)
{
	struct wifi_connect_req_params wifi_params = config_get_wifi_params();

	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, net_if_get_default(), &wifi_params,
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
	net_mgmt(NET_REQUEST_WIFI_DISCONNECT, net_if_get_default(), NULL, 0);
}
