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
#include <stdlib.h>
#include <sys/printk.h>
#include <version.h>

#include <net/dns_resolve.h>
#include <net/sntp.h>

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

#include "objects/objects.h"
#include <random/rand32.h>

#ifdef CONFIG_WIFI
#    ifdef CONFIG_WIFI_ESWIFI
#        include <wifi/eswifi/eswifi.h>
#    endif // CONFIG_WIFI_ESWIFI
#    include <net/wifi.h>
#    include <net/wifi_mgmt.h>
#endif // CONFIG_WIFI

#ifdef CONFIG_LTE_LINK_CONTROL
#    include <modem/lte_lc.h>
#endif // CONFIG_LTE_LINK_CONTROL

#ifdef CONFIG_POSIX_API
#    include <posix/time.h>
#endif // CONFIG_POSIX_API

#ifdef CONFIG_DATE_TIME
#    include <date_time.h>
#endif // CONFIG_DATE_TIME

static const anjay_dm_object_def_t **DEVICE_OBJ;

#define ANJAY_THREAD_PRIO 1
#define ANJAY_THREAD_STACK_SIZE 8192

static struct k_thread ANJAY_THREAD;
K_THREAD_STACK_DEFINE(ANJAY_STACK, ANJAY_THREAD_STACK_SIZE);

static void set_system_time(const struct sntp_time *time) {
#ifdef CONFIG_POSIX_API
    struct timespec ts = {
        .tv_sec = time->seconds,
        .tv_nsec = ((uint64_t) time->fraction * 1000000000) >> 32
    };
    if (clock_settime(CLOCK_REALTIME, &ts)) {
        avs_log(zephyr_demo, WARNING, "Failed to set time");
    }
#endif // CONFIG_POSIX_API

#ifdef CONFIG_DATE_TIME
    const struct tm *current_time = gmtime(&time->seconds);
    date_time_set(current_time);
    date_time_update_async(NULL);
#endif // CONFIG_DATE_TIME
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

static int register_objects(anjay_t *anjay) {
    if (!(DEVICE_OBJ = device_object_create())
            || anjay_register_object(anjay, DEVICE_OBJ)) {
        return -1;
    }
    /**
     * Add additional object registrations here
     */

    return 0;
}

static void update_objects(avs_sched_t *sched, const void *anjay_ptr) {
    anjay_t *anjay = *(anjay_t *const *) anjay_ptr;

    device_object_update(anjay, DEVICE_OBJ);
    /**
     * Add additional object updates here
     */

    AVS_SCHED_DELAYED(sched, NULL, avs_time_duration_from_scalar(1, AVS_TIME_S),
                      update_objects, &anjay, sizeof(anjay));
}

static void release_objects(void) {
    device_object_release(DEVICE_OBJ);
    /**
     * Add additional object release here
     */
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
        .ssid = (uint8_t *) CONFIG_ANJAY_CLIENT_WIFI_SSID,
        .ssid_length = strlen(CONFIG_ANJAY_CLIENT_WIFI_SSID),
        .psk = (uint8_t *) CONFIG_ANJAY_CLIENT_WIFI_PASSWORD,
        .psk_length = strlen(CONFIG_ANJAY_CLIENT_WIFI_PASSWORD),
        .security = WIFI_SECURITY_TYPE_PSK
    };

    if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params,
                 sizeof(struct wifi_connect_req_params))) {
        avs_log(zephyr_demo, ERROR, "Failed to configure Wi-Fi");
        exit(1);
    }

    net_mgmt_event_wait_on_iface(iface, NET_EVENT_IPV4_ADDR_ADD, NULL, NULL,
                                 NULL, K_FOREVER);
#endif // CONFIG_WIFI

#ifdef CONFIG_LTE_LINK_CONTROL
    int ret = lte_lc_init_and_connect();
    if (ret < 0) {
        avs_log(zephyr_demo, ERROR, "LTE link could not be established.");
        exit(1);
    }
#endif // CONFIG_LTE_LINK_CONTROL
    avs_log(zephyr_demo, INFO, "Connected to network");
}

void run_anjay(void *arg1, void *arg2, void *arg3) {
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    const anjay_configuration_t config = {
        .endpoint_name = CONFIG_ANJAY_CLIENT_ENDPOINT_NAME,
        .in_buffer_size = 4000,
        .out_buffer_size = 4000
    };

    anjay_t *anjay = anjay_new(&config);
    if (!anjay) {
        avs_log(zephyr_demo, ERROR, "Could not create Anjay object");
        exit(1);
    }

    // Install attr_storage and necessary objects
    if (anjay_attr_storage_install(anjay)
            || anjay_security_object_install(anjay)
            || anjay_server_object_install(anjay)) {
        avs_log(zephyr_demo, ERROR, "Failed to install necessary modules");
        goto cleanup;
    }

    if (register_objects(anjay)) {
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
    if (anjay_security_object_add_instance(anjay, &security_instance,
                                           &security_instance_id)) {
        avs_log(zephyr_demo, ERROR, "Failed to instantiate Security object");
        goto cleanup;
    }

    if (!false) {
        anjay_iid_t server_instance_id = ANJAY_ID_INVALID;
        if (anjay_server_object_add_instance(anjay, &server_instance,
                                             &server_instance_id)) {
            avs_log(zephyr_demo, ERROR, "Failed to instantiate Server object");
            goto cleanup;
        }
    }

    avs_log(zephyr_demo, INFO, "Successfully created thread");

    update_objects(anjay_get_scheduler(anjay), &anjay);
    anjay_event_loop_run(anjay, avs_time_duration_from_scalar(1, AVS_TIME_S));

cleanup:
    anjay_delete(anjay);
    release_objects();
}

void main(void) {
    initialize_network();

    k_sleep(K_SECONDS(1));
    synchronize_clock();

    k_thread_create(&ANJAY_THREAD, ANJAY_STACK, ANJAY_THREAD_STACK_SIZE,
                    run_anjay, NULL, NULL, NULL, ANJAY_THREAD_PRIO, 0,
                    K_NO_WAIT);
    k_thread_join(&ANJAY_THREAD, K_FOREVER);
    avs_log(zephyr_demo, INFO, "Anjay stopped");
}
