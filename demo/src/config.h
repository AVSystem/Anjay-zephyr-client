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

#pragma once

#include <stdint.h>

#include <shell/shell.h>

// To preserve backward compatibility, do not remove any of these and add new
// options IDs right before _OPTION_STRING_END.
typedef enum {
#ifdef CONFIG_WIFI
    OPTION_SSID,
    OPTION_PASSWORD,
#endif // CONFIG_WIFI
    OPTION_URI,
    OPTION_EP_NAME,
    OPTION_PSK,
    OPTION_BOOTSTRAP,
#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF
    OPTION_GPS_NRF_PRIO_MODE_TIMEOUT,
    OPTION_GPS_NRF_PRIO_MODE_COOLDOWN,
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF
    _OPTION_STRING_END
} option_id_t;

void config_init(const struct shell *shell);
void config_save(const struct shell *shell);

void config_default_init(void);

void config_print_summary(const struct shell *shell);

int config_set_option(const struct shell *shell,
                      size_t argc,
                      char **argv,
                      option_id_t option);

const char *config_get_endpoint_name(void);

#ifdef CONFIG_WIFI
const char *config_get_wifi_ssid(void);

const char *config_get_wifi_password(void);
#endif // CONFIG_WIFI

const char *config_get_server_uri(void);

const char *config_get_psk(void);

bool config_is_bootstrap(void);

#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF
uint32_t config_get_gps_nrf_prio_mode_timeout(void);

uint32_t config_get_gps_nrf_prio_mode_cooldown(void);
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF
