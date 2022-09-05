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

#include "utils.h"

#include <string.h>

#include <zephyr/drivers/hwinfo.h>

#include <avsystem/commons/avs_utils.h>

#ifdef CONFIG_NET_IPV6
#include <zephyr/net/socketutils.h>
#endif // CONFIG_NET_IPV6

int get_device_id(struct device_id *out_id)
{
	memset(out_id->value, 0, sizeof(out_id->value));

	uint8_t id[12];
	ssize_t retval = hwinfo_get_device_id(id, sizeof(id));

	if (retval <= 0) {
		return -1;
	}

	return avs_hexlify(out_id->value, sizeof(out_id->value), NULL, id, (size_t)retval);
}

/*
 * Copyright (c) 2019 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * This sntp_simple_ipv6 function is a copy of sntp_simple function from Zephyr
 * (https://github.com/zephyrproject-rtos/zephyr/blob/zephyr-v3.0.0/subsys/net/lib/sntp/sntp_simple.c)
 * repository. The only change is the desired address family.
 */

#ifdef CONFIG_NET_IPV6
int sntp_simple_ipv6(const char *server, uint32_t timeout, struct sntp_time *time)
{
	int res;
	static struct addrinfo hints;
	struct addrinfo *addr;
	struct sntp_ctx sntp_ctx;
	uint64_t deadline;
	uint32_t iter_timeout;

	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = 0;
	/* 123 is the standard SNTP port per RFC4330 */
	res = net_getaddrinfo_addr_str(server, "123", &hints, &addr);

	if (res < 0) {
		/* Just in case, as namespace for getaddrinfo errors is
		 * different from errno errors.
		 */
		errno = EDOM;
		return res;
	}

	res = sntp_init(&sntp_ctx, addr->ai_addr, addr->ai_addrlen);
	if (res < 0) {
		goto freeaddr;
	}

	if (timeout == SYS_FOREVER_MS) {
		deadline = (uint64_t)timeout;
	} else {
		deadline = k_uptime_get() + (uint64_t)timeout;
	}

	/* Timeout for current iteration */
	iter_timeout = 100;

	while (k_uptime_get() < deadline) {
		res = sntp_query(&sntp_ctx, iter_timeout, time);

		if (res != -ETIMEDOUT) {
			break;
		}

		/* Exponential backoff with limit */
		if (iter_timeout < 1000) {
			iter_timeout *= 2;
		}
	}

	sntp_close(&sntp_ctx);

freeaddr:
	freeaddrinfo(addr);

	return res;
}
#endif // CONFIG_NET_IPV6
