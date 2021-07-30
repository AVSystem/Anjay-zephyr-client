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
#include <net/net_conn_mgr.h>
#include <net/net_core.h>
#include <net/net_event.h>
#include <net/net_mgmt.h>

#include <avsystem/commons/avs_log.h>
#include <avsystem/commons/avs_prng.h>
LOG_MODULE_REGISTER(anjay);

#include "common.h"

#include "objects/objects.h"
#include <random/rand32.h>

static const anjay_dm_object_def_t **DEVICE_OBJ;

static anjay_t *ANJAY;
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
    sys_rand_get(out_buf, out_buf_len);
    return 0;
}

static void set_system_time(const struct sntp_time *time) {
    struct timespec ts = {
        .tv_sec = time->seconds,
        .tv_nsec = ((uint64_t) time->fraction * 1000000000) >> 32
    };
    if (clock_settime(CLOCK_REALTIME, &ts)) {
        avs_log(zephyr_demo, WARNING, "Failed to set time");
    }
}

void synchronize_clock(void) {
    struct sntp_time time;
    const uint32_t timeout_ms = 5000;
    if (sntp_simple(CONFIG_ANJAY_CLIENT_NTP_SERVER, timeout_ms, &time)) {
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
    /**
     * Add additional object registrations here
     */

    return 0;
}

static void update_objects_frequent(void) {
    device_object_update(ANJAY, DEVICE_OBJ);
    /**
     * Add additional object updates here
     */
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
        }

        cycle++;
        k_sleep(K_SECONDS(1));
    }
}

static void release_objects(void) {
    device_object_release(DEVICE_OBJ);
    /**
     * Add additional object release here
     */
}

void initialize_network(void) {
    avs_log(zephyr_demo, INFO, "Initializing network connection...");
    /**
     * Initialize WIFI, cellular or some other internet connection here
     */
    avs_log(zephyr_demo, INFO, "Connected to network");
}

static avs_crypto_prng_ctx_t *PRNG_CTX;

void run_anjay(void *arg1, void *arg2, void *arg3) {
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    const anjay_configuration_t config = {
        .endpoint_name = CONFIG_ANJAY_CLIENT_ENDPOINT_NAME,
        .in_buffer_size = 4000,
        .out_buffer_size = 4000,
        .prng_ctx = PRNG_CTX
    };

    ANJAY = anjay_new(&config);
    if (!ANJAY) {
        avs_log(zephyr_demo, ERROR, "Could not create Anjay object");
        exit(1);
    }

    // Install attr_storage and necessary objects
    if (anjay_attr_storage_install(ANJAY)
            || anjay_security_object_install(ANJAY)
            || anjay_server_object_install(ANJAY)) {
        avs_log(zephyr_demo, ERROR, "Failed to install necessary modules");
        goto cleanup;
    }

    if (register_objects()) {
        avs_log(zephyr_demo, ERROR, "Failed to initialize objects");
        goto cleanup;
    }

    const anjay_security_instance_t security_instance = {
        .ssid = 1,
        .bootstrap_server = false,
        .server_uri = CONFIG_ANJAY_CLIENT_SERVER_URI,
        .security_mode = ANJAY_SECURITY_PSK,
        .public_cert_or_psk_identity = CONFIG_ANJAY_CLIENT_PSK_IDENTITY,
        .public_cert_or_psk_identity_size =
                strlen(security_instance.public_cert_or_psk_identity),
        .private_cert_or_psk_key = CONFIG_ANJAY_CLIENT_PSK_KEY,
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
        goto cleanup;
    }

    if (!false) {
        anjay_iid_t server_instance_id = ANJAY_ID_INVALID;
        if (anjay_server_object_add_instance(ANJAY, &server_instance,
                                             &server_instance_id)) {
            avs_log(zephyr_demo, ERROR, "Failed to instantiate Server object");
            goto cleanup;
        }
    }

    k_thread_create(&UPDATE_OBJECTS_THREAD, UPDATE_OBJECTS_STACK,
                    UPDATE_OBJECTS_THREAD_STACK_SIZE, update_objects, NULL,
                    NULL, NULL, UPDATE_OBJECTS_THREAD_PRIO, 0, K_NO_WAIT);

    avs_log(zephyr_demo, INFO, "Successfully created thread");

    main_loop();

cleanup:
    anjay_delete(ANJAY);
    release_objects();
}

void main(void) {
    avs_log_set_handler(log_handler);
    avs_log_set_default_level(DEFAULT_LOG_LEVEL);

    initialize_network();

    k_sleep(K_SECONDS(1));
    synchronize_clock();

    PRNG_CTX = avs_crypto_prng_new(entropy_callback, NULL);

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
            avs_log(zephyr_demo, INFO, "Anjay stopped");
        } else {
            k_sleep(K_SECONDS(1));
        }
    }
}
