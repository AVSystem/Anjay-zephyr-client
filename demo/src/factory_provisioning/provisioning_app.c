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

#include <logging/log.h>
#include <logging/log_ctrl.h>
#include <shell/shell_uart.h>

#include <anjay/anjay.h>
#include <anjay/access_control.h>
#include <anjay/factory_provisioning.h>
#include <anjay/security.h>
#include <anjay/server.h>

#include "../config.h"
#include "../persistence.h"
#include "factory_flash.h"

LOG_MODULE_REGISTER(provisioning_app);

static anjay_t *initialize_anjay(void)
{
	anjay_t *anjay = anjay_new(
		&(const anjay_configuration_t){ .endpoint_name = config_default_ep_name() });

	if (!anjay) {
		LOG_ERR("Could not create Anjay object");
		return NULL;
	}

	if (anjay_security_object_install(anjay) || anjay_server_object_install(anjay) ||
	    anjay_access_control_install(anjay)) {
		anjay_delete(anjay);
		LOG_ERR("Failed to install necessary modules");
		return NULL;
	}

	return anjay;
}

static void factory_provision(void)
{
	anjay_t *anjay = initialize_anjay();

	if (!anjay) {
		LOG_ERR("Couldn't initialize Anjay. Rebooting.");
		abort();
	}

	if (is_factory_provisioning_info_present()) {
		LOG_INF("Factory provisioning information already present. "
			"Please flash production firmware. Halting.");
	} else {
		z_shell_log_backend_disable(shell_backend_uart_get_ptr()->log_backend);

		avs_stream_t *stream = factory_flash_input_stream_create();

		assert(stream);

		avs_error_t err = anjay_factory_provision(anjay, stream);
		avs_error_t cleanup_err = avs_stream_cleanup(&stream);

		if (avs_is_ok(err)) {
			err = cleanup_err;
		}
		if (avs_is_ok(err) && persist_factory_provisioning_info(anjay)) {
			err = avs_errno(AVS_EIO);
		}
		factory_flash_finished(avs_is_ok(err) ? 0 : -1);

		z_shell_log_backend_enable(shell_backend_uart_get_ptr()->log_backend,
					   (struct shell *)(uintptr_t)shell_backend_uart_get_ptr(),
					   AVS_MIN(CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL,
						   CONFIG_LOG_MAX_LEVEL));

		if (avs_is_err(err)) {
			LOG_ERR("Could not perform factory provisioning. Rebooting.");
			abort();
		} else {
			LOG_INF("Factory provisioning finished. "
				"Please flash production firmware. Halting.");
		}
	}

	while (true) {
		k_sleep(K_SECONDS(1));
	}
}

void main(void)
{
	LOG_PANIC();

	if (persistence_init()) {
		LOG_ERR("Can't initialize persistence");
	}

	factory_provision();
}
