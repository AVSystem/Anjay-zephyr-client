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

static const anjay_dm_object_def_t **TEMPERATURE_OBJ;
static const anjay_dm_object_def_t **HUMIDITY_OBJ;
static const anjay_dm_object_def_t **BAROMETER_OBJ;
static const anjay_dm_object_def_t **ACCELEROMETER_OBJ;
static const anjay_dm_object_def_t **DISTANCE_OBJ;
static const anjay_dm_object_def_t **GYROMETER_OBJ;
static const anjay_dm_object_def_t **MAGNETOMETER_OBJ;
static const anjay_dm_object_def_t **BUZZER_OBJ;
static const anjay_dm_object_def_t **LED_COLOR_LIGHT_OBJ;
static const anjay_dm_object_def_t **LOCATION_OBJ;
static const anjay_dm_object_def_t **SWITCH_OBJ;
static const anjay_dm_object_def_t **PUSH_BUTTON_OBJ;
static const anjay_dm_object_def_t **DEVICE_OBJ;

anjay_t *ANJAY;
K_MUTEX_DEFINE(ANJAY_MTX);
volatile bool ANJAY_RUNNING = false;

// main thread priority is 0 and 1 is the lower priority
#define UPDATE_OBJECTS_THREAD_PRIO 2
#define UPDATE_OBJECTS_THREAD_STACK_SIZE 2048

static struct k_thread UPDATE_OBJECTS_THREAD;
K_THREAD_STACK_DEFINE(UPDATE_OBJECTS_STACK, UPDATE_OBJECTS_THREAD_STACK_SIZE);

struct k_thread ANJAY_THREAD;
K_THREAD_STACK_DEFINE(ANJAY_STACK, ANJAY_THREAD_STACK_SIZE);

void main_loop(void) {
    while (ANJAY_RUNNING) {
        SYNCHRONIZED(ANJAY_MTX) {
            // Obtain all network data sources
            AVS_LIST(avs_net_socket_t *const) sockets = NULL;
            sockets = anjay_get_sockets(ANJAY);

            // Prepare to poll() on them
            size_t numsocks = AVS_LIST_SIZE(sockets);
            struct pollfd pollfds[numsocks];
            size_t i = 0;
            AVS_LIST(avs_net_socket_t *const) sock;
            AVS_LIST_FOREACH(sock, sockets) {
                pollfds[i].fd = *(const int *) avs_net_socket_get_system(*sock);
                pollfds[i].events = POLLIN;
                pollfds[i].revents = 0;
                ++i;
            }

            const int max_wait_time_ms = 1000;
            // Determine the expected time to the next job in milliseconds.
            // If there is no job we will wait till something arrives for
            // at most 1 second (i.e. max_wait_time_ms).
            int wait_ms = max_wait_time_ms;
            wait_ms =
                    anjay_sched_calculate_wait_time_ms(ANJAY, max_wait_time_ms);

            // Wait for the events if necessary, and handle them.
            if (poll(pollfds, numsocks, wait_ms) > 0) {
                int socket_id = 0;
                AVS_LIST(avs_net_socket_t *const) socket = NULL;
                AVS_LIST_FOREACH(socket, sockets) {
                    if (pollfds[socket_id].revents) {
                        if (anjay_serve(ANJAY, *socket)) {
                            avs_log(zephyr_demo, ERROR, "anjay_serve failed");
                        }
                    }
                    ++socket_id;
                }
            }

            // Finally run the scheduler
            anjay_sched_run(ANJAY);
        }
    }
}

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

static int register_objects(void) {
    if (!(DEVICE_OBJ = device_object_create())
            || anjay_register_object(ANJAY, DEVICE_OBJ)) {
        return -1;
    }

    if ((TEMPERATURE_OBJ = temperature_object_create())) {
        anjay_register_object(ANJAY, TEMPERATURE_OBJ);
    }
    if ((HUMIDITY_OBJ = humidity_object_create())) {
        anjay_register_object(ANJAY, HUMIDITY_OBJ);
    }
    if ((DISTANCE_OBJ = distance_object_create())) {
        anjay_register_object(ANJAY, DISTANCE_OBJ);
    }
    if ((BAROMETER_OBJ = barometer_object_create())) {
        anjay_register_object(ANJAY, BAROMETER_OBJ);
    }
    if ((ACCELEROMETER_OBJ = accelerometer_object_create())) {
        anjay_register_object(ANJAY, ACCELEROMETER_OBJ);
    }
    if ((GYROMETER_OBJ = gyrometer_object_create())) {
        anjay_register_object(ANJAY, GYROMETER_OBJ);
    }
    if ((MAGNETOMETER_OBJ = magnetometer_object_create())) {
        anjay_register_object(ANJAY, MAGNETOMETER_OBJ);
    }
    if ((BUZZER_OBJ = buzzer_object_create())) {
        anjay_register_object(ANJAY, BUZZER_OBJ);
    }
    if ((LED_COLOR_LIGHT_OBJ = led_color_light_object_create())) {
        anjay_register_object(ANJAY, LED_COLOR_LIGHT_OBJ);
    }
    if ((LOCATION_OBJ = location_object_create())) {
        anjay_register_object(ANJAY, LOCATION_OBJ);
    }
    if ((PUSH_BUTTON_OBJ = push_button_object_create())) {
        anjay_register_object(ANJAY, PUSH_BUTTON_OBJ);
    }
    if ((SWITCH_OBJ = switch_object_create())) {
        anjay_register_object(ANJAY, SWITCH_OBJ);
    }

    return 0;
}

static void update_objects_frequent(void) {
    device_object_update(ANJAY, DEVICE_OBJ);
    push_button_object_update(ANJAY, PUSH_BUTTON_OBJ);
    switch_object_update(ANJAY, SWITCH_OBJ);
    buzzer_object_update(ANJAY, BUZZER_OBJ);
}

static void update_objects_periodic(void) {
    temperature_object_update(ANJAY, TEMPERATURE_OBJ);
    humidity_object_update(ANJAY, HUMIDITY_OBJ);
    barometer_object_update(ANJAY, BAROMETER_OBJ);
    accelerometer_object_update(ANJAY, ACCELEROMETER_OBJ);
    distance_object_update(ANJAY, DISTANCE_OBJ);
    gyrometer_object_update(ANJAY, GYROMETER_OBJ);
    magnetometer_object_update(ANJAY, MAGNETOMETER_OBJ);
    location_object_update(ANJAY, LOCATION_OBJ);
}

void update_objects(void *arg1, void *arg2, void *arg3) {
    (void) arg1;
    (void) arg2;
    (void) arg3;

    assert(ANJAY);

    size_t cycle = 0;
    while (ANJAY_RUNNING) {
        SYNCHRONIZED(ANJAY_MTX) {
            update_objects_frequent();
            if (cycle % 5 == 0) {
                update_objects_periodic();
            }
        }

        status_led_toggle();

        cycle++;
        k_sleep(K_SECONDS(1));
    }
}

static void release_objects(void) {
    device_object_release(&DEVICE_OBJ);
    push_button_object_release(&PUSH_BUTTON_OBJ);
    temperature_object_release(&TEMPERATURE_OBJ);
    humidity_object_release(&HUMIDITY_OBJ);
    barometer_object_release(&BAROMETER_OBJ);
    accelerometer_object_release(&ACCELEROMETER_OBJ);
    distance_object_release(&DISTANCE_OBJ);
    gyrometer_object_release(&GYROMETER_OBJ);
    magnetometer_object_release(&MAGNETOMETER_OBJ);
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

static int initialize_anjay(void) {
    const anjay_configuration_t config = {
        .endpoint_name = config_get_endpoint_name(),
        .in_buffer_size = 4000,
        .out_buffer_size = 4000,
        .prng_ctx = PRNG_CTX
    };

    ANJAY = anjay_new(&config);
    if (!ANJAY) {
        avs_log(zephyr_demo, ERROR, "Could not create Anjay object");
        return -1;
    }

    if (anjay_attr_storage_install(ANJAY)
            || anjay_security_object_install(ANJAY)
            || anjay_server_object_install(ANJAY)) {
        avs_log(zephyr_demo, ERROR, "Failed to install necessary modules");
        return -1;
    }

    if (register_objects()) {
        avs_log(zephyr_demo, ERROR, "Failed to initialize objects");
        return -1;
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
    if (anjay_security_object_add_instance(ANJAY, &security_instance,
                                           &security_instance_id)) {
        avs_log(zephyr_demo, ERROR, "Failed to instantiate Security object");
        return -1;
    }

    if (!bootstrap) {
        anjay_iid_t server_instance_id = ANJAY_ID_INVALID;
        if (anjay_server_object_add_instance(ANJAY, &server_instance,
                                             &server_instance_id)) {
            avs_log(zephyr_demo, ERROR, "Failed to instantiate Server object");
            return -1;
        }
    }

    return 0;
}

void run_anjay(void *arg1, void *arg2, void *arg3) {
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    int err = -1;
    SYNCHRONIZED(ANJAY_MTX) {
        err = initialize_anjay();
    }
    if (err) {
        goto cleanup;
    }

    k_thread_create(&UPDATE_OBJECTS_THREAD, UPDATE_OBJECTS_STACK,
                    UPDATE_OBJECTS_THREAD_STACK_SIZE, update_objects, NULL,
                    NULL, NULL, UPDATE_OBJECTS_THREAD_PRIO, 0, K_NO_WAIT);

#if defined(CONFIG_THREAD_ANALYZER) && defined(CONFIG_THREAD_NAME)
    k_thread_name_set((k_tid_t) &UPDATE_OBJECTS_THREAD, "update_objects");
#endif // defined(CONFIG_THREAD_ANALYZER) && defined(CONFIG_THREAD_NAME)

    avs_log(zephyr_demo, INFO, "Successfully created thread");

    main_loop();

cleanup:
    SYNCHRONIZED(ANJAY_MTX) {
        if (ANJAY) {
            anjay_delete(ANJAY);
            ANJAY = NULL;
        }

        release_objects();
    }
}

void main(void) {
    avs_log_set_handler(log_handler);
    avs_log_set_default_level(DEFAULT_LOG_LEVEL);

    config_init(shell_backend_uart_get_ptr());

    status_led_init();

    initialize_network();

#if GPS_AVAILABLE
    initialize_gps();
#endif // GPS_AVAILABLE

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

    ANJAY_RUNNING = true;

    while (true) {
        if (ANJAY_RUNNING) {
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
