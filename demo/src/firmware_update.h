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

#include <anjay/anjay.h>
#include <anjay/fw_update.h>

#ifdef CONFIG_ANJAY_CLIENT_FOTA
int fw_update_install(anjay_t *anjay);
void fw_update_apply(void);
bool fw_update_requested(void);
void fw_update_reboot(void);
#endif // CONFIG_ANJAY_CLIENT_FOTA
