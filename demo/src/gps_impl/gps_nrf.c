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

#if !(defined(CONFIG_BOARD_NRF9160DK_NRF9160_NS) \
      || defined(CONFIG_BOARD_THINGY91_NRF9160_NS))
#    error "This GPS implementation is not supported by selected board"
#endif // !(defined(CONFIG_BOARD_NRF9160DK_NRF9160_NS) ||
       // defined(CONFIG_BOARD_THINGY91_NRF9160_NS))

#include <kernel.h>
#include <logging/log.h>
#include <nrf_modem_at.h>
#include <nrf_modem_gnss.h>
#include <sys/timeutil.h>
#include <zephyr.h>

#include "../common.h"
#include "../config.h"
#include "../gps.h"
#include "../utils.h"

LOG_MODULE_REGISTER(gps_nrf);

#define INTERRUPTED_FIXES_WARN_THRESHOLD 10
#define PRIO_MODE_TIMEOUT K_MINUTES(5)

// Suggested GPS tuning parameters come from Nordic's SDK example
// https://github.com/nrfconnect/sdk-nrf/blob/master/samples/nrf9160/gps/src/main.c
static const char *const INIT_AT_COMMANDS[] = {
// AT%XMAGPIO controls antenna tuner
#ifdef CONFIG_BOARD_THINGY91_NRF9160_NS
    "AT%XMAGPIO=1,1,1,7,1,746,803,2,698,748,2,1710,2200,3,824,894,4,880,960,5,"
    "791,849,7,1565,1586",
#endif // CONFIG_BOARD_THINGY91_NRF9160_NS
#ifdef CONFIG_BOARD_NRF9160DK_NRF9160_NS
    "AT%XMAGPIO=1,0,0,1,1,1574,1577",
#endif // CONFIG_BOARD_NRF9160DK_NRF9160_NS

// AT%XCOEX0 controls external Low-Noise Amplifier
#ifdef CONFIG_ANJAY_CLIENT_EXTERNAL_GPS_ANTENNA
    "AT%XCOEX0",
#else  // CONFIG_ANJAY_CLIENT_EXTERNAL_GPS_ANTENNA
    "AT%XCOEX0=1,1,1565,1586",
#endif // CONFIG_ANJAY_CLIENT_EXTERNAL_GPS_ANTENNA
};

K_MUTEX_DEFINE(GPS_READ_LAST_MTX);
gps_data_t GPS_READ_LAST = {
    .valid = false
};

static int64_t prio_mode_cooldown_end_time = 0;
static size_t interrupted_fixes_in_row = 0;
K_MSGQ_DEFINE(incoming_pvt_msgq,
              sizeof(struct nrf_modem_gnss_pvt_data_frame),
              1,
              1);

static void incoming_pvt_work_handler(struct k_work *work);
static void prio_mode_disable_dwork_handler(struct k_work *work);

K_WORK_DEFINE(incoming_pvt_work, incoming_pvt_work_handler);
K_WORK_DELAYABLE_DEFINE(prio_mode_disable_dwork,
                        prio_mode_disable_dwork_handler);

static int64_t
gnss_datetime_to_timestamp(const struct nrf_modem_gnss_datetime *datetime) {
    struct tm broken_down = {
        .tm_year = datetime->year - 1900,
        .tm_mon = datetime->month - 1,
        .tm_mday = datetime->day,
        .tm_hour = datetime->hour,
        .tm_min = datetime->minute,
        .tm_sec = datetime->seconds
    };

    return timeutil_timegm64(&broken_down);
}

void prio_mode_disable(void) {
    LOG_INF("Disabling gnss_prio_mode");

    SYNCHRONIZED(ANJAY_MUTEX) {
        if (ANJAY) {
            anjay_transport_exit_offline(ANJAY, ANJAY_TRANSPORT_SET_IP);
        }
    }

    if (nrf_modem_gnss_prio_mode_disable()) {
        LOG_ERR("Couldn't disable gnss_prio_mode");
        return;
    }

    prio_mode_cooldown_end_time =
            k_uptime_get()
            + MSEC_PER_SEC * config_get_gps_nrf_prio_mode_cooldown();
}

static void incoming_pvt_work_handler(struct k_work *work) {
    struct nrf_modem_gnss_pvt_data_frame pvt;
    if (k_msgq_get(&incoming_pvt_msgq, &pvt, K_NO_WAIT)) {
        return;
    }

    LOG_DBG("Incoming PVT flags: 0x%02hhx", pvt.flags);

    if (pvt.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID) {
        interrupted_fixes_in_row = 0;

        // It's not possible to call k_work_flush_delayable(dwork, ...) from WQ
        // running dwork, but if we're already executing inside this WQ, the
        // "async" calls below become safe
        if (k_work_delayable_busy_get(&prio_mode_disable_dwork)) {
            k_work_cancel_delayable(&prio_mode_disable_dwork);
            prio_mode_disable();
        }

        SYNCHRONIZED(GPS_READ_LAST_MTX) {
            if (!GPS_READ_LAST.valid) {
                LOG_INF("First valid GPS fix produced");
            }
            GPS_READ_LAST.valid = true;
            GPS_READ_LAST.timestamp = gnss_datetime_to_timestamp(&pvt.datetime);
            GPS_READ_LAST.latitude = pvt.latitude;
            GPS_READ_LAST.longitude = pvt.longitude;
            GPS_READ_LAST.altitude = pvt.altitude;
            GPS_READ_LAST.radius = pvt.accuracy;
            GPS_READ_LAST.speed = pvt.speed;
        }
    } else if (pvt.flags & NRF_MODEM_GNSS_PVT_FLAG_NOT_ENOUGH_WINDOW_TIME) {
        if (k_uptime_get() > prio_mode_cooldown_end_time
                && ++interrupted_fixes_in_row
                               == INTERRUPTED_FIXES_WARN_THRESHOLD) {
            interrupted_fixes_in_row = 0;

            uint32_t gps_prio_mode_timeout =
                    config_get_gps_nrf_prio_mode_timeout();
            if (gps_prio_mode_timeout == 0) {
                return;
            }

            LOG_WRN("GPS was interrupted multiple times by the LTE modem when "
                    "producing a fix");

            if (nrf_modem_gnss_prio_mode_enable()) {
                LOG_ERR("Couldn't enable gnss_prio_mode");
                return;
            }
            LOG_WRN("gnss_prio_mode enabled");

            SYNCHRONIZED(ANJAY_MUTEX) {
                if (ANJAY) {
                    anjay_transport_enter_offline(ANJAY,
                                                  ANJAY_TRANSPORT_SET_IP);
                }
            }

            k_work_schedule(&prio_mode_disable_dwork,
                            K_SECONDS(gps_prio_mode_timeout));
        }
    }
}

static void prio_mode_disable_dwork_handler(struct k_work *work) {
    prio_mode_disable();
}

static void gnss_event_handler(int event) {
    if (event == NRF_MODEM_GNSS_EVT_PVT) {
        struct nrf_modem_gnss_pvt_data_frame pvt;
        int32_t res =
                nrf_modem_gnss_read(&pvt, sizeof(pvt), NRF_MODEM_GNSS_DATA_PVT);
        assert(res == 0);

        if (k_msgq_put(&incoming_pvt_msgq, &pvt, K_NO_WAIT)) {
            return;
        }
        k_work_submit(&incoming_pvt_work);
    }
}

static int config_at(void) {
    for (size_t i = 0; i < ARRAY_SIZE(INIT_AT_COMMANDS); i++) {
        if (nrf_modem_at_printf("%s", INIT_AT_COMMANDS[i])) {
            LOG_ERR("Failed to write initial AT command: %s",
                    INIT_AT_COMMANDS[i]);
            return -1;
        }
    }

    return 0;
}

int initialize_gps(void) {
    if (config_at() || nrf_modem_gnss_event_handler_set(gnss_event_handler)
            || nrf_modem_gnss_fix_retry_set(0)
            || nrf_modem_gnss_fix_interval_set(1) || nrf_modem_gnss_start()) {
        LOG_ERR("Failed to initialize GPS interface");
        return -1;
    }
    return 0;
}
