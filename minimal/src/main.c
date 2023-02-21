/*
 * Copyright 2020-2023 AVSystem <avsystem@avsystem.com>
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

#include <anjay_zephyr/config.h>
#include <anjay_zephyr/lwm2m.h>
#include <anjay_zephyr/objects.h>

void main(void)
{
	anjay_zephyr_lwm2m_init_from_settings();
	anjay_zephyr_lwm2m_start();
	/**
	 * Add additional threads creation here
	 */

	while (1) {
		k_sleep(K_SECONDS(1));
	}
}
