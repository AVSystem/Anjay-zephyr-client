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

#include <devicetree.h>
#include <kernel.h>
#include <stdlib.h>
#include <sys/printk.h>
#include <logging/log.h>
#include <logging/log_ctrl.h>
#include <version.h>

#include <net/dns_resolve.h>
#include <net/sntp.h>
#include <posix/time.h>

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
#include "objects/objects.h"

#if defined(CONFIG_WIFI)
#ifdef CONFIG_WIFI_ESWIFI
#include <wifi/eswifi/eswifi.h>
#endif // CONFIG_WIFI_ESWIFI
#ifdef CONFIG_WIFI_ESP32
#include <esp_wifi.h>
#include <esp_timer.h>
#include <esp_event.h>
#endif // CONFIG_WIFI_ESP32
#include <net/wifi.h>
#include <net/wifi_mgmt.h>
#elif defined(CONFIG_LTE_LINK_CONTROL)
#include <modem/lte_lc.h>
#endif /* defined(CONFIG_WIFI) || defined(CONFIG_LTE_LINK_CONTROL)
	*/

#ifdef CONFIG_DATE_TIME
#include <date_time.h>
#endif // CONFIG_DATE_TIME

#ifdef CONFIG_ANJAY_CLIENT_NRF_LC_INFO
#include "nrf_lc_info.h"
#endif // CONFIG_ANJAY_CLIENT_NRF_LC_INFO

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

#if defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
// The only parameters needed to address a credential stored in the modem
// are its type and its security tag - the type is defined already by
// the proper function being called, so the query contains only a single
// integer - the desired security tag.
static const char *PSK_QUERY = "1";
#endif // defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)

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

	if (sntp_simple(NTP_SERVER, timeout_ms, &time)) {
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

static K_MUTEX_DEFINE(net_connect_mutex);
static K_CONDVAR_DEFINE(net_connect_condvar);

#if defined(CONFIG_WIFI)
static volatile atomic_bool ip_addr_assigned;

static void ip_addr_add_cb(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
			   struct net_if *iface)
{
	(void)cb;
	(void)mgmt_event;
	(void)iface;

	atomic_store(&ip_addr_assigned, true);
	interrupt_net_connect_wait_loop();
}

static int wait_for_ip_addr_interruptible(void)
{
	// Note: this mirrors the logic of net_mgmt_event_wait_on_iface(), but there is
	// a nasty bug in that net_mgmt_del_event_callback() is never called by that function.
	struct net_mgmt_event_callback cb;
	int ret = -EAGAIN;

	memset(&cb, 0, sizeof(cb));
	net_mgmt_init_event_callback(&cb, ip_addr_add_cb, NET_EVENT_IPV4_ADDR_ADD);
	atomic_store(&ip_addr_assigned, false);
	net_mgmt_add_event_callback(&cb);

	SYNCHRONIZED(net_connect_mutex)
	{
		do {
			k_condvar_wait(&net_connect_condvar, &net_connect_mutex, K_FOREVER);
			if (atomic_load(&ip_addr_assigned)) {
				ret = 0;
			} else if (!atomic_load(&anjay_running)) {
				ret = -ETIMEDOUT;
			}
		} while (ret == -EAGAIN);
	}

	net_mgmt_del_event_callback(&cb);

	return ret;
}
#elif defined(CONFIG_LTE_LINK_CONTROL)
static void lte_connect_handler(const struct lte_lc_evt *const evt)
{
	if (evt && evt->type == LTE_LC_EVT_NW_REG_STATUS &&
	    (evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ||
	     evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_ROAMING)) {
		interrupt_net_connect_wait_loop();
	}
}

static int lte_interruptible_connect(void)
{
	int ret = -1;
	enum lte_lc_nw_reg_status status;

	SYNCHRONIZED(net_connect_mutex)
	{
		// Note: this is supposed to be handled by lte_lc_connect_async(),
		// but there is a nasty bug in in_progress flag handling
		ret = lte_lc_nw_reg_status_get(&status);
		if (!ret && status != LTE_LC_NW_REG_REGISTERED_HOME &&
		    status != LTE_LC_NW_REG_REGISTERED_ROAMING) {
			ret = lte_lc_connect_async(lte_connect_handler);
			while (!ret) {
				if (!atomic_load(&anjay_running)) {
					ret = -ETIMEDOUT;
				} else {
					ret = lte_lc_nw_reg_status_get(&status);
					if (!ret && (status == LTE_LC_NW_REG_REGISTERED_HOME ||
						     status == LTE_LC_NW_REG_REGISTERED_ROAMING)) {
						break;
					}
					k_condvar_wait(&net_connect_condvar, &net_connect_mutex,
						       K_FOREVER);
				}
			}
		}
	}

	return ret;
}
#endif // defined(CONFIG_WIFI) || defined(CONFIG_LTE_LINK_CONTROL)

void interrupt_net_connect_wait_loop(void)
{
	SYNCHRONIZED(net_connect_mutex)
	{
		k_condvar_broadcast(&net_connect_condvar);
	}
}

static int initialize_network(void)
{
	LOG_INF("Initializing network connection...");
#if defined(CONFIG_WIFI)
	struct net_if *iface = net_if_get_default();

#ifdef CONFIG_WIFI_ESWIFI
	struct eswifi_dev *eswifi = eswifi_by_iface_idx(0);

	assert(eswifi);
	// Set regulatory domain to "World Wide (passive Ch12-14)"; eS-WiFi defaults
	// to "US" which prevents connecting to networks that use channels 12-14.
	if (eswifi_at_cmd(eswifi, "CN=XV\r") < 0) {
		LOG_WRN("Failed to set Wi-Fi regulatory domain");
	}
#endif // CONFIG_WIFI_ESWIFI

#ifdef CONFIG_WIFI_ESP32
	net_dhcpv4_start(iface);

	AVS_STATIC_ASSERT(!IS_ENABLED(CONFIG_ESP32_WIFI_STA_AUTO),
			  esp32_wifi_auto_mode_incompatible_with_project);

	wifi_config_t wifi_config = { 0 };

	// use strncpy with the maximum length of sizeof(wifi_config.sta.{ssid|password}),
	// because ESP32 Wi-Fi buffers don't have to be null-terminated
	strncpy(wifi_config.sta.ssid, config_get_wifi_ssid(), sizeof(wifi_config.sta.ssid));
	strncpy(wifi_config.sta.password, config_get_wifi_password(),
		sizeof(wifi_config.sta.password));

	if (esp_wifi_set_mode(WIFI_MODE_STA) ||
	    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) || esp_wifi_connect()) {
		LOG_ERR("connection failed");
	}
#else // CONFIG_WIFI_ESP32
	struct wifi_connect_req_params wifi_params = { .ssid = (uint8_t *)config_get_wifi_ssid(),
						       .ssid_length =
							       strlen(config_get_wifi_ssid()),
						       .psk = (uint8_t *)config_get_wifi_password(),
						       .psk_length =
							       strlen(config_get_wifi_password()),
						       .security = WIFI_SECURITY_TYPE_PSK };

	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params,
			   sizeof(struct wifi_connect_req_params));

	if (ret && ret != -EALREADY && ret != -EINPROGRESS) {
		LOG_ERR("Failed to configure Wi-Fi");
		return -1;
	}
#endif // CONFIG_WIFI_ESP32

	if (wait_for_ip_addr_interruptible()) {
		LOG_ERR("Wi-Fi link could not be established.");
		net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);
		return -1;
	}
#elif defined(CONFIG_LTE_LINK_CONTROL)
	int ret = lte_lc_init();

	if (!ret) {
		ret = lte_interruptible_connect();
	}

	if (ret < 0 && ret != -EALREADY && ret != -EINPROGRESS) {
		LOG_ERR("LTE link could not be established.");
		return -1;
	}
#endif /* defined(CONFIG_WIFI) || defined(CONFIG_LTE_LINK_CONTROL)
	*/
	LOG_INF("Connected to network");
	return 0;
}

#ifndef CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
static int configure_servers_from_config(anjay_t *anjay, const anjay_configuration_t *config)
{
	const bool bootstrap = config_is_bootstrap();

#if defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
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
#endif // defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)

	const anjay_security_instance_t security_instance = {
		.ssid = 1,
		.bootstrap_server = bootstrap,
		.server_uri = config_get_server_uri(),
		.security_mode = ANJAY_SECURITY_PSK,
#if defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
		.psk_identity = avs_crypto_psk_identity_info_from_engine(PSK_QUERY),
		.psk_key = avs_crypto_psk_key_info_from_engine(PSK_QUERY),
#else // defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
		.public_cert_or_psk_identity = config->endpoint_name,
		.public_cert_or_psk_identity_size =
			strlen(security_instance.public_cert_or_psk_identity),
		.private_cert_or_psk_key = config_get_psk(),
		.private_cert_or_psk_key_size = strlen(security_instance.private_cert_or_psk_key)
#endif // defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
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
	if (initialize_network()) {
		LOG_ERR("Could not connect to the network");
		return NULL;
	}

#ifdef CONFIG_ANJAY_CLIENT_GPS
	initialize_gps();
#endif // CONFIG_ANJAY_CLIENT_GPS

	synchronize_clock();

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
#if defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
	if (avs_is_err(avs_crypto_psk_engine_key_rm(PSK_QUERY))) {
		LOG_WRN("Removing PSK key failed");
	}
	if (avs_is_err(avs_crypto_psk_engine_identity_rm(PSK_QUERY))) {
		LOG_WRN("Removing PSK identity failed");
	}
#endif // defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
	anjay_delete(anjay);
	release_objects();
	return NULL;
}

void run_anjay(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	anjay_t *anjay = initialize_anjay();

	if (anjay) {
		LOG_INF("Successfully created thread");

		SYNCHRONIZED(global_anjay_mutex)
		{
			global_anjay = anjay;
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

#if defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
		if (avs_is_err(avs_crypto_psk_engine_key_rm(PSK_QUERY))) {
			LOG_WRN("Removing PSK key failed");
		}
		if (avs_is_err(avs_crypto_psk_engine_identity_rm(PSK_QUERY))) {
			LOG_WRN("Removing PSK identity failed");
		}
#endif // defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)

#ifdef CONFIG_ANJAY_CLIENT_FOTA
		if (fw_update_requested()) {
			fw_update_reboot();
		}
#endif // CONFIG_ANJAY_CLIENT_FOTA
	}

	SYNCHRONIZED(anjay_thread_running_mutex)
	{
		anjay_thread_running = false;
		k_condvar_broadcast(&anjay_thread_running_condvar);
	}
}

void main(void)
{
#ifdef WITH_ANJAY_CLIENT_CONFIG
	config_init(shell_backend_uart_get_ptr());
#endif // WITH_ANJAY_CLIENT_CONFIG

#ifdef CONFIG_ANJAY_CLIENT_PERSISTENCE
	if (persistence_init()) {
		LOG_ERR("Can't initialize persistence");
	}
#endif // CONFIG_ANJAY_CLIENT_PERSISTENCE

	status_led_init();

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
