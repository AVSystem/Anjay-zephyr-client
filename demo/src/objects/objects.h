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

#include <anjay/dm.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/util.h>

#ifdef CONFIG_ANJAY_CLIENT_NRF_LC_INFO
#include "../nrf_lc_info.h"
#endif // CONFIG_ANJAY_CLIENT_NRF_LC_INFO

#define TEMPERATURE_NODE DT_ALIAS(temperature)
#define TEMPERATURE_AVAILABLE DT_NODE_HAS_STATUS(TEMPERATURE_NODE, okay)

#define HUMIDITY_NODE DT_ALIAS(humidity)
#define HUMIDITY_AVAILABLE DT_NODE_HAS_STATUS(HUMIDITY_NODE, okay)

#define BAROMETER_NODE DT_ALIAS(barometer)
#define BAROMETER_AVAILABLE DT_NODE_HAS_STATUS(BAROMETER_NODE, okay)

#define DISTANCE_NODE DT_ALIAS(distance)
#define DISTANCE_AVAILABLE DT_NODE_HAS_STATUS(DISTANCE_NODE, okay)

#define ILLUMINANCE_NODE DT_ALIAS(illuminance)
#define ILLUMINANCE_AVAILABLE DT_NODE_HAS_STATUS(ILLUMINANCE_NODE, okay)

void basic_sensors_install(anjay_t *anjay);
void basic_sensors_update(anjay_t *anjay);

#define ACCELEROMETER_NODE DT_ALIAS(accelerometer)
#define ACCELEROMETER_AVAILABLE DT_NODE_HAS_STATUS(ACCELEROMETER_NODE, okay)

#define GYROMETER_NODE DT_ALIAS(gyrometer)
#define GYROMETER_AVAILABLE DT_NODE_HAS_STATUS(GYROMETER_NODE, okay)

#define MAGNETOMETER_NODE DT_ALIAS(magnetometer)
#define MAGNETOMETER_AVAILABLE DT_NODE_HAS_STATUS(MAGNETOMETER_NODE, okay)

void three_axis_sensors_install(anjay_t *anjay);
void three_axis_sensors_update(anjay_t *anjay);

#define PUSH_BUTTON_NODE(idx) DT_ALIAS(push_button_##idx)
#define PUSH_BUTTON_AVAILABLE(idx) DT_NODE_HAS_STATUS(PUSH_BUTTON_NODE(idx), okay)
#define PUSH_BUTTON_AVAILABLE_ANY                                                                  \
	(PUSH_BUTTON_AVAILABLE(0) || PUSH_BUTTON_AVAILABLE(1) || PUSH_BUTTON_AVAILABLE(2) ||       \
	 PUSH_BUTTON_AVAILABLE(3))
int push_button_object_install(anjay_t *anjay);

#define SWITCH_NODE(idx) DT_ALIAS(switch_##idx)
#define SWITCH_AVAILABLE(idx) DT_NODE_HAS_STATUS(SWITCH_NODE(idx), okay)
#define SWITCH_AVAILABLE_ANY (SWITCH_AVAILABLE(0) || SWITCH_AVAILABLE(1) || SWITCH_AVAILABLE(2))
const anjay_dm_object_def_t **switch_object_create(void);
void switch_object_release(const anjay_dm_object_def_t ***out_def);
void switch_object_update(anjay_t *anjay, const anjay_dm_object_def_t *const *def);

#define BUZZER_NODE DT_ALIAS(buzzer_pwm)
#define BUZZER_AVAILABLE DT_NODE_HAS_STATUS(BUZZER_NODE, okay)
const anjay_dm_object_def_t **buzzer_object_create(void);
void buzzer_object_release(const anjay_dm_object_def_t ***out_def);
void buzzer_object_update(anjay_t *anjay, const anjay_dm_object_def_t *const *def);

#define RGB_NODE DT_ALIAS(rgb_pwm)
#define LED_COLOR_LIGHT_AVAILABLE DT_NODE_HAS_STATUS(RGB_NODE, okay)
const anjay_dm_object_def_t **led_color_light_object_create(void);
void led_color_light_object_release(const anjay_dm_object_def_t ***out_def);

const anjay_dm_object_def_t **device_object_create(void);
void device_object_release(const anjay_dm_object_def_t ***out_def);
void device_object_update(anjay_t *anjay, const anjay_dm_object_def_t *const *def);

const anjay_dm_object_def_t **location_object_create(void);
void location_object_release(const anjay_dm_object_def_t ***out_def);
void location_object_update(anjay_t *anjay, const anjay_dm_object_def_t *const *def);

#ifdef CONFIG_ANJAY_CLIENT_NRF_LC_INFO

#define OID_CONN_MON 4

#define RID_CONN_MON_NETWORK_BEARER 0
#define RID_CONN_MON_AVAILABLE_NETWORK_BEARER 1
#define RID_CONN_MON_RSS 2
#define RID_CONN_MON_LINK_QUALITY 3
#define RID_CONN_MON_IP_ADDRESSES 4
#define RID_CONN_MON_CELL_ID 8
#define RID_CONN_MON_SMNC 9
#define RID_CONN_MON_SMCC 10
#define RID_CONN_MON_LAC 12

const anjay_dm_object_def_t **conn_mon_object_create(const struct nrf_lc_info *nrf_lc_info);
void conn_mon_object_release(const anjay_dm_object_def_t ***out_def);
void conn_mon_object_update(anjay_t *anjay, const anjay_dm_object_def_t *const *def,
			    const struct nrf_lc_info *nrf_lc_info);

#define OID_ECID 10256

#define RID_ECID_PHYSCELLID 0
#define RID_ECID_ARFCNEUTRA 2
#define RID_ECID_RSRP_RESULT 3
#define RID_ECID_RSRQ_RESULT 4
#define RID_ECID_UE_RXTXTIMEDIFF 5

const anjay_dm_object_def_t **ecid_object_create(const struct nrf_lc_info *nrf_lc_info);
void ecid_object_release(const anjay_dm_object_def_t ***out_def);
void ecid_object_update(anjay_t *anjay, const anjay_dm_object_def_t *const *def,
			const struct nrf_lc_info *nrf_lc_info);
uint8_t ecid_object_instance_count(const anjay_dm_object_def_t *const *def);
#endif // CONFIG_ANJAY_CLIENT_NRF_LC_INFO

#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES
const anjay_dm_object_def_t **loc_assist_object_create(void);
void loc_assist_object_release(const anjay_dm_object_def_t ***out_def);

#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS
#define LOC_ASSIST_A_GPS_MASK_UTC BIT(0)
#define LOC_ASSIST_A_GPS_MASK_EPHEMERIS BIT(1)
#define LOC_ASSIST_A_GPS_MASK_ALMANAC BIT(2)
#define LOC_ASSIST_A_GPS_MASK_KLOBUCHAR BIT(3)
#define LOC_ASSIST_A_GPS_MASK_NEQUICK BIT(4)
#define LOC_ASSIST_A_GPS_MASK_TOW BIT(5)
#define LOC_ASSIST_A_GPS_MASK_CLOCK BIT(6)
#define LOC_ASSIST_A_GPS_MASK_LOCATION BIT(7)
#define LOC_ASSIST_A_GPS_MASK_INTEGRITY BIT(8)

void loc_assist_object_send_agps_request(anjay_t *anjay,
					 const anjay_dm_object_def_t *const *obj_def,
					 uint32_t request_mask);
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS

#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
enum loc_assist_cell_request_type {
	LOC_ASSIST_CELL_REQUEST_INFORM_SINGLE = 1,
	LOC_ASSIST_CELL_REQUEST_INFORM_MULTI = 2,
	LOC_ASSIST_CELL_REQUEST_REQUEST_SINGLE = 3,
	LOC_ASSIST_CELL_REQUEST_REQUEST_MULTI = 4
};

void loc_assist_object_send_cell_request(anjay_t *anjay,
					 const anjay_dm_object_def_t *const *loc_assist_def,
					 const anjay_dm_object_def_t *const *ecid_def,
					 enum loc_assist_cell_request_type request_type);
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES
