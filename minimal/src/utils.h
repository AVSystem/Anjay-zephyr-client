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

#include <stdint.h>

#ifdef CONFIG_NET_IPV6
#include <zephyr/net/sntp.h>
#endif // CONFIG_NET_IPV6

struct device_id {
	// 96 bits as hex + NULL-byte
	char value[25];
};

int get_device_id(struct device_id *out_id);
#ifdef CONFIG_NET_IPV6
int sntp_simple_ipv6(const char *server, uint32_t timeout, struct sntp_time *time);
#endif // CONFIG_NET_IPV6
