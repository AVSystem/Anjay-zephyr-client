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

#include <stddef.h>

#ifdef CONFIG_ANJAY_CLIENT_FOTA
#include <dfu/mcuboot.h>
#endif // CONFIG_ANJAY_CLIENT_FOTA

struct device_id {
	// 96 bits as hex + NULL-byte
	char value[25];
};

int get_device_id(struct device_id *out_id);

#ifdef CONFIG_ANJAY_CLIENT_FOTA
int get_fw_version_image_0(char *out_buf, size_t buf_size);
int get_fw_version_image_1(char *out_buf, size_t buf_size);
#endif // CONFIG_ANJAY_CLIENT_FOTA

#define SYNCHRONIZED(Mtx)                                                                          \
	for (int _synchronized_exit = k_mutex_lock(&(Mtx), K_FOREVER); !_synchronized_exit;        \
	     _synchronized_exit = -1, k_mutex_unlock(&(Mtx)))

#if defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
int tls_session_cache_purge(void);
#endif // defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
