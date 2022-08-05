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
#include <shell/shell.h>
#include <shell/shell_uart.h>
#include <stdatomic.h>
#include <stdlib.h>

#include <anjay/anjay.h>

#include "objects/objects.h"

extern volatile atomic_bool device_initialized;

extern anjay_t *volatile global_anjay;
extern struct k_mutex global_anjay_mutex;
extern volatile atomic_bool anjay_running;

#define ANJAY_THREAD_PRIO 1
#define ANJAY_THREAD_STACK_SIZE 8192
extern struct k_thread anjay_thread;
extern volatile bool anjay_thread_running;
extern struct k_mutex anjay_thread_running_mutex;
extern struct k_condvar anjay_thread_running_condvar;

#ifdef CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
struct cell_request_job_args {
	anjay_t *anjay;
	enum loc_assist_cell_request_type request_type;
};
avs_sched_clb_t cell_request_job;
#endif // CONFIG_ANJAY_CLIENT_LOCATION_SERVICES_MANUAL_CELL_BASED
#ifdef CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS
avs_sched_clb_t agps_request_job;
#endif // CONFIG_ANJAY_CLIENT_GPS_NRF_A_GPS

void interrupt_net_connect_wait_loop(void);
