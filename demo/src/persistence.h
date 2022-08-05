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

#include <anjay/core.h>

#ifdef CONFIG_ANJAY_CLIENT_PERSISTENCE
int persistence_init(void);
int persistence_purge(void);
int restore_anjay_from_persistence(anjay_t *anjay);
int persist_anjay_if_required(anjay_t *anjay);

#ifdef CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
int restore_anjay_from_factory_provisioning(anjay_t *anjay);
#endif // CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
#ifdef CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING_INITIAL_FLASH
bool is_factory_provisioning_info_present(void);
int persist_factory_provisioning_info(anjay_t *anjay);
#endif // CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING_INITIAL_FLASH
#endif // CONFIG_ANJAY_CLIENT_PERSISTENCE
