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
#include <avsystem/commons/avs_utils.h>
#include <drivers/hwinfo.h>
#include <stdint.h>
#include <string.h>

#ifdef CONFIG_ANJAY_CLIENT_FOTA
#include <dfu/mcuboot.h>
#include <stdio.h>
#include <storage/flash_map.h>
#endif // CONFIG_ANJAY_CLIENT_FOTA

#if defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
#include <modem/modem_info.h>
#include <nrf_socket.h>
#endif // defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)

int get_device_id(struct device_id *out_id)
{
	memset(out_id->value, 0, sizeof(out_id->value));

#ifdef CONFIG_NRF_MODEM_LIB
	return (modem_info_init() ||
		modem_info_string_get(MODEM_INFO_IMEI, out_id->value, sizeof(out_id->value)) < 0) ?
		       -1 :
		       0;
#else // CONFIG_NRF_MODEM_LIB
	uint8_t id[12];
	ssize_t retval = hwinfo_get_device_id(id, sizeof(id));

	if (retval <= 0) {
		return -1;
	}

	return avs_hexlify(out_id->value, sizeof(out_id->value), NULL, id, (size_t)retval);
#endif // CONFIG_NRF_MODEM_LIB
}

#ifdef CONFIG_ANJAY_CLIENT_FOTA
static int get_fw_version(char *out_buf, size_t buf_size, uint8_t area_id)
{
	// apparently BOOT_IMG_VER_STRLEN_MAX accounts for the nullchar too
	if (buf_size < BOOT_IMG_VER_STRLEN_MAX) {
		return -1;
	}

	struct mcuboot_img_header header;

	if (boot_read_bank_header(area_id, &header, sizeof(header))) {
		return -1;
	}

	assert(header.mcuboot_version == 1);

	struct mcuboot_img_sem_ver *ver = &header.h.v1.sem_ver;

	// workaround: 8-bit (hhu/hhd) format specifiers are not supported,
	// (see explanation in inttypes.h), so we can't pass major/minor values
	// directly
	uint16_t major = ver->major;
	uint16_t minor = ver->minor;
	static const char *const fmt = "%" PRIu16 ".%" PRIu16 ".%" PRIu16 "+%" PRIu32;

	if (snprintf(out_buf, BOOT_IMG_VER_STRLEN_MAX, fmt, major, minor, ver->revision,
		     ver->build_num) < 0) {
		return -1;
	}
	return 0;
}

int get_fw_version_image_0(char *out_buf, size_t buf_size)
{
	return get_fw_version(out_buf, buf_size, FLASH_AREA_ID(image_0));
}

int get_fw_version_image_1(char *out_buf, size_t buf_size)
{
	return get_fw_version(out_buf, buf_size, FLASH_AREA_ID(image_1));
}
#endif // CONFIG_ANJAY_CLIENT_FOTA

#if defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
int tls_session_cache_purge(void)
{
	int dummy_socket_fd = nrf_socket(NRF_AF_INET, NRF_SOCK_STREAM, NRF_SPROTO_TLS1v2);

	if (dummy_socket_fd == -1) {
		return -1;
	}

	int dummy_integer = 42;
	int result = nrf_setsockopt(dummy_socket_fd, NRF_SOL_SECURE, NRF_SO_SEC_SESSION_CACHE_PURGE,
				    &dummy_integer, sizeof(dummy_integer));
	int close_result = nrf_close(dummy_socket_fd);

	if (result) {
		return result;
	} else {
		return close_result;
	}
}
#endif // defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
