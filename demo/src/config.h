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

#pragma once

#include <stdint.h>

#include <shell/shell.h>

#ifdef CONFIG_WIFI
#define OPTION_KEY_SSID wifi_ssid
#define OPTION_KEY_PASSWORD wifi_password
#endif // CONFIG_WIFI
#ifndef CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
#define OPTION_KEY_URI uri
#define OPTION_KEY_EP_NAME endpoint
#define OPTION_KEY_LIFETIME lifetime
#define OPTION_KEY_PSK psk
#define OPTION_KEY_BOOTSTRAP bootstrap
#endif // CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF
#define OPTION_KEY_GPS_NRF_PRIO_MODE_TIMEOUT gps_prio_mode_timeout
#define OPTION_KEY_GPS_NRF_PRIO_MODE_COOLDOWN gps_prio_mode_cooldown
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF
#if defined(CONFIG_ANJAY_CLIENT_PERSISTENCE) && !defined(CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING)
#define OPTION_KEY_USE_PERSISTENCE use_persistence
#endif /* defined(CONFIG_ANJAY_CLIENT_PERSISTENCE) &&
	* !defined(CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING)
	*/

#if defined(CONFIG_WIFI) || defined(CONFIG_ANJAY_CLIENT_GPS_NRF) ||                                \
	!defined(CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING)
#define WITH_ANJAY_CLIENT_CONFIG
#endif /* defined(CONFIG_WIFI) || defined(CONFIG_ANJAY_CLIENT_GPS_NRF) ||
	* !defined(CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING)
	*/

const char *config_default_ep_name(void);

#ifdef WITH_ANJAY_CLIENT_CONFIG
void config_init(const struct shell *shell);
void config_save(const struct shell *shell);

void config_default_init(void);

void config_print_summary(const struct shell *shell);

int config_set_option(const struct shell *shell, size_t argc, char **argv);
#endif // WITH_ANJAY_CLIENT_CONFIG

#ifndef CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
const char *config_get_endpoint_name(void);
#endif // CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING

#ifdef CONFIG_WIFI
const char *config_get_wifi_ssid(void);

const char *config_get_wifi_password(void);
#endif // CONFIG_WIFI

#ifndef CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
const char *config_get_server_uri(void);

uint32_t config_get_lifetime(void);

const char *config_get_psk(void);

bool config_is_bootstrap(void);
#endif // CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING

#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF
uint32_t config_get_gps_nrf_prio_mode_timeout(void);

uint32_t config_get_gps_nrf_prio_mode_cooldown(void);
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF

#ifdef CONFIG_ANJAY_CLIENT_PERSISTENCE
#ifdef CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
static inline bool config_is_use_persistence(void)
{
	return true;
}
#else // CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
bool config_is_use_persistence(void);
#endif // CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
#endif // CONFIG_ANJAY_CLIENT_PERSISTENCE
