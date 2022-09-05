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
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/printk.h>

#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/sntp.h>
#include <zephyr/posix/time.h>

#include <anjay/anjay.h>
#include <anjay/access_control.h>
#include <anjay/factory_provisioning.h>
#include <anjay/security.h>
#include <anjay/server.h>

#include <avsystem/commons/avs_prng.h>
#include <avsystem/commons/avs_crypto_psk.h>

#include "common.h"
#include "config.h"
#include "default_config.h"
#include "firmware_update.h"
#include "gps.h"
#include "utils.h"
#include "persistence.h"
#include "status_led.h"

#include "network/network.h"
#include "objects/objects.h"

#ifdef CONFIG_DATE_TIME
#include <date_time.h>
#endif // CONFIG_DATE_TIME

#ifdef CONFIG_ANJAY_CLIENT_NRF_LC_INFO
#include "nrf_lc_info.h"
#endif // CONFIG_ANJAY_CLIENT_NRF_LC_INFO

#ifdef CONFIG_NET_L2_OPENTHREAD
#define RETRY_SYNC_CLOCK_DELAY_TIME_S 1
#endif // CONFIG_NET_L2_OPENTHREAD

LOG_MODULE_REGISTER(main_app);
static const anjay_dm_object_def_t **buzzer_obj;
static const anjay_dm_object_def_t **device_obj;
static const anjay_dm_object_def_t **led_color_light_obj;
static const anjay_dm_object_def_t **location_obj;
static const anjay_dm_object_def_t **switch_obj;

#ifdef CONFIG_ANJAY_CLIENT_NRF_LC_INFO
static const anjay_dm_object_def_t **conn_mon_obj;
static const anjay_dm_object_def_t **ecid_obj;
#endif // CONFIG_ANJAY_CLIENT_NRF_LC_INFO

#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES
static const anjay_dm_object_def_t **loc_assist_obj;
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES

static avs_sched_handle_t update_objects_handle;

volatile atomic_bool device_initialized;

anjay_t *volatile global_anjay;
K_MUTEX_DEFINE(global_anjay_mutex);
volatile atomic_bool anjay_running;

struct k_thread anjay_thread;
volatile bool anjay_thread_running;
K_MUTEX_DEFINE(anjay_thread_running_mutex);
K_CONDVAR_DEFINE(anjay_thread_running_condvar);
K_THREAD_STACK_DEFINE(anjay_stack, ANJAY_THREAD_STACK_SIZE);

#ifdef CONFIG_NET_L2_OPENTHREAD
static struct k_work_delayable sync_clock_work;
#endif // CONFIG_NET_L2_OPENTHREAD

static bool anjay_online;
static enum network_bearer_t anjay_last_known_bearer;

static K_SEM_DEFINE(synchronize_clock_sem, 0, 1);

#if defined(CONFIG_ANJAY_COMPAT_ZEPHYR_TLS) && defined(CONFIG_NRF_MODEM_LIB) &&                    \
	defined(CONFIG_MODEM_KEY_MGMT)
// The only parameters needed to address a credential stored in the modem
// are its type and its security tag - the type is defined already by
// the proper function being called, so the query contains only a single
// integer - the desired security tag.
static const char *PSK_QUERY = "1";
#endif /* defined(CONFIG_ANJAY_COMPAT_ZEPHYR_TLS) && defined(CONFIG_NRF_MODEM_LIB) &&
	* defined(CONFIG_MODEM_KEY_MGMT)
	*/

#ifdef CONFIG_DATE_TIME
static void set_system_time(const struct sntp_time *time)
{
	const struct tm *current_time = gmtime(&time->seconds);

	date_time_set(current_time);
	date_time_update_async(NULL);
}
#else // CONFIG_DATE_TIME
static void set_system_time(const struct sntp_time *time)
{
	struct timespec ts = { .tv_sec = time->seconds,
			       .tv_nsec = ((uint64_t)time->fraction * 1000000000) >> 32 };
	if (clock_settime(CLOCK_REALTIME, &ts)) {
		LOG_WRN("Failed to set time");
	}
}
#endif // CONFIG_DATE_TIME

void synchronize_clock(void)
{
	struct sntp_time time;
	const uint32_t timeout_ms = 5000;

	if (false
#if defined(CONFIG_NET_IPV6)
	    || !sntp_simple_ipv6(NTP_SERVER, timeout_ms, &time)
#endif
#if defined(CONFIG_NET_IPV4)
	    || !sntp_simple(NTP_SERVER, timeout_ms, &time)
#endif
	) {
		set_system_time(&time);
		k_sem_give(&synchronize_clock_sem);
	} else {
		LOG_WRN("Failed to get current time");
#ifdef CONFIG_NET_L2_OPENTHREAD
		k_work_schedule(&sync_clock_work, K_SECONDS(RETRY_SYNC_CLOCK_DELAY_TIME_S));
#endif // CONFIG_NET_L2_OPENTHREAD
	}
}

#ifdef CONFIG_NET_L2_OPENTHREAD
static void retry_synchronize_clock_work_handler(struct k_work *work)
{
	synchronize_clock();
}
#endif // CONFIG_NET_L2_OPENTHREAD

static int register_objects(anjay_t *anjay)
{
	device_obj = device_object_create();
	if (!device_obj || anjay_register_object(anjay, device_obj)) {
		return -1;
	}

	basic_sensors_install(anjay);
	three_axis_sensors_install(anjay);
	push_button_object_install(anjay);

	buzzer_obj = buzzer_object_create();
	if (buzzer_obj) {
		anjay_register_object(anjay, buzzer_obj);
	}

	led_color_light_obj = led_color_light_object_create();
	if (led_color_light_obj) {
		anjay_register_object(anjay, led_color_light_obj);
	}

	location_obj = location_object_create();
	if (location_obj) {
		anjay_register_object(anjay, location_obj);
	}

	switch_obj = switch_object_create();
	if (switch_obj) {
		anjay_register_object(anjay, switch_obj);
	}

#ifdef CONFIG_ANJAY_CLIENT_NRF_LC_INFO
	struct nrf_lc_info nrf_lc_info;

	nrf_lc_info_get(&nrf_lc_info);

	conn_mon_obj = conn_mon_object_create(&nrf_lc_info);
	if (conn_mon_obj) {
		anjay_register_object(anjay, conn_mon_obj);
	}

	ecid_obj = ecid_object_create(&nrf_lc_info);
	if (ecid_obj) {
		anjay_register_object(anjay, ecid_obj);
	}
#endif // CONFIG_ANJAY_CLIENT_NRF_LC_INFO

#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES
	loc_assist_obj = loc_assist_object_create();
	if (loc_assist_obj) {
		anjay_register_object(anjay, loc_assist_obj);
	}
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES

	return 0;
}

#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
void cell_request_job(avs_sched_t *sched, const void *cell_request_job_args_ptr)
{
	struct cell_request_job_args args =
		*(const struct cell_request_job_args *)cell_request_job_args_ptr;
	loc_assist_object_send_cell_request(args.anjay, loc_assist_obj, ecid_obj,
					    args.request_type);
}
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED

#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS
void agps_request_job(avs_sched_t *sched, const void *anjay_ptr)
{
	static const uint32_t full_mask =
		LOC_ASSIST_A_GPS_MASK_UTC | LOC_ASSIST_A_GPS_MASK_KLOBUCHAR |
		LOC_ASSIST_A_GPS_MASK_NEQUICK | LOC_ASSIST_A_GPS_MASK_TOW |
		LOC_ASSIST_A_GPS_MASK_CLOCK | LOC_ASSIST_A_GPS_MASK_LOCATION |
		LOC_ASSIST_A_GPS_MASK_INTEGRITY | LOC_ASSIST_A_GPS_MASK_EPHEMERIS |
		LOC_ASSIST_A_GPS_MASK_ALMANAC;
	LOG_INF("Manual request of A-GPS data");
	loc_assist_object_send_agps_request(*(anjay_t *const *)anjay_ptr, loc_assist_obj,
					    full_mask);
}
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS

static void update_objects_frequent(anjay_t *anjay)
{
	device_object_update(anjay, device_obj);
	switch_object_update(anjay, switch_obj);
	buzzer_object_update(anjay, buzzer_obj);
}

static void update_objects_periodic(anjay_t *anjay)
{
	basic_sensors_update(anjay);
	three_axis_sensors_update(anjay);
	location_object_update(anjay, location_obj);
}

#ifdef CONFIG_ANJAY_CLIENT_NRF_LC_INFO
static void update_objects_nrf_lc_info(anjay_t *anjay, const struct nrf_lc_info *nrf_lc_info)
{
	conn_mon_object_update(anjay, conn_mon_obj, nrf_lc_info);
	ecid_object_update(anjay, ecid_obj, nrf_lc_info);
}
#endif // CONFIG_ANJAY_CLIENT_NRF_LC_INFO

static void update_objects(avs_sched_t *sched, const void *anjay_ptr)
{
	anjay_t *anjay = *(anjay_t *const *)anjay_ptr;

	static size_t cycle;

	update_objects_frequent(anjay);
	if (cycle % 5 == 0) {
		update_objects_periodic(anjay);
	}

#ifdef CONFIG_ANJAY_CLIENT_NRF_LC_INFO
	struct nrf_lc_info nrf_lc_info;

	if (nrf_lc_info_get_if_changed(&nrf_lc_info)) {
		update_objects_nrf_lc_info(anjay, &nrf_lc_info);
	}
#endif // CONFIG_ANJAY_CLIENT_NRF_LC_INFO

#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS
	uint32_t request_mask = gps_fetch_modem_agps_request_mask();

	if (request_mask) {
		LOG_INF("Modem requests A-GPS data");
		loc_assist_object_send_agps_request(anjay, loc_assist_obj, request_mask);
	}
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS

#ifdef CONFIG_ANJAY_CLIENT_PERSISTENCE
	if (config_is_use_persistence() && persist_anjay_if_required(anjay)) {
		LOG_ERR("Couldn't persist Anjay's state!");
	}
#endif // CONFIG_ANJAY_CLIENT_PERSISTENCE

	status_led_toggle();

	cycle++;
	AVS_SCHED_DELAYED(sched, &update_objects_handle,
			  avs_time_duration_from_scalar(1, AVS_TIME_S), update_objects, &anjay,
			  sizeof(anjay));
}

static void release_objects(void)
{
	buzzer_object_release(&buzzer_obj);
	device_object_release(&device_obj);
	led_color_light_object_release(&led_color_light_obj);
	location_object_release(&location_obj);
	switch_object_release(&switch_obj);

#ifdef CONFIG_ANJAY_CLIENT_NRF_LC_INFO
	conn_mon_object_release(&conn_mon_obj);
	ecid_object_release(&ecid_obj);
#endif // CONFIG_ANJAY_CLIENT_NRF_LC_INFO

#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES
	loc_assist_object_release(&loc_assist_obj);
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES
}

#ifndef CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
static int configure_servers_from_config(anjay_t *anjay, const anjay_configuration_t *config)
{
	const bool bootstrap = config_is_bootstrap();

#if defined(CONFIG_ANJAY_COMPAT_ZEPHYR_TLS) && defined(CONFIG_NRF_MODEM_LIB) &&                    \
	defined(CONFIG_MODEM_KEY_MGMT)
	const char *psk_key = config_get_psk();
	avs_crypto_psk_key_info_t psk_key_info =
		avs_crypto_psk_key_info_from_buffer(psk_key, strlen(psk_key));
	if (avs_is_err(avs_crypto_psk_engine_key_store(PSK_QUERY, &psk_key_info))) {
		LOG_ERR("Storing PSK key failed");
		return -1;
	}
	avs_crypto_psk_identity_info_t identity_info = avs_crypto_psk_identity_info_from_buffer(
		config->endpoint_name, strlen(config->endpoint_name));
	if (avs_is_err(avs_crypto_psk_engine_identity_store(PSK_QUERY, &identity_info))) {
		LOG_ERR("Storing PSK identity failed");
		return -1;
	}
#endif /* defined(CONFIG_ANJAY_COMPAT_ZEPHYR_TLS) && defined(CONFIG_NRF_MODEM_LIB) &&
	* defined(CONFIG_MODEM_KEY_MGMT)
	*/

	const anjay_security_instance_t security_instance = {
		.ssid = 1,
		.bootstrap_server = bootstrap,
		.server_uri = config_get_server_uri(),
		.security_mode = ANJAY_SECURITY_PSK,
#if defined(CONFIG_ANJAY_COMPAT_ZEPHYR_TLS) && defined(CONFIG_NRF_MODEM_LIB) &&                    \
	defined(CONFIG_MODEM_KEY_MGMT)
		.psk_identity = avs_crypto_psk_identity_info_from_engine(PSK_QUERY),
		.psk_key = avs_crypto_psk_key_info_from_engine(PSK_QUERY),
#else /* defined(CONFIG_ANJAY_COMPAT_ZEPHYR_TLS) && defined(CONFIG_NRF_MODEM_LIB) &&
       * defined(CONFIG_MODEM_KEY_MGMT)
       */
		.public_cert_or_psk_identity = config->endpoint_name,
		.public_cert_or_psk_identity_size =
			strlen(security_instance.public_cert_or_psk_identity),
		.private_cert_or_psk_key = config_get_psk(),
		.private_cert_or_psk_key_size = strlen(security_instance.private_cert_or_psk_key)
#endif /* defined(CONFIG_ANJAY_COMPAT_ZEPHYR_TLS) && defined(CONFIG_NRF_MODEM_LIB) &&
	* defined(CONFIG_MODEM_KEY_MGMT)
	*/
	};

	const anjay_server_instance_t server_instance = { .ssid = 1,
							  .lifetime = config_get_lifetime(),
							  .default_min_period = -1,
							  .default_max_period = -1,
							  .disable_timeout = -1,
							  .binding = "U" };

	anjay_iid_t security_instance_id = ANJAY_ID_INVALID;

	if (anjay_security_object_add_instance(anjay, &security_instance, &security_instance_id)) {
		LOG_ERR("Failed to instantiate Security object");
		return -1;
	}

	if (!bootstrap) {
		anjay_iid_t server_instance_id = ANJAY_ID_INVALID;

		if (anjay_server_object_add_instance(anjay, &server_instance,
						     &server_instance_id)) {
			LOG_ERR("Failed to instantiate Server object");
			return -1;
		}
	}

	return 0;
}
#endif // CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING

static anjay_t *initialize_anjay(void)
{
	const anjay_configuration_t config = {
#ifdef CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
		.endpoint_name = config_default_ep_name(),
#else // CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
		.endpoint_name = config_get_endpoint_name(),
#endif // CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
		.in_buffer_size = 4000,
		.out_buffer_size = 4000,
		.udp_dtls_hs_tx_params =
			&(const avs_net_dtls_handshake_timeouts_t){
				// Change the default DTLS handshake parameters so that "anjay stop"
				// is more responsive; note that an exponential backoff is
				// implemented, so the maximum of 8 seconds adds up to up to 15
				// seconds in total.
				.min = { .seconds = 1, .nanoseconds = 0 },
				.max = { .seconds = 8, .nanoseconds = 0 } },
		.disable_legacy_server_initiated_bootstrap = true
	};

	anjay_t *anjay = anjay_new(&config);

	if (!anjay) {
		LOG_ERR("Could not create Anjay object");
		return NULL;
	}

	if (anjay_security_object_install(anjay) ||
	    anjay_server_object_install(anjay)
#ifdef CONFIG_ANJAY_CLIENT_PERSISTENCE
	    // Access Control object is necessary if Server Object with many servers is loaded
	    || anjay_access_control_install(anjay)
#endif // CONFIG_ANJAY_CLIENT_PERSISTENCE
	) {
		LOG_ERR("Failed to install necessary modules");
		goto error;
	}

#ifdef CONFIG_ANJAY_CLIENT_FOTA
	if (fw_update_install(anjay)) {
		LOG_ERR("Failed to initialize fw update module");
		goto error;
	}
#endif // CONFIG_ANJAY_CLIENT_FOTA

	if (register_objects(anjay)) {
		LOG_ERR("Failed to initialize objects");
		goto error;
	}
#ifdef CONFIG_ANJAY_CLIENT_PERSISTENCE
	if (config_is_use_persistence() && !restore_anjay_from_persistence(anjay)) {
		return anjay;
	}
#endif // CONFIG_ANJAY_CLIENT_PERSISTENCE

#ifdef CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
	if (!restore_anjay_from_factory_provisioning(anjay)) {
		return anjay;
	}
#else // CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
	if (!configure_servers_from_config(anjay, &config)) {
		return anjay;
	}
#endif // CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING

error:
#if defined(CONFIG_ANJAY_COMPAT_ZEPHYR_TLS) && defined(CONFIG_NRF_MODEM_LIB) &&                    \
	defined(CONFIG_MODEM_KEY_MGMT)
	if (avs_is_err(avs_crypto_psk_engine_key_rm(PSK_QUERY))) {
		LOG_WRN("Removing PSK key failed");
	}
	if (avs_is_err(avs_crypto_psk_engine_identity_rm(PSK_QUERY))) {
		LOG_WRN("Removing PSK identity failed");
	}
#endif /* defined(CONFIG_ANJAY_COMPAT_ZEPHYR_TLS) && defined(CONFIG_NRF_MODEM_LIB) &&
	* defined(CONFIG_MODEM_KEY_MGMT)
	*/
	anjay_delete(anjay);
	release_objects();
	return NULL;
}

static void update_anjay_network_bearer_unlocked(anjay_t *anjay, enum network_bearer_t bearer)
{
	if (anjay_online && !network_bearer_valid(bearer)) {
		LOG_INF("Anjay is now offline");
		if (!anjay_transport_enter_offline(anjay, ANJAY_TRANSPORT_SET_ALL)) {
			anjay_online = false;
		}
	} else if (network_bearer_valid(bearer) &&
		   (!anjay_online || anjay_last_known_bearer != bearer)) {
		LOG_INF("Anjay is now online on bearer %d", (int)bearer);
		if (anjay_last_known_bearer != bearer) {
			if (!anjay_transport_schedule_reconnect(anjay, ANJAY_TRANSPORT_SET_ALL)) {
				anjay_last_known_bearer = bearer;
				anjay_online = true;
			}
		} else if (!anjay_transport_exit_offline(anjay, ANJAY_TRANSPORT_SET_ALL)) {
			anjay_online = true;
		}
	}
}

static void update_anjay_network_bearer_job(avs_sched_t *sched, const void *dummy)
{
	ARG_UNUSED(dummy);

	SYNCHRONIZED(global_anjay_mutex)
	{
		if (global_anjay) {
			update_anjay_network_bearer_unlocked(global_anjay,
							     network_current_bearer());
		}
	}
}

void sched_update_anjay_network_bearer(void)
{
	static avs_sched_handle_t job_handle;

	SYNCHRONIZED(global_anjay_mutex)
	{
		if (global_anjay) {
			AVS_SCHED_NOW(anjay_get_scheduler(global_anjay), &job_handle,
				      update_anjay_network_bearer_job, NULL, 0);
		}
	}
}

void run_anjay(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	LOG_INF("Connecting to the network...");

	if (network_connect_async()) {
		LOG_ERR("Could not initiate connection");
		goto finish;
	}

	if (network_wait_for_connected_interruptible()) {
		LOG_ERR("Could not connect to the network");
		goto disconnect;
	}

	LOG_INF("Connected to network");

	k_sem_reset(&synchronize_clock_sem);
	synchronize_clock();
	if (k_sem_take(&synchronize_clock_sem, K_SECONDS(30))) {
		LOG_WRN("Could not synchronize system clock within timeout, "
			"continuing without real time...");
	}

	anjay_t *anjay = initialize_anjay();

	if (!anjay) {
		goto disconnect;
	}

	LOG_INF("Successfully created thread");

	SYNCHRONIZED(global_anjay_mutex)
	{
		global_anjay = anjay;

		anjay_last_known_bearer = (enum network_bearer_t)0;
		update_anjay_network_bearer_unlocked(anjay, network_current_bearer());
	}

	// anjay stop could be called immediately after anjay start
	if (atomic_load(&anjay_running)) {
		update_objects(anjay_get_scheduler(anjay), &anjay);
		anjay_event_loop_run_with_error_handling(
			anjay, avs_time_duration_from_scalar(1, AVS_TIME_S));
	}

	avs_sched_del(&update_objects_handle);
#ifdef CONFIG_ANJAY_CLIENT_PERSISTENCE
	if (config_is_use_persistence() && persist_anjay_if_required(anjay)) {
		LOG_ERR("Couldn't persist Anjay's state!");
	}
#endif // CONFIG_ANJAY_CLIENT_PERSISTENCE

	SYNCHRONIZED(global_anjay_mutex)
	{
		global_anjay = NULL;
	}
	anjay_delete(anjay);
	release_objects();

#if defined(CONFIG_ANJAY_COMPAT_ZEPHYR_TLS) && defined(CONFIG_NRF_MODEM_LIB) &&                    \
	defined(CONFIG_MODEM_KEY_MGMT)
	if (avs_is_err(avs_crypto_psk_engine_key_rm(PSK_QUERY))) {
		LOG_WRN("Removing PSK key failed");
	}
	if (avs_is_err(avs_crypto_psk_engine_identity_rm(PSK_QUERY))) {
		LOG_WRN("Removing PSK identity failed");
	}
#endif /* defined(CONFIG_ANJAY_COMPAT_ZEPHYR_TLS) && defined(CONFIG_NRF_MODEM_LIB) &&
	* defined(CONFIG_MODEM_KEY_MGMT)
	*/

#ifdef CONFIG_ANJAY_CLIENT_FOTA
	if (fw_update_requested()) {
		fw_update_reboot();
	}
#endif // CONFIG_ANJAY_CLIENT_FOTA

disconnect:
	network_disconnect();

finish:
	SYNCHRONIZED(anjay_thread_running_mutex)
	{
		anjay_thread_running = false;
		k_condvar_broadcast(&anjay_thread_running_condvar);
	}
}

void main(void)
{
	LOG_INF("Initializing Anjay-zephyr-client demo " CLIENT_VERSION);

#ifdef WITH_ANJAY_CLIENT_CONFIG
	config_init(shell_backend_uart_get_ptr());
#endif // WITH_ANJAY_CLIENT_CONFIG

#ifdef CONFIG_ANJAY_CLIENT_PERSISTENCE
	if (persistence_init()) {
		LOG_ERR("Can't initialize persistence");
	}
#endif // CONFIG_ANJAY_CLIENT_PERSISTENCE

	status_led_init();

#ifdef CONFIG_NET_L2_OPENTHREAD
	k_work_init_delayable(&sync_clock_work, retry_synchronize_clock_work_handler);
#endif // CONFIG_NET_L2_OPENTHREAD

	if (network_initialize()) {
		LOG_ERR("Cannot initialize the network");
		LOG_PANIC();
		abort();
	}

#ifdef CONFIG_ANJAY_CLIENT_GPS
	initialize_gps();
#endif // CONFIG_ANJAY_CLIENT_GPS

#ifdef CONFIG_ANJAY_CLIENT_FOTA
	fw_update_apply();
#endif // CONFIG_ANJAY_CLIENT_FOTA

#ifdef CONFIG_ANJAY_CLIENT_NRF_LC_INFO
	if (initialize_nrf_lc_info_listener()) {
		LOG_ERR("Can't initialize Link Control info listener");
		LOG_PANIC();
		abort();
	}
#endif // CONFIG_ANJAY_CLIENT_NRF_LC_INFO

	atomic_store(&device_initialized, true);
	atomic_store(&anjay_running, true);

	while (true) {
		if (atomic_load(&anjay_running)) {
			SYNCHRONIZED(anjay_thread_running_mutex)
			{
				k_thread_create(&anjay_thread, anjay_stack, ANJAY_THREAD_STACK_SIZE,
						run_anjay, NULL, NULL, NULL, ANJAY_THREAD_PRIO, 0,
						K_NO_WAIT);
				anjay_thread_running = true;

				while (anjay_thread_running) {
					k_condvar_wait(&anjay_thread_running_condvar,
						       &anjay_thread_running_mutex, K_FOREVER);
				}

				k_thread_join(&anjay_thread, K_FOREVER);
			}

			shell_print(shell_backend_uart_get_ptr(), "Anjay stopped");
		} else {
			k_sleep(K_SECONDS(1));
		}
	}
}
