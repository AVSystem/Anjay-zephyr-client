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

#include <kernel.h>
#include <stdbool.h>

#if defined(CONFIG_BOARD_NRF9160DK_NRF9160NS) \
        || defined(CONFIG_BOARD_THINGY91_NRF9160NS)
#    define GPS_AVAILABLE 1
#endif // defined(CONFIG_BOARD_NRF9160DK_NRF9160NS) ||
       // defined(CONFIG_BOARD_THINGY91_NRF9160NS)

#if GPS_AVAILABLE
#    if defined(CONFIG_BOARD_NRF9160DK_NRF9160NS) \
            || defined(CONFIG_BOARD_THINGY91_NRF9160NS)
#        define GPS_ALTITUDE_AVAILABLE 1
#        define GPS_RADIUS_AVAILABLE 1
#        define GPS_SPEED_AVAILABLE 1
#    endif // defined(CONFIG_BOARD_NRF9160DK_NRF9160NS) ||
           // defined(CONFIG_BOARD_THINGY91_NRF9160NS)

typedef struct gps_data_struct {
    bool valid;
    int64_t timestamp;
    double latitude;
    double longitude;
#    ifdef GPS_ALTITUDE_AVAILABLE
    double altitude;
#    endif // GPS_ALTITUDE_AVAILABLE
#    ifdef GPS_RADIUS_AVAILABLE
    double radius;
#    endif // GPS_RADIUS_AVAILABLE
#    ifdef GPS_VELOCITY_AVAILABLE
// TODO: implement velocity binary format as in 3GPP-TS_23.032
#        error "velocity binary format not implemented yet"
#    endif // GPS_VELOCITY_AVAILABLE
#    ifdef GPS_SPEED_AVAILABLE
    double speed;
#    endif // GPS_SPEED_AVAILABLE
} gps_data_t;

extern struct k_mutex GPS_READ_LAST_MTX;
extern gps_data_t GPS_READ_LAST;

int initialize_gps(void);
#endif // GPS_AVAILABLE
