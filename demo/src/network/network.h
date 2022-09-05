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

#pragma once

#include <stdbool.h>

#include <avsystem/commons/avs_defs.h>

enum network_bearer_t {
#ifdef CONFIG_WIFI
	NETWORK_BEARER_WIFI,
#endif // CONFIG_WIFI
#if defined(CONFIG_MODEM) || defined(CONFIG_LTE_LINK_CONTROL)
	NETWORK_BEARER_CELLULAR,
#endif // defined(CONFIG_MODEM) || defined(CONFIG_LTE_LINK_CONTROL)
#ifdef CONFIG_NET_L2_OPENTHREAD
	NETWORK_BEARER_OPENTHREAD,
#endif // CONFIG_NET_L2_OPENTHREAD
	NETWORK_BEARER_LIMIT
};

AVS_STATIC_ASSERT(NETWORK_BEARER_LIMIT > 0, no_network_bearers_available);

static inline bool network_bearer_valid(enum network_bearer_t bearer)
{
	return bearer >= (enum network_bearer_t)0 && bearer < NETWORK_BEARER_LIMIT;
}

void network_interrupt_connect_wait_loop(void);
int network_initialize(void);
int network_connect_async(void);
enum network_bearer_t network_current_bearer(void);
int network_wait_for_connected_interruptible(void);
void network_disconnect(void);

static inline bool network_is_connected(void)
{
	return network_bearer_valid(network_current_bearer());
}
