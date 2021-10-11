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

#include <devicetree.h>
#include <logging/log.h>
#include <stdlib.h>
#include <sys/printk.h>
#include <version.h>

#include <drivers/entropy.h>
#include <net/dns_resolve.h>
#include <net/sntp.h>
#include <posix/time.h>

#include <anjay/anjay.h>
#include <anjay/attr_storage.h>
#include <anjay/security.h>
#include <anjay/server.h>

#include <avsystem/commons/avs_log.h>
#include <avsystem/commons/avs_prng.h>
LOG_MODULE_REGISTER(anjay);

#include "common.h"

#include "config.h"
#include "default_config.h"

#include "gps.h"
#include "objects/objects.h"
#include "status_led.h"

#ifdef CONFIG_WIFI
#    ifdef CONFIG_WIFI_ESWIFI
#        include <wifi/eswifi/eswifi.h>
#    endif // CONFIG_WIFI_ESWIFI
#    include <net/wifi.h>
#    include <net/wifi_mgmt.h>
#else // CONFIG_WIFI
#    include <modem/lte_lc.h>
#endif // CONFIG_WIFI

#if defined(CONFIG_BOARD_NRF9160DK_NRF9160NS) \
        || defined(CONFIG_BOARD_THINGY91_NRF9160NS)
#    include <date_time.h>
#endif // CONFIG_BOARD_NRF9160DK_NRF9160NS ||
       // defined(CONFIG_BOARD_THINGY91_NRF9160NS)

static const anjay_dm_object_def_t **BUZZER_OBJ;
static const anjay_dm_object_def_t **LED_COLOR_LIGHT_OBJ;
static const anjay_dm_object_def_t **LOCATION_OBJ;
static const anjay_dm_object_def_t **SWITCH_OBJ;
static const anjay_dm_object_def_t **DEVICE_OBJ;

anjay_t *volatile ANJAY;
volatile atomic_bool ANJAY_RUNNING;

struct k_thread ANJAY_THREAD;
K_THREAD_STACK_DEFINE(ANJAY_STACK, ANJAY_THREAD_STACK_SIZE);

static void
log_handler(avs_log_level_t level, const char *module, const char *message) {
    switch (level) {
    case AVS_LOG_TRACE:
    case AVS_LOG_DEBUG:
        LOG_DBG("%s", message);
        break;

    case AVS_LOG_INFO:
        LOG_INF("%s", message);
        break;

    case AVS_LOG_WARNING:
        LOG_WRN("%s", message);
        break;

    case AVS_LOG_ERROR:
        LOG_ERR("%s", message);
        break;

    default:
        break;
    }
}

int entropy_callback(unsigned char *out_buf, size_t out_buf_len, void *dev) {
    assert(dev);
    if (entropy_get_entropy((const struct device *) dev, out_buf,
                            out_buf_len)) {
        avs_log(zephyr_demo, ERROR, "Failed to get random bits");
        return -1;
    }
    return 0;
}

#ifdef CONFIG_BOARD_DISCO_L475_IOT1
static void set_system_time(const struct sntp_time *time) {
    struct timespec ts = {
        .tv_sec = time->seconds,
        .tv_nsec = ((uint64_t) time->fraction * 1000000000) >> 32
    };
    if (clock_settime(CLOCK_REALTIME, &ts)) {
        avs_log(zephyr_demo, WARNING, "Failed to set time");
    }
}
#else  // CONFIG_BOARD_DISCO_L475_IOT1
static void set_system_time(const struct sntp_time *time) {
    const struct tm *current_time = gmtime(&time->seconds);
    date_time_set(current_time);
    date_time_update_async(NULL);
}
#endif // CONFIG_BOARD_DISCO_L475_IOT1

void synchronize_clock(void) {
    struct sntp_time time;
    const uint32_t timeout_ms = 5000;
    if (sntp_simple(NTP_SERVER, timeout_ms, &time)) {
        avs_log(zephyr_demo, WARNING, "Failed to get current time");
    } else {
        set_system_time(&time);
    }
}

static int register_objects(anjay_t *anjay) {
    if (!(DEVICE_OBJ = device_object_create())
            || anjay_register_object(anjay, DEVICE_OBJ)) {
        return -1;
    }

    basic_sensors_install(anjay);
    three_axis_sensors_install(anjay);
    push_button_object_install(anjay);

    if ((BUZZER_OBJ = buzzer_object_create())) {
        anjay_register_object(anjay, BUZZER_OBJ);
    }
    if ((LED_COLOR_LIGHT_OBJ = led_color_light_object_create())) {
        anjay_register_object(anjay, LED_COLOR_LIGHT_OBJ);
    }
    if ((LOCATION_OBJ = location_object_create())) {
        anjay_register_object(anjay, LOCATION_OBJ);
    }
    if ((SWITCH_OBJ = switch_object_create())) {
        anjay_register_object(anjay, SWITCH_OBJ);
    }

    return 0;
}

static void update_objects_frequent(anjay_t *anjay) {
    device_object_update(anjay, DEVICE_OBJ);
    switch_object_update(anjay, SWITCH_OBJ);
    buzzer_object_update(anjay, BUZZER_OBJ);
}

static void update_objects_periodic(anjay_t *anjay) {
    basic_sensors_update(anjay);
    three_axis_sensors_update(anjay);
    location_object_update(anjay, LOCATION_OBJ);
}

static void update_objects(avs_sched_t *sched, const void *anjay_ptr) {
    anjay_t *anjay = *(anjay_t *const *) anjay_ptr;

    static size_t cycle = 0;

    update_objects_frequent(anjay);
    if (cycle % 5 == 0) {
        update_objects_periodic(anjay);
    }

    status_led_toggle();

    cycle++;
    AVS_SCHED_DELAYED(sched, NULL, avs_time_duration_from_scalar(1, AVS_TIME_S),
                      update_objects, &anjay, sizeof(anjay));
}

static void release_objects(void) {
    device_object_release(&DEVICE_OBJ);
    buzzer_object_release(&BUZZER_OBJ);
    led_color_light_object_release(&LED_COLOR_LIGHT_OBJ);
    switch_object_release(&SWITCH_OBJ);
    location_object_release(&LOCATION_OBJ);
}

void initialize_network(void) {
    avs_log(zephyr_demo, INFO, "Initializing network connection...");
#ifdef CONFIG_WIFI
    struct net_if *iface = net_if_get_default();

#    ifdef CONFIG_WIFI_ESWIFI
    struct eswifi_dev *eswifi = eswifi_by_iface_idx(0);
    assert(eswifi);
    // Set regulatory domain to "World Wide (passive Ch12-14)"; eS-WiFi defaults
    // to "US" which prevents connecting to networks that use channels 12-14.
    if (eswifi_at_cmd(eswifi, "CN=XV\r") < 0) {
        avs_log(zephyr_demo, WARNING, "Failed to set Wi-Fi regulatory domain");
    }
#    endif // CONFIG_WIFI_ESWIFI

    struct wifi_connect_req_params wifi_params = {
        .ssid = (uint8_t *) config_get_wifi_ssid(),
        .ssid_length = strlen(config_get_wifi_ssid()),
        .psk = (uint8_t *) config_get_wifi_password(),
        .psk_length = strlen(config_get_wifi_password()),
        .security = WIFI_SECURITY_TYPE_PSK
    };

    if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params,
                 sizeof(struct wifi_connect_req_params))) {
        avs_log(zephyr_demo, ERROR, "Failed to configure Wi-Fi");
        exit(1);
    }

    net_mgmt_event_wait_on_iface(iface, NET_EVENT_IPV4_ADDR_ADD, NULL, NULL,
                                 NULL, K_FOREVER);
#else  // CONFIG_WIFI
    int ret = lte_lc_init_and_connect();
    if (ret < 0) {
        avs_log(zephyr_demo, ERROR, "LTE link could not be established.");
        exit(1);
    }
#endif // CONFIG_WIFI
    avs_log(zephyr_demo, INFO, "Connected to network");
}

static avs_crypto_prng_ctx_t *PRNG_CTX;

static anjay_t *initialize_anjay(void) {
    const anjay_configuration_t config = {
        .endpoint_name = config_get_endpoint_name(),
        .in_buffer_size = 4000,
        .out_buffer_size = 4000,
        .prng_ctx = PRNG_CTX
    };

    anjay_t *anjay = anjay_new(&config);
    if (!anjay) {
        avs_log(zephyr_demo, ERROR, "Could not create Anjay object");
        return NULL;
    }

    if (anjay_attr_storage_install(anjay)
            || anjay_security_object_install(anjay)
            || anjay_server_object_install(anjay)) {
        avs_log(zephyr_demo, ERROR, "Failed to install necessary modules");
        goto error;
    }

    if (register_objects(anjay)) {
        avs_log(zephyr_demo, ERROR, "Failed to initialize objects");
        goto error;
    }

    const bool bootstrap = config_is_bootstrap();

    const anjay_security_instance_t security_instance = {
        .ssid = 1,
        .bootstrap_server = bootstrap,
        .server_uri = config_get_server_uri(),
        .security_mode = ANJAY_SECURITY_PSK,
        .public_cert_or_psk_identity = config.endpoint_name,
        .public_cert_or_psk_identity_size =
                strlen(security_instance.public_cert_or_psk_identity),
        .private_cert_or_psk_key = config_get_psk(),
        .private_cert_or_psk_key_size =
                strlen(security_instance.private_cert_or_psk_key)
    };

    const anjay_server_instance_t server_instance = {
        .ssid = 1,
        .lifetime = 60,
        .default_min_period = -1,
        .default_max_period = -1,
        .disable_timeout = -1,
        .binding = "U"
    };

    anjay_iid_t security_instance_id = ANJAY_ID_INVALID;
    if (anjay_security_object_add_instance(anjay, &security_instance,
                                           &security_instance_id)) {
        avs_log(zephyr_demo, ERROR, "Failed to instantiate Security object");
        goto error;
    }

    if (!bootstrap) {
        anjay_iid_t server_instance_id = ANJAY_ID_INVALID;
        if (anjay_server_object_add_instance(anjay, &server_instance,
                                             &server_instance_id)) {
            avs_log(zephyr_demo, ERROR, "Failed to instantiate Server object");
            goto error;
        }
    }

    return anjay;
error:
    anjay_delete(anjay);
    release_objects();
    return NULL;
}

void run_anjay(void *arg1, void *arg2, void *arg3) {
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    anjay_t *anjay = initialize_anjay();
    if (!anjay) {
        return;
    }

    avs_log(zephyr_demo, INFO, "Successfully created thread");

    ANJAY = anjay;
    update_objects(anjay_get_scheduler(anjay), &anjay);
    anjay_event_loop_run(ANJAY, avs_time_duration_from_scalar(1, AVS_TIME_S));
    ANJAY = NULL;

    anjay_delete(anjay);
    release_objects();
}

void main(void) {
    avs_log_set_handler(log_handler);
    avs_log_set_default_level(DEFAULT_LOG_LEVEL);

    config_init(shell_backend_uart_get_ptr());

    status_led_init();

    initialize_network();

#ifdef CONFIG_ANJAY_CLIENT_GPS
    initialize_gps();
#endif // CONFIG_ANJAY_CLIENT_GPS

    synchronize_clock();

    const struct device *entropy_dev =
            device_get_binding(DT_CHOSEN_ZEPHYR_ENTROPY_LABEL);
    if (!entropy_dev) {
        avs_log(zephyr_demo, ERROR, "Failed to acquire entropy device");
        exit(1);
    }

    PRNG_CTX = avs_crypto_prng_new(entropy_callback,
                                   (void *) (intptr_t) entropy_dev);

    if (!PRNG_CTX) {
        avs_log(zephyr_demo, ERROR, "Failed to initialize PRNG ctx");
        exit(1);
    }

    atomic_store(&ANJAY_RUNNING, true);

    while (true) {
        if (atomic_load(&ANJAY_RUNNING)) {
            k_thread_create(&ANJAY_THREAD, ANJAY_STACK, ANJAY_THREAD_STACK_SIZE,
                            run_anjay, NULL, NULL, NULL, ANJAY_THREAD_PRIO, 0,
                            K_NO_WAIT);
            k_thread_join(&ANJAY_THREAD, K_FOREVER);
            shell_print(shell_backend_uart_get_ptr(), "Anjay stopped");
        } else {
            k_sleep(K_SECONDS(1));
        }
    }
}
