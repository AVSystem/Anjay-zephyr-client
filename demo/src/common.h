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

#ifndef ANJAY_ZEPHYR_COMMON

#define ANJAY_ZEPHYR_COMMON

#include <kernel.h>
#include <shell/shell.h>
#include <shell/shell_uart.h>
#include <stdlib.h>

#include <anjay/anjay.h>

extern struct k_mutex ANJAY_MTX;
extern anjay_t *ANJAY;
extern volatile bool ANJAY_RUNNING;

#define SYNCHRONIZED(Mtx)                                          \
    for (int _synchronized_exit = k_mutex_lock(&(Mtx), K_FOREVER); \
         !_synchronized_exit; _synchronized_exit = -1, k_mutex_unlock(&(Mtx)))

#define ANJAY_THREAD_PRIO 1
#define ANJAY_THREAD_STACK_SIZE 4096

extern struct k_thread ANJAY_THREAD;

#endif // ANJAY_ZEPHYR_COMMON
