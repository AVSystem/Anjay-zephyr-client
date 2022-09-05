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

#include <stdlib.h>

#include <zephyr/logging/log.h>
#include <zephyr/net/openthread.h>

#include <openthread/thread.h>

#include "network.h"
#include "network_internal.h"

#include "../utils.h"
#include "../default_config.h"

/*
 * Due to the lack of a function declaration in the openthread.h
 * file, it has been added here.
 */
int openthread_stop(struct openthread_context *ot_context);

LOG_MODULE_REGISTER(network_openthread);

static void ot_state_changed(uint32_t flags, void *context)
{
	network_internal_connection_state_changed();
}

int network_internal_platform_initialize(void)
{
	openthread_set_state_changed_cb(ot_state_changed);
	return 0;
}

int network_connect_async(void)
{
	int ret = openthread_start(openthread_get_default_context());

	if (ret) {
		LOG_WRN("Failed to start Openthread.");
	}

	return ret;
}

enum network_bearer_t network_current_bearer(void)
{
	struct openthread_context *ctx = openthread_get_default_context();

	bool connected =
		otThreadGetDeviceRole(ctx->instance) >= OT_DEVICE_ROLE_CHILD &&
		net_if_ipv6_get_global_addr(NET_ADDR_PREFERRED, &(struct net_if *){ ctx->iface });

	return connected ? NETWORK_BEARER_OPENTHREAD : NETWORK_BEARER_LIMIT;
}

void network_disconnect(void)
{
	openthread_stop(openthread_get_default_context());
}
