/*
 * Copyright 2020-2021 AVSystem <avsystem@avsystem.com>
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

static int
cmd_anjay_start(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

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

static int cmd_anjay_stop(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (atomic_load(&ANJAY_RUNNING)) {
        anjay_event_loop_interrupt(ANJAY);
        atomic_store(&ANJAY_RUNNING, false);
        k_thread_join(&ANJAY_THREAD, K_FOREVER);
    } else {
        shell_warn(shell, "Anjay is not running");
    }

    return 0;
}

static int common_cmd_setter(const struct shell *shell,
                             size_t argc,
                             char **argv,
                             option_id_t option) {
    if (atomic_load(&ANJAY_RUNNING)) {
        shell_print(shell, "Cannot change the config while Anjay is running");
        return -1;
    } else {
        return config_set_option(shell, argc, argv, option);
    }
}

static int cmd_anjay_config_set_endpoint(const struct shell *shell,
                                         size_t argc,
                                         char **argv) {
    return common_cmd_setter(shell, argc, argv, OPTION_EP_NAME);
}

#ifdef CONFIG_WIFI
static int cmd_anjay_config_set_wifi_ssid(const struct shell *shell,
                                          size_t argc,
                                          char **argv) {
    return common_cmd_setter(shell, argc, argv, OPTION_SSID);
}

static int cmd_anjay_config_set_wifi_password(const struct shell *shell,
                                              size_t argc,
                                              char **argv) {
    return common_cmd_setter(shell, argc, argv, OPTION_PASSWORD);
}
#endif // CONFIG_WIFI

static int
cmd_anjay_config_set_uri(const struct shell *shell, size_t argc, char **argv) {
    return common_cmd_setter(shell, argc, argv, OPTION_URI);
}

static int
cmd_anjay_config_set_psk(const struct shell *shell, size_t argc, char **argv) {
    return common_cmd_setter(shell, argc, argv, OPTION_PSK);
}

static int cmd_anjay_config_set_bootstrap(const struct shell *shell,
                                          size_t argc,
                                          char **argv) {
    return common_cmd_setter(shell, argc, argv, OPTION_BOOTSTRAP);
}

#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF
static int cmd_anjay_config_set_gps_nrf_prio_mode_timeout(
        const struct shell *shell, size_t argc, char **argv) {
    return common_cmd_setter(
            shell, argc, argv, OPTION_GPS_NRF_PRIO_MODE_TIMEOUT);
}

static int cmd_anjay_config_set_gps_nrf_prio_mode_cooldown(
        const struct shell *shell, size_t argc, char **argv) {
    return common_cmd_setter(
            shell, argc, argv, OPTION_GPS_NRF_PRIO_MODE_COOLDOWN);
}
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF

static int
cmd_anjay_config_default(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (atomic_load(&ANJAY_RUNNING)) {
        shell_print(shell, "Cannot change the config while Anjay is running");
        return -1;
    } else {
        config_default_init();
    }

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
        SHELL_CMD(
                endpoint, NULL, "Endpoint name", cmd_anjay_config_set_endpoint),
#ifdef CONFIG_WIFI
        SHELL_CMD(
                wifi_ssid, NULL, "Wi-Fi SSID", cmd_anjay_config_set_wifi_ssid),
        SHELL_CMD(wifi_password,
                  NULL,
                  "Wi-Fi password",
                  cmd_anjay_config_set_wifi_password),
#endif // CONFIG_WIFI
        SHELL_CMD(uri, NULL, "Server URI", cmd_anjay_config_set_uri),
        SHELL_CMD(psk, NULL, "PSK", cmd_anjay_config_set_psk),
        SHELL_CMD(bootstrap,
                  NULL,
                  "Perform bootstrap",
                  cmd_anjay_config_set_bootstrap),
#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF
        SHELL_CMD(gps_prio_mode_timeout,
                  NULL,
                  "GPS priority mode timeout - determines (in seconds) for how "
                  "long the modem can run with LTE disabled, in case of "
                  "trouble with producing a GPS fix. Set to 0 to disable GPS "
                  "priority mode at all.",
                  cmd_anjay_config_set_gps_nrf_prio_mode_timeout),
        SHELL_CMD(gps_prio_mode_cooldown,
                  NULL,
                  "GPS priority mode cooldown - determines (in seconds) how "
                  "much time must pass after a failed try to produce a GPS fix "
                  "to enable GPS priority mode again.",
                  cmd_anjay_config_set_gps_nrf_prio_mode_cooldown),
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
        SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(anjay, &sub_anjay, "Anjay commands", NULL);
