/*
 * Copyright 2020 AVSystem <avsystem@avsystem.com>
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

#include <stdio.h>

#include <devicetree.h>
#include <zephyr.h>

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

#include "default_config.h"
#include "menu.h"

#include "led.h"
#include "objects/objects.h"

#ifdef CONFIG_WIFI
#    include <net/wifi.h>
#    include <net/wifi_mgmt.h>
#    include <poll.h>
#else // CONFIG_WIFI
#    include <modem/lte_lc.h>
#endif // CONFIG_WIFI

#ifdef CONFIG_BOARD_DISCO_L475_IOT1
static const anjay_dm_object_def_t **TEMPERATURE_OBJ;
static const anjay_dm_object_def_t **HUMIDITY_OBJ;
static const anjay_dm_object_def_t **DISTANCE_OBJ;
static const anjay_dm_object_def_t **BAROMETER_OBJ;
static const anjay_dm_object_def_t **ACCELEROMETER_OBJ;
static const anjay_dm_object_def_t **GYROMETER_OBJ;
static const anjay_dm_object_def_t **MAGNETOMETER_OBJ;
#else  // CONFIG_BOARD_DISCO_L475_IOT1
static const anjay_dm_object_def_t **SWITCH_OBJ;
#endif // CONFIG_BOARD_DISCO_L475_IOT1
static const anjay_dm_object_def_t **PUSH_BUTTON_OBJ;
static const anjay_dm_object_def_t **DEVICE_OBJ;

static anjay_t *ANJAY;
K_MUTEX_DEFINE(ANJAY_MTX);

#define SYNCHRONIZED(Mtx)                                          \
    for (int _synchronized_exit = k_mutex_lock(&(Mtx), K_FOREVER); \
         !_synchronized_exit; _synchronized_exit = -1, k_mutex_unlock(&(Mtx)))

// main thread priority is 0 and 1 is the lower priority
#define UPDATE_OBJECTS_THREAD_PRIO 1
#define UPDATE_OBJECTS_THREAD_STACK_SIZE 2048

static struct k_thread UPDATE_OBJECTS_THREAD;
K_THREAD_STACK_DEFINE(UPDATE_OBJECTS_STACK, UPDATE_OBJECTS_THREAD_STACK_SIZE);

void main_loop(void) {
    while (true) {
        // Obtain all network data sources
        AVS_LIST(avs_net_socket_t *const) sockets = NULL;
        SYNCHRONIZED(ANJAY_MTX) {
            sockets = anjay_get_sockets(ANJAY);
        }

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
        SYNCHRONIZED(ANJAY_MTX) {
            wait_ms =
                    anjay_sched_calculate_wait_time_ms(ANJAY, max_wait_time_ms);
        }

        // Wait for the events if necessary, and handle them.
        if (poll(pollfds, numsocks, wait_ms) > 0) {
            int socket_id = 0;
            AVS_LIST(avs_net_socket_t *const) socket = NULL;
            AVS_LIST_FOREACH(socket, sockets) {
                if (pollfds[socket_id].revents) {
                    SYNCHRONIZED(ANJAY_MTX) {
                        if (anjay_serve(ANJAY, *socket)) {
                            avs_log(zephyr_demo, ERROR, "anjay_serve failed");
                        }
                    }
                }
                ++socket_id;
            }
        }

        // Finally run the scheduler
        SYNCHRONIZED(ANJAY_MTX) {
            anjay_sched_run(ANJAY);
        }
    }
}

static void
log_handler(avs_log_level_t level, const char *module, const char *message) {
    printf("%s\n", message);
}

#ifdef CONFIG_BOARD_DISCO_L475_IOT1
int entropy_callback(unsigned char *out_buf, size_t out_buf_len, void *dev) {
    assert(dev);
    if (entropy_get_entropy((struct device *) dev, out_buf, out_buf_len)) {
        avs_log(zephyr_demo, ERROR, "Failed to get random bits");
        return -1;
    }
    return 0;
}
#else  // CONFIG_BOARD_DISCO_L475_IOT1
int entropy_callback(unsigned char *out_buf, size_t out_buf_len, void *dev) {
    assert(dev);
    // entropy_get_entropy() of nRF CC310 library requires the input buffer to
    // be 144 bytes long
    uint8_t buf[144];
    size_t remaining_bytes = out_buf_len;
    do {
        if (entropy_get_entropy((struct device *) dev, buf, sizeof(buf))) {
            avs_log(zephyr_demo, ERROR, "Failed to get random bits");
            return -1;
        }
        size_t num_of_bytes_to_copy;
        if (remaining_bytes < sizeof(buf)) {
            num_of_bytes_to_copy = remaining_bytes;
            remaining_bytes = 0;
        } else {
            num_of_bytes_to_copy = sizeof(buf);
            remaining_bytes -= sizeof(buf);
        }
        memcpy(out_buf, buf, num_of_bytes_to_copy);
        out_buf += num_of_bytes_to_copy;
    } while (remaining_bytes);
    return 0;
}
#endif // CONFIG_BOARD_DISCO_L475_IOT1

void update_objects(void *arg1, void *arg2, void *arg3) {
    (void) arg1;
    (void) arg2;
    (void) arg3;

    assert(ANJAY);

    size_t cycle = 0;
    while (ANJAY) {
        SYNCHRONIZED(ANJAY_MTX) {
#ifdef CONFIG_BOARD_DISCO_L475_IOT1
            if (cycle % 5 == 0) {
                temperature_object_update(ANJAY, TEMPERATURE_OBJ);
                humidity_object_update(ANJAY, HUMIDITY_OBJ);
                distance_object_update(ANJAY, DISTANCE_OBJ);
                barometer_object_update(ANJAY, BAROMETER_OBJ);
                accelerometer_object_update(ANJAY, ACCELEROMETER_OBJ);
                gyrometer_object_update(ANJAY, GYROMETER_OBJ);
                magnetometer_object_update(ANJAY, MAGNETOMETER_OBJ);
            }
#else  // CONFIG_BOARD_DISCO_L475_IOT1
            switch_object_update(ANJAY, SWITCH_OBJ);
#endif // CONFIG_BOARD_DISCO_L475_IOT1
            push_button_object_update(ANJAY, PUSH_BUTTON_OBJ);
            device_object_update(ANJAY, DEVICE_OBJ);
        }

        disco_led_toggle(DISCO_LED2);

        cycle++;
        k_sleep(K_SECONDS(1));
    }
}

void synchronize_clock(void) {
    struct sntp_time time;
    const uint32_t timeout_ms = 5000;
    if (sntp_simple(NTP_SERVER, timeout_ms, &time)) {
        avs_log(zephyr_demo, WARNING, "Failed to get current time");
    } else {
        struct timespec ts = {
            .tv_sec = time.seconds,
            .tv_nsec = ((uint64_t) time.fraction * 1000000000) >> 32
        };
        if (clock_settime(CLOCK_REALTIME, &ts)) {
            avs_log(zephyr_demo, WARNING, "Failed to set time");
        }
    }
}

void initialize_network(void) {
    avs_log(zephyr_demo, INFO, "Initializing network connection...");
#ifdef CONFIG_WIFI
    struct net_if *iface = net_if_get_default();

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
        while (true) {
        }
    }

    net_mgmt_event_wait_on_iface(iface, NET_EVENT_IPV4_ADDR_ADD, NULL, NULL,
                                 NULL, K_FOREVER);
#else  // CONFIG_WIFI
    int ret = lte_lc_init_and_connect();
    if (ret < 0) {
        avs_log(zephyr_demo, ERROR, "LTE link could not be established.");
        while (true) {
        }
    }
#endif // CONFIG_WIFI
    avs_log(zephyr_demo, INFO, "Connected to network");
}

void main(void) {
    avs_log_set_handler(log_handler);
    avs_log_set_default_level(DEFAULT_LOG_LEVEL);

    config_init();
    disco_led_init();

    initialize_network();

    const char *dns_servers[] = { "8.8.8.8", NULL };
    if (dns_resolve_init(dns_resolve_get_default(), dns_servers, NULL)) {
        avs_log(zephyr_demo, ERROR, "DNS resolver init fail");
        while (true) {
        }
    }

#ifdef CONFIG_BOARD_DISCO_L475_IOT1
    synchronize_clock();
#endif // CONFIG_BOARD_DISCO_L475_IOT1

    struct device *entropy_dev =
            device_get_binding(DT_CHOSEN_ZEPHYR_ENTROPY_LABEL);
    if (!entropy_dev) {
        avs_log(zephyr_demo, ERROR, "Failed to acquire entropy device");
        while (true) {
        }
    }

    avs_crypto_prng_ctx_t *prng_ctx =
            avs_crypto_prng_new(entropy_callback, entropy_dev);

    if (!prng_ctx) {
        avs_log(zephyr_demo, ERROR, "Failed to initialize PRNG ctx");
        while (true) {
        }
    }

    const anjay_configuration_t config = {
        .endpoint_name = config_get_endpoint_name(),
        .in_buffer_size = 4000,
        .out_buffer_size = 4000,
        .prng_ctx = prng_ctx
    };

    ANJAY = anjay_new(&config);
    if (!ANJAY) {
        avs_log(zephyr_demo, ERROR, "Could not create Anjay object");
        while (true) {
        }
    }

    // Install attr_storage and necessary objects
    if (anjay_attr_storage_install(ANJAY)
            || anjay_security_object_install(ANJAY)
            || anjay_server_object_install(ANJAY)) {
        avs_log(zephyr_demo, ERROR, "Failed to install necessary modules");
        goto cleanup;
    }

    if (!(DEVICE_OBJ = device_object_create())
            || anjay_register_object(ANJAY, DEVICE_OBJ)
            || !(PUSH_BUTTON_OBJ = push_button_object_create())
            || anjay_register_object(ANJAY, PUSH_BUTTON_OBJ)
#ifdef CONFIG_BOARD_DISCO_L475_IOT1
            || !(TEMPERATURE_OBJ = temperature_object_create())
            || anjay_register_object(ANJAY, TEMPERATURE_OBJ)
            || !(HUMIDITY_OBJ = humidity_object_create())
            || anjay_register_object(ANJAY, HUMIDITY_OBJ)
            || !(DISTANCE_OBJ = distance_object_create())
            || anjay_register_object(ANJAY, DISTANCE_OBJ)
            || !(BAROMETER_OBJ = barometer_object_create())
            || anjay_register_object(ANJAY, BAROMETER_OBJ)
            || !(ACCELEROMETER_OBJ = accelerometer_object_create())
            || anjay_register_object(ANJAY, ACCELEROMETER_OBJ)
            || !(GYROMETER_OBJ = gyrometer_object_create())
            || anjay_register_object(ANJAY, GYROMETER_OBJ)
            || !(MAGNETOMETER_OBJ = magnetometer_object_create())
            || anjay_register_object(ANJAY, MAGNETOMETER_OBJ)
#else  // CONFIG_BOARD_DISCO_L475_IOT1
            || !(SWITCH_OBJ = switch_object_create())
            || anjay_register_object(ANJAY, SWITCH_OBJ)
#endif // CONFIG_BOARD_DISCO_L475_IOT1
    ) {
        avs_log(zephyr_demo, ERROR, "Failed to initialize objects");
        goto cleanup;
    }

    const anjay_security_instance_t security_instance = {
        .ssid = 1,
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
    anjay_iid_t server_instance_id = ANJAY_ID_INVALID;
    if (anjay_security_object_add_instance(ANJAY, &security_instance,
                                           &security_instance_id)
            || anjay_server_object_add_instance(ANJAY, &server_instance,
                                                &server_instance_id)) {
        avs_log(zephyr_demo, ERROR,
                "Failed to instantiate Security or Server object");
        goto cleanup;
    }

    k_thread_create(&UPDATE_OBJECTS_THREAD, UPDATE_OBJECTS_STACK,
                    UPDATE_OBJECTS_THREAD_STACK_SIZE, update_objects, NULL,
                    NULL, NULL, UPDATE_OBJECTS_THREAD_PRIO, 0, K_NO_WAIT);

    main_loop();

cleanup:
    anjay_delete(ANJAY);
    avs_crypto_prng_free(&prng_ctx);
#ifdef CONFIG_BOARD_DISCO_L475_IOT1
    temperature_object_release(TEMPERATURE_OBJ);
    humidity_object_release(HUMIDITY_OBJ);
    distance_object_release(DISTANCE_OBJ);
    barometer_object_release(BAROMETER_OBJ);
    accelerometer_object_release(ACCELEROMETER_OBJ);
    gyrometer_object_release(GYROMETER_OBJ);
    magnetometer_object_release(MAGNETOMETER_OBJ);
#else  // CONFIG_BOARD_DISCO_L475_IOT1
    switch_object_release(SWITCH_OBJ);
#endif // CONFIG_BOARD_DISCO_L475_IOT1
    push_button_object_release(PUSH_BUTTON_OBJ);
    device_object_release(DEVICE_OBJ);
}
