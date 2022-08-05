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

#include "common.h"
#include "config.h"
#include "utils.h"
#include "persistence.h"

static int cmd_anjay_start(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!atomic_load(&device_initialized)) {
		shell_warn(shell, "Cannot start Anjay - device initialization is ongoing "
				  "(perhaps it hasn't connected to network yet)");
		return 0;
	}

	if (!atomic_load(&anjay_running)) {
#ifdef WITH_ANJAY_CLIENT_CONFIG
		shell_print(shell, "Saving config");
		config_save(shell_backend_uart_get_ptr());
#endif // WITH_ANJAY_CLIENT_CONFIG
		shell_print(shell, "Starting Anjay");
		atomic_store(&anjay_running, true);
	} else {
		shell_warn(shell, "Cannot start Anjay - already running");
	}

	return 0;
}

static void interrupt_anjay(avs_sched_t *sched, const void *anjay_ptr)
{
	anjay_event_loop_interrupt(*(anjay_t *const *)anjay_ptr);
}

static int cmd_anjay_stop(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (atomic_load(&anjay_running)) {
		shell_print(shell, "Shutting down Anjay...");

		// change the flag first to interrupt the thread if event loop is not
		// running yet
		atomic_store(&anjay_running, false);
		interrupt_net_connect_wait_loop();

		SYNCHRONIZED(global_anjay_mutex)
		{
			if (global_anjay) {
				// hack to make sure that anjay_event_loop_interrupt() is called
				// only when the event loop is running
				anjay_t *anjay = global_anjay;

				AVS_SCHED_NOW(anjay_get_scheduler(anjay), NULL, interrupt_anjay,
					      &anjay, sizeof(anjay));
			}
		}

		SYNCHRONIZED(anjay_thread_running_mutex)
		{
			if (anjay_thread_running) {
				shell_print(shell, "Waiting for Anjay to stop...");
				shell_print(shell,
					    "If a DTLS handshake is in progress, it might take "
					    "up to 15 s for it to time out");
			}
			while (anjay_thread_running) {
				k_condvar_wait(&anjay_thread_running_condvar,
					       &anjay_thread_running_mutex, K_FOREVER);
			}
		}
	} else {
		shell_warn(shell, "Anjay is not running");
	}

	return 0;
}

#ifdef WITH_ANJAY_CLIENT_CONFIG
static int cmd_anjay_config_set(const struct shell *shell, size_t argc, char **argv)
{
	if (atomic_load(&anjay_running)) {
		shell_print(shell, "Cannot change the config while Anjay is running");
		return -1;
	}

	return config_set_option(shell, argc, argv);
}

static int cmd_anjay_config_default(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (atomic_load(&anjay_running)) {
		shell_print(shell, "Cannot change the config while Anjay is running");
		return -1;
	}

	config_default_init();
	return 0;
}

static int cmd_anjay_config_show(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	config_print_summary(shell);

	return 0;
}

static int cmd_anjay_config_save(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "Saving config");
	config_save(shell_backend_uart_get_ptr());
	config_print_summary(shell);

	return 0;
}
#endif // WITH_ANJAY_CLIENT_CONFIG

#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
static int cmd_anjay_nls_cell_request(const struct shell *shell, size_t argc, char **argv,
				      void *data)
{
	SYNCHRONIZED(global_anjay_mutex)
	{
		if (global_anjay) {
			struct cell_request_job_args args = {
				.anjay = global_anjay,
				.request_type = (enum loc_assist_cell_request_type)(uintptr_t)data
			};
			AVS_SCHED_NOW(anjay_get_scheduler(global_anjay), NULL, cell_request_job,
				      &args, sizeof(args));
		} else {
			shell_warn(shell, "Anjay is not running");
		}
	}
	return 0;
}
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED

#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS
static int cmd_anjay_nls_agps_request(const struct shell *shell, size_t argc, char **argv,
				      void *data)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	SYNCHRONIZED(global_anjay_mutex)
	{
		if (global_anjay) {
			anjay_t *anjay = global_anjay;

			AVS_SCHED_NOW(anjay_get_scheduler(anjay), NULL, agps_request_job, &anjay,
				      sizeof(anjay));
		} else {
			shell_warn(shell, "Anjay is not running");
		}
	}
	return 0;
}
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS

#ifdef CONFIG_ANJAY_CLIENT_PERSISTENCE
static int cmd_anjay_persistence_purge(const struct shell *shell, size_t argc, char **argv,
				       void *data)
{
	int err = 0;

	SYNCHRONIZED(global_anjay_mutex)
	{
		if (atomic_load(&anjay_running) || global_anjay) {
			shell_warn(shell, "Cannot purge persistence while Anjay is running");
		} else {
			err = persistence_purge();
			if (err) {
				shell_warn(shell, "Could not purge persistence");
			} else {
				shell_print(shell, "Successfully purged persistence");
			}
		}
	}
	return err;
}
#endif // CONFIG_ANJAY_CLIENT_PERSISTENCE

#if defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
static int cmd_anjay_session_cache_purge(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	int err = tls_session_cache_purge();

	if (err) {
		shell_warn(shell, "Could not purge the TLS session cache");
	} else {
		shell_print(shell, "Successfully purged the TLS session cache");
	}

	return err;
}
#endif // defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)

#ifdef WITH_ANJAY_CLIENT_CONFIG
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_anjay_config_set,
#ifndef CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
	SHELL_CMD(OPTION_KEY_EP_NAME, NULL, "Endpoint name", cmd_anjay_config_set),
#endif // CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
#ifdef CONFIG_WIFI
	SHELL_CMD(OPTION_KEY_SSID, NULL, "Wi-Fi SSID", cmd_anjay_config_set),
	SHELL_CMD(OPTION_KEY_PASSWORD, NULL, "Wi-Fi password", cmd_anjay_config_set),
#endif // CONFIG_WIFI
#ifndef CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
	SHELL_CMD(OPTION_KEY_URI, NULL, "Server URI", cmd_anjay_config_set),
	SHELL_CMD(OPTION_KEY_LIFETIME, NULL, "Device lifetime", cmd_anjay_config_set),
	SHELL_CMD(OPTION_KEY_PSK, NULL, "PSK", cmd_anjay_config_set),
	SHELL_CMD(OPTION_KEY_BOOTSTRAP, NULL, "Perform bootstrap", cmd_anjay_config_set),
#endif // CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF
	SHELL_CMD(OPTION_KEY_GPS_NRF_PRIO_MODE_TIMEOUT, NULL,
		  "GPS priority mode timeout - determines (in seconds) for how "
		  "long the modem can run with LTE disabled, in case of "
		  "trouble with producing a GPS fix. Set to 0 to disable GPS "
		  "priority mode at all.",
		  cmd_anjay_config_set),
	SHELL_CMD(OPTION_KEY_GPS_NRF_PRIO_MODE_COOLDOWN, NULL,
		  "GPS priority mode cooldown - determines (in seconds) how "
		  "much time must pass after a failed try to produce a GPS fix "
		  "to enable GPS priority mode again.",
		  cmd_anjay_config_set),
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF
#if defined(CONFIG_ANJAY_CLIENT_PERSISTENCE) && !defined(CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING)
	SHELL_CMD(
		OPTION_KEY_USE_PERSISTENCE, NULL,
		"Enables persistence of Access Control Object, Attribute Storage, Security Object and Server Object.",
		cmd_anjay_config_set),
#endif /* defined(CONFIG_ANJAY_CLIENT_PERSISTENCE) &&
	* !defined(CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING)
	*/
	SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_anjay_config,
			       SHELL_CMD(default, NULL, "Restore the default config",
					 cmd_anjay_config_default),
			       SHELL_CMD(save, NULL, "Save Anjay config", cmd_anjay_config_save),
			       SHELL_CMD(set, &sub_anjay_config_set, "Change Anjay config", NULL),
			       SHELL_CMD(show, NULL, "Show Anjay config", cmd_anjay_config_show),
			       SHELL_SUBCMD_SET_END);
#endif // WITH_ANJAY_CLIENT_CONFIG

#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
SHELL_SUBCMD_DICT_SET_CREATE(
	sub_anjay_nls_cell_request, cmd_anjay_nls_cell_request,
	(inform_single, (void *)(uintptr_t)LOC_ASSIST_CELL_REQUEST_INFORM_SINGLE),
	(inform_multi, (void *)(uintptr_t)LOC_ASSIST_CELL_REQUEST_INFORM_MULTI),
	(request_single, (void *)(uintptr_t)LOC_ASSIST_CELL_REQUEST_REQUEST_SINGLE),
	(request_multi, (void *)(uintptr_t)LOC_ASSIST_CELL_REQUEST_REQUEST_MULTI));
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_anjay, SHELL_CMD(start, NULL, "Save config and start Anjay", cmd_anjay_start),
	SHELL_CMD(stop, NULL, "Stop Anjay", cmd_anjay_stop),
#ifdef WITH_ANJAY_CLIENT_CONFIG
	SHELL_CMD(config, &sub_anjay_config, "Configure Anjay params", NULL),
#endif // WITH_ANJAY_CLIENT_CONFIG
#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
	SHELL_CMD(nls_cell_request, &sub_anjay_nls_cell_request,
		  "Make a cell-based location request to Nordic Location Services", NULL),
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS
	SHELL_CMD(nls_agps_request, NULL, "Make a manual A-GPS request to Nordic Location Services",
		  cmd_anjay_nls_agps_request),
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS
#ifdef CONFIG_ANJAY_CLIENT_PERSISTENCE
	SHELL_CMD(persistence_purge, NULL, "Purges persisted Anjay state",
		  cmd_anjay_persistence_purge),
#endif // CONFIG_ANJAY_CLIENT_PERSISTENCE
#if defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
	SHELL_CMD(session_cache_purge, NULL, "Remove the TLS session data cached in the nRF modem",
		  cmd_anjay_session_cache_purge),
#endif // defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(anjay, &sub_anjay, "Anjay commands", NULL);
