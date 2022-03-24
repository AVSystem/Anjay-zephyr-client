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

static int
cmd_anjay_start(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (!atomic_load(&DEVICE_INITIALIZED)) {
        shell_warn(shell,
                   "Cannot start Anjay - device initialization is ongoing "
                   "(perhaps it hasn't connected to network yet)");
        return 0;
    }

    if (!atomic_load(&ANJAY_RUNNING)) {
        shell_print(shell, "Saving config");
        config_save(shell_backend_uart_get_ptr());
        shell_print(shell, "Starting Anjay");
        atomic_store(&ANJAY_RUNNING, true);
    } else {
        shell_warn(shell, "Cannot start Anjay - already running");
    }

    return 0;
}

static void interrupt_anjay(avs_sched_t *sched, const void *anjay_ptr) {
    anjay_event_loop_interrupt(*(anjay_t *const *) anjay_ptr);
}

static int cmd_anjay_stop(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (atomic_load(&ANJAY_RUNNING)) {
        shell_print(shell, "Shutting down Anjay...");

        // change the flag first to interrupt the thread if event loop is not
        // running yet
        atomic_store(&ANJAY_RUNNING, false);

        SYNCHRONIZED(ANJAY_MUTEX) {
            if (ANJAY) {
                // hack to make sure that anjay_event_loop_interrupt() is called
                // only when the event loop is running
                anjay_t *anjay = ANJAY;
                AVS_SCHED_NOW(anjay_get_scheduler(anjay), NULL, interrupt_anjay,
                              &anjay, sizeof(anjay));
            }
        }

        SYNCHRONIZED(ANJAY_THREAD_RUNNING_MUTEX) {
            if (ANJAY_THREAD_RUNNING) {
                shell_print(shell, "Waiting for Anjay to stop...");
                shell_print(shell,
                            "If a DTLS handshake is in progress, it might take "
                            "up to 15 s for it to time out");
            }
            while (ANJAY_THREAD_RUNNING) {
                k_condvar_wait(&ANJAY_THREAD_RUNNING_CONDVAR,
                               &ANJAY_THREAD_RUNNING_MUTEX,
                               K_FOREVER);
            }
        }
    } else {
        shell_warn(shell, "Anjay is not running");
    }

    return 0;
}

#if defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
static int cmd_anjay_session_cache_purge(const struct shell *shell,
                                         size_t argc,
                                         char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    int err;
    if ((err = tls_session_cache_purge())) {
        shell_warn(shell, "Could not purge the TLS session cache");
    } else {
        shell_print(shell, "Successfully purged the TLS session cache");
    }

    return err;
}
#endif // defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)

static int
cmd_anjay_config_set(const struct shell *shell, size_t argc, char **argv) {
    if (atomic_load(&ANJAY_RUNNING)) {
        shell_print(shell, "Cannot change the config while Anjay is running");
        return -1;
    }

    return config_set_option(shell, argc, argv);
}

static int
cmd_anjay_config_default(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (atomic_load(&ANJAY_RUNNING)) {
        shell_print(shell, "Cannot change the config while Anjay is running");
        return -1;
    }

    config_default_init();
    return 0;
}

static int
cmd_anjay_config_show(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    config_print_summary(shell);

    return 0;
}

static int
cmd_anjay_config_save(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(shell, "Saving config");
    config_save(shell_backend_uart_get_ptr());
    config_print_summary(shell);

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
        sub_anjay_config_set,
        SHELL_CMD(OPTION_KEY_EP_NAME,
                  NULL,
                  "Endpoint name",
                  cmd_anjay_config_set),
#ifdef CONFIG_WIFI
        SHELL_CMD(OPTION_KEY_SSID, NULL, "Wi-Fi SSID", cmd_anjay_config_set),
        SHELL_CMD(OPTION_KEY_PASSWORD,
                  NULL,
                  "Wi-Fi password",
                  cmd_anjay_config_set),
#endif // CONFIG_WIFI
        SHELL_CMD(OPTION_KEY_URI, NULL, "Server URI", cmd_anjay_config_set),
        SHELL_CMD(OPTION_KEY_PSK, NULL, "PSK", cmd_anjay_config_set),
        SHELL_CMD(OPTION_KEY_BOOTSTRAP,
                  NULL,
                  "Perform bootstrap",
                  cmd_anjay_config_set),
#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF
        SHELL_CMD(OPTION_KEY_GPS_NRF_PRIO_MODE_TIMEOUT,
                  NULL,
                  "GPS priority mode timeout - determines (in seconds) for how "
                  "long the modem can run with LTE disabled, in case of "
                  "trouble with producing a GPS fix. Set to 0 to disable GPS "
                  "priority mode at all.",
                  cmd_anjay_config_set),
        SHELL_CMD(OPTION_KEY_GPS_NRF_PRIO_MODE_COOLDOWN,
                  NULL,
                  "GPS priority mode cooldown - determines (in seconds) how "
                  "much time must pass after a failed try to produce a GPS fix "
                  "to enable GPS priority mode again.",
                  cmd_anjay_config_set),
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF
        SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(
        sub_anjay_config,
        SHELL_CMD(default,
                  NULL,
                  "Restore the default config",
                  cmd_anjay_config_default),
        SHELL_CMD(save, NULL, "Save Anjay config", cmd_anjay_config_save),
        SHELL_CMD(set, &sub_anjay_config_set, "Change Anjay config", NULL),
        SHELL_CMD(show, NULL, "Show Anjay config", cmd_anjay_config_show),
        SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(
        sub_anjay,
        SHELL_CMD(start, NULL, "Save config and start Anjay", cmd_anjay_start),
        SHELL_CMD(stop, NULL, "Stop Anjay", cmd_anjay_stop),
        SHELL_CMD(config, &sub_anjay_config, "Configure Anjay params", NULL),
#if defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
        SHELL_CMD(session_cache_purge,
                  NULL,
                  "Remove the TLS session data cached in the nRF modem",
                  cmd_anjay_session_cache_purge),
#endif // defined(CONFIG_NRF_MODEM_LIB) && defined(CONFIG_MODEM_KEY_MGMT)
        SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(anjay, &sub_anjay, "Anjay commands", NULL);
