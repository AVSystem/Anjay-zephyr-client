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

#include <version.h>

#include <zephyr/devicetree.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/sntp.h>

#ifdef CONFIG_POSIX_API
#include <zephyr/posix/time.h>
#else // CONFIG_POSIX_API
#include <date_time.h>
#endif // CONFIG_POSIX_API

#ifdef CONFIG_LTE_LINK_CONTROL
#include <modem/lte_lc.h>
#endif // CONFIG_LTE_LINK_CONTROL

#include <anjay/anjay.h>
#include <anjay/attr_storage.h>
#include <anjay/security.h>
#include <anjay/server.h>

#include <zephyr/net/net_conn_mgr.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_mgmt.h>

#include <avsystem/commons/avs_prng.h>

#include "led.h"

#include "objects/objects.h"

LOG_MODULE_REGISTER(zephyr_demo);

static const anjay_dm_object_def_t **device_obj;
static const anjay_dm_object_def_t **pattern_detector_obj;

static avs_sched_handle_t update_objects_handle;

#define ANJAY_THREAD_PRIO 1
#define ANJAY_THREAD_STACK_SIZE 8192

static struct k_thread anjay_thread;
K_THREAD_STACK_DEFINE(anjay_stack, ANJAY_THREAD_STACK_SIZE);

#ifdef CONFIG_POSIX_API
static void set_system_time(const struct sntp_time *time)
{
	struct timespec ts = { .tv_sec = time->seconds,
			       .tv_nsec = ((uint64_t)time->fraction * 1000000000) >> 32 };
	if (clock_settime(CLOCK_REALTIME, &ts)) {
		LOG_WRN("Failed to set time");
	}
}
#else // CONFIG_POSIX_API
static void set_system_time(const struct sntp_time *time)
{
	const struct tm *current_time = gmtime(&time->seconds);

	date_time_set(current_time);
	date_time_update_async(NULL);
}
#endif // CONFIG_POSIX_API

void synchronize_clock(void)
{
	struct sntp_time time;
	const uint32_t timeout_ms = 5000;

	if (sntp_simple(CONFIG_ANJAY_CLIENT_NTP_SERVER, timeout_ms, &time)) {
		LOG_WRN("Failed to get current time");
	} else {
		set_system_time(&time);
	}
}

static int register_objects(anjay_t *anjay)
{
	device_obj = device_object_create();
	if (!device_obj || anjay_register_object(anjay, device_obj)) {
		return -1;
	}

	pattern_detector_obj = pattern_detector_object_create();
	if (pattern_detector_obj) {
		anjay_register_object(anjay, pattern_detector_obj);
	}

	return 0;
}

static void update_objects(avs_sched_t *sched, const void *anjay_ptr)
{
	anjay_t *anjay = *(anjay_t *const *)anjay_ptr;

	device_object_update(anjay, device_obj);
	pattern_detector_object_update(anjay, pattern_detector_obj);

	AVS_SCHED_DELAYED(sched, &update_objects_handle,
			  avs_time_duration_from_scalar(1, AVS_TIME_S), update_objects, &anjay,
			  sizeof(anjay));
}

static void release_objects(void)
{
	device_object_release(device_obj);
	pattern_detector_object_release(pattern_detector_obj);
}

void initialize_network(void)
{
	LOG_INF("Initializing network connection...");
#ifdef CONFIG_LTE_LINK_CONTROL
	if (lte_lc_init_and_connect() < 0) {
		LOG_ERR("LTE link could not be established.");
		abort();
	}
#endif // CONFIG_LTE_LINK_CONTROL
	LOG_INF("Connected to network");
}

void run_anjay(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	const anjay_configuration_t config = { .endpoint_name = CONFIG_ANJAY_CLIENT_ENDPOINT_NAME,
					       .in_buffer_size = 4000,
					       .out_buffer_size = 4000 };

	anjay_t *anjay = anjay_new(&config);

	if (!anjay) {
		LOG_ERR("Could not create Anjay object");
		abort();
	}

	// Install attr_storage and necessary objects
	if (anjay_security_object_install(anjay) || anjay_server_object_install(anjay)) {
		LOG_ERR("Failed to install necessary modules");
		goto cleanup;
	}

	if (register_objects(anjay)) {
		LOG_ERR("Failed to initialize objects");
		goto cleanup;
	}

	const anjay_security_instance_t security_instance = {
		.ssid = 1,
		.bootstrap_server = false,
		.server_uri = CONFIG_ANJAY_CLIENT_SERVER_URI,
		.security_mode = ANJAY_SECURITY_PSK,
		.public_cert_or_psk_identity = CONFIG_ANJAY_CLIENT_PSK_IDENTITY,
		.public_cert_or_psk_identity_size =
			strlen(security_instance.public_cert_or_psk_identity),
		.private_cert_or_psk_key = CONFIG_ANJAY_CLIENT_PSK_KEY,
		.private_cert_or_psk_key_size = strlen(security_instance.private_cert_or_psk_key)
	};

	const anjay_server_instance_t server_instance = { .ssid = 1,
							  .lifetime = 50,
							  .default_min_period = -1,
							  .default_max_period = -1,
							  .disable_timeout = -1,
							  .binding = "U" };

	anjay_iid_t security_instance_id = ANJAY_ID_INVALID;

	if (anjay_security_object_add_instance(anjay, &security_instance, &security_instance_id)) {
		LOG_ERR("Failed to instantiate Security object");
		goto cleanup;
	}

	anjay_iid_t server_instance_id = ANJAY_ID_INVALID;

	if (anjay_server_object_add_instance(anjay, &server_instance, &server_instance_id)) {
		LOG_ERR("Failed to instantiate Server object");
		goto cleanup;
	}

	LOG_INF("Successfully created thread");

	update_objects(anjay_get_scheduler(anjay), &anjay);
	anjay_event_loop_run_with_error_handling(anjay,
						 avs_time_duration_from_scalar(1, AVS_TIME_S));

cleanup:
	avs_sched_del(&update_objects_handle);
	anjay_delete(anjay);
	release_objects();
}

void main(void)
{
	led_init();

	initialize_network();

	k_sleep(K_SECONDS(1));
	synchronize_clock();

	k_thread_create(&anjay_thread, anjay_stack, ANJAY_THREAD_STACK_SIZE, run_anjay, NULL, NULL,
			NULL, ANJAY_THREAD_PRIO, 0, K_NO_WAIT);
	k_thread_join(&anjay_thread, K_FOREVER);
	LOG_INF("Anjay stopped");
}
