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
#include <anjay/fw_update.h>

#include <stdbool.h>

#include <dfu/flash_img.h>
#include <dfu/mcuboot.h>
#include <logging/log.h>
#include <logging/log_ctrl.h>
#include <sys/reboot.h>

#include "firmware_update.h"
#include "utils.h"

LOG_MODULE_REGISTER(fw_update);

static bool just_updated;
static bool update_requested;

static struct flash_img_context img_ctx;
static bool img_ctx_initialized;

static int fw_stream_open(void *user_ptr, const char *package_uri,
			  const struct anjay_etag *package_etag)
{
	(void)user_ptr;
	(void)package_uri;
	(void)package_etag;

	assert(!img_ctx_initialized);

	memset(&img_ctx, 0, sizeof(img_ctx));
	if (flash_img_init(&img_ctx)) {
		return -1;
	}

	img_ctx_initialized = true;
	return 0;
}

static int fw_stream_write(void *user_ptr, const void *data, size_t length)
{
	(void)user_ptr;

	assert(img_ctx_initialized);

	if (flash_img_buffered_write(&img_ctx, data, length, false)) {
		return -1;
	}
	return 0;
}

static int fw_stream_finish(void *user_ptr)
{
	(void)user_ptr;

	assert(img_ctx_initialized);

	if (flash_img_buffered_write(&img_ctx, NULL, 0, true)) {
		return -1;
	}

	img_ctx_initialized = false;
	return 0;
}

static void fw_reset(void *user_ptr)
{
	(void)user_ptr;

	img_ctx_initialized = false;
}

static const char *fw_get_version(void *user_ptr)
{
	(void)user_ptr;

	static char fw_version[BOOT_IMG_VER_STRLEN_MAX];

	if (get_fw_version_image_1(fw_version, sizeof(fw_version))) {
		return NULL;
	}

	return fw_version;
}

static int fw_perform_upgrade(void *anjay_)
{
	anjay_t *anjay = (anjay_t *)anjay_;

	if (boot_request_upgrade(BOOT_UPGRADE_TEST)) {
		return -1;
	}

	anjay_event_loop_interrupt(anjay);
	update_requested = true;
	return 0;
}

static const anjay_fw_update_handlers_t handlers = { .stream_open = fw_stream_open,
						     .stream_write = fw_stream_write,
						     .stream_finish = fw_stream_finish,
						     .reset = fw_reset,
						     .get_version = fw_get_version,
						     .perform_upgrade = fw_perform_upgrade };

int fw_update_install(anjay_t *anjay)
{
	anjay_fw_update_initial_state_t state = { 0 };

	if (just_updated) {
		state.result = ANJAY_FW_UPDATE_INITIAL_SUCCESS;
	}

	return anjay_fw_update_install(anjay, &handlers, anjay, &state);
}

void fw_update_apply(void)
{
	// Image may be unconfirmed, because:
	// - we've just did a FOTA of the device and new
	//   firmware is being run
	// - the firmware was flashed using external programmer
	//
	// In both cases we want to mark the image as
	// confirmed (to either accept the new firmware,
	// or put MCUBoot in consistent state after flashing),
	// but only in the former case we should notify the
	// server that we've successfully updated the firmware.
	//
	// We can differentiate these two situations by taking
	// the retval of boot_write_img_confirmed().
	just_updated = !boot_is_img_confirmed() && !boot_write_img_confirmed();
	if (just_updated) {
		LOG_INF("Successfully updated firmware");
	}
}

bool fw_update_requested(void)
{
	return update_requested;
}

void fw_update_reboot(void)
{
	LOG_INF("Rebooting to perform a firmware upgrade...");
	LOG_PANIC();
	sys_reboot(SYS_REBOOT_WARM);
}
