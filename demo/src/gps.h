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

#include <kernel.h>
#include <stdbool.h>

#ifdef CONFIG_ANJAY_CLIENT_GPS
typedef struct gps_data_struct {
    bool valid;
    int64_t timestamp;
    double latitude;
    double longitude;
#    ifdef CONFIG_ANJAY_CLIENT_GPS_ALTITUDE
    double altitude;
#    endif // CONFIG_ANJAY_CLIENT_GPS_ALTITUDE
#    ifdef CONFIG_ANJAY_CLIENT_GPS_RADIUS
    double radius;
#    endif // CONFIG_ANJAY_CLIENT_GPS_RADIUS
#    ifdef CONFIG_ANJAY_CLIENT_GPS_VELOCITY
// TODO: implement velocity binary format as in 3GPP-TS_23.032
#        error "velocity binary format not implemented yet"
#    endif // CONFIG_ANJAY_CLIENT_GPS_VELOCITY
#    ifdef CONFIG_ANJAY_CLIENT_GPS_SPEED
    double speed;
#    endif // CONFIG_ANJAY_CLIENT_GPS_SPEED
} gps_data_t;

extern struct k_mutex GPS_READ_LAST_MTX;
extern gps_data_t GPS_READ_LAST;

int initialize_gps(void);
#endif // CONFIG_ANJAY_CLIENT_GPS
