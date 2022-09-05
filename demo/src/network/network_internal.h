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

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>

extern struct k_mutex network_internal_connect_mutex;
extern struct k_condvar network_internal_connect_condvar;

int network_internal_platform_initialize(void);

#if defined(CONFIG_NET_NATIVE_IPV4) || defined(CONFIG_NET_NATIVE_IPV6)
bool network_internal_has_ip_address(struct net_if *iface);
#endif // defined(CONFIG_NET_NATIVE_IPV4) || defined(CONFIG_NET_NATIVE_IPV6)

void network_internal_connection_state_changed(void);
