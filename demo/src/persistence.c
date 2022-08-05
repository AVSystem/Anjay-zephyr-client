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

#include <zephyr.h>
#include <logging/log.h>
#include <settings/settings.h>

#include <anjay/core.h>
#include <anjay/access_control.h>
#include <anjay/security.h>
#include <anjay/server.h>

#include <avsystem/commons/avs_stream_inbuf.h>
#include <avsystem/commons/avs_stream_membuf.h>

#include "persistence.h"

LOG_MODULE_REGISTER(persistence);

#define PERSISTENCE_ROOT_NAME "anjay_persistence"

#ifdef CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
#define FACTORY_PROVISIONING_ROOT_NAME "anjay_factory"
#endif // CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING

struct persistence_load_args {
	size_t left_to_read;
	anjay_t *anjay;
	const char *root_name;
};

typedef avs_error_t restore_fn_t(anjay_t *anjay, avs_stream_t *stream);
typedef avs_error_t persist_fn_t(anjay_t *anjay, avs_stream_t *stream);
typedef bool is_modified_fn_t(anjay_t *anjay);
typedef void purge_fn_t(anjay_t *anjay);

#define DECL_TARGET(Name)                                                                          \
	{                                                                                          \
		.name = AVS_QUOTE(Name), .restore = anjay_##Name##_restore,                        \
		.persist = anjay_##Name##_persist, .is_modified = anjay_##Name##_is_modified,      \
		.purge = anjay_##Name##_purge                                                      \
	}

struct persistence_target {
	const char *name;
	restore_fn_t *restore;
	persist_fn_t *persist;
	is_modified_fn_t *is_modified;
	purge_fn_t *purge;
};

static bool previous_attempt_failed;
static const struct persistence_target targets[] = { DECL_TARGET(access_control),
						     DECL_TARGET(security_object),
						     DECL_TARGET(server_object) };

#undef DECL_TARGET

int persistence_init(void)
{
	if (settings_subsys_init()) {
		LOG_ERR("Couldn't init settings subsystem");
		return -1;
	}
	return 0;
}

static int settings_purge(const char *root_name)
{
	for (size_t i = 0; i < AVS_ARRAY_SIZE(targets); i++) {
		char key_buf[64];

		if (avs_simple_snprintf(key_buf, sizeof(key_buf), "%s/%s", root_name,
					targets[i].name) < 0 ||
		    settings_save_one(key_buf, NULL, 0)) {
			LOG_ERR("Couldn't delete %s/%s from storage", root_name, targets[i].name);
			return -1;
		}
	}
	return 0;
}

int persistence_purge(void)
{
	return settings_purge(PERSISTENCE_ROOT_NAME);
}

static int persistence_load(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg,
			    void *args_)
{
	struct persistence_load_args *args = (struct persistence_load_args *)args_;

	if (!key) {
		return -ENOENT;
	}

	for (size_t i = 0; i < AVS_ARRAY_SIZE(targets); i++) {
		if (!strcmp(key, targets[i].name)) {
			void *buf = avs_malloc(len);
			avs_stream_inbuf_t stream = AVS_STREAM_INBUF_STATIC_INITIALIZER;

			if (!buf) {
				return -ENOMEM;
			}

			avs_stream_inbuf_set_buffer(&stream, buf, len);

			int result = -1;

			if (read_cb(cb_arg, buf, len) >= 0 &&
			    avs_is_ok(targets[i].restore(args->anjay, (avs_stream_t *)&stream))) {
				args->left_to_read--;
				result = 0;

				LOG_INF("Successfully loaded %s from persistence", key);
			} else {
				LOG_ERR("Couldn't load %s from persistence", key);
			}

			avs_free(buf);
			return result;
		}
	}

	LOG_WRN("Unknown key: %s/%s, skipping", args->root_name, key);
	return 0;
}

static int restore_anjay_from_settings(anjay_t *anjay, const char *root_name, bool purge_on_fail)
{
	assert(anjay);
	assert(root_name);

	struct persistence_load_args args = { .anjay = anjay,
					      .left_to_read = AVS_ARRAY_SIZE(targets),
					      .root_name = root_name };

	if (settings_load_subtree_direct(root_name, persistence_load, (void *)&args) ||
	    args.left_to_read > 0) {
		LOG_ERR("Couldn't restore Anjay from %s", root_name);

		if (purge_on_fail) {
			for (size_t i = 0; i < AVS_ARRAY_SIZE(targets); i++) {
				targets[i].purge(anjay);
			}
			settings_purge(root_name);
		}
		return -1;
	}

	LOG_INF("Anjay restored from %s", root_name);
	return 0;
}

int restore_anjay_from_persistence(anjay_t *anjay)
{
	return restore_anjay_from_settings(anjay, PERSISTENCE_ROOT_NAME, true);
}

#ifdef CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING
int restore_anjay_from_factory_provisioning(anjay_t *anjay)
{
	return restore_anjay_from_settings(anjay, FACTORY_PROVISIONING_ROOT_NAME, false);
}
#endif // CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING

static int persist_target_to_settings(anjay_t *anjay, const char *root_name,
				      const struct persistence_target *target)
{
	avs_stream_t *stream = avs_stream_membuf_create();
	void *collected_stream = NULL;
	size_t collected_stream_len;

	if (!stream) {
		return -ENOMEM;
	}

	int result = -1;

	if (avs_is_err(target->persist(anjay, stream)) ||
	    avs_is_err(avs_stream_membuf_take_ownership(stream, &collected_stream,
							&collected_stream_len))) {
		LOG_ERR("Couldn't persist %s", target->name);
		goto cleanup;
	}

	char key_buf[64];

	if (avs_simple_snprintf(key_buf, sizeof(key_buf), "%s/%s", root_name, target->name) < 0 ||
	    settings_save_one(key_buf, collected_stream, collected_stream_len)) {
		LOG_ERR("Couldn't save %s/%s to storage", root_name, target->name);
		previous_attempt_failed = true;
		goto cleanup;
	}

	LOG_INF("%s persisted, len: %zu", target->name, collected_stream_len);

	result = 0;

cleanup:
	avs_stream_cleanup(&stream);
	avs_free(collected_stream);
	return result;
}

int persist_anjay_if_required(anjay_t *anjay)
{
	assert(anjay);

	bool anything_persisted = false;

	for (size_t i = 0; i < AVS_ARRAY_SIZE(targets); i++) {
		if (!previous_attempt_failed && !targets[i].is_modified(anjay)) {
			continue;
		}

		int result = persist_target_to_settings(anjay, PERSISTENCE_ROOT_NAME, &targets[i]);

		if (result) {
			return result;
		}

		anything_persisted = true;
	}

	if (anything_persisted) {
		previous_attempt_failed = false;
		LOG_INF("All targets successfully persisted");
	}
	return 0;
}

#ifdef CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING_INITIAL_FLASH
static int provisioning_info_present_cb(const char *key, size_t len, settings_read_cb read_cb,
					void *cb_arg, void *param)
{
	bool *provisioning_info_present_ptr = (bool *)param;

	if (!key) {
		return -ENOENT;
	}

	if (!*provisioning_info_present_ptr) {
		for (size_t i = 0; i < AVS_ARRAY_SIZE(targets); i++) {
			if (!strcmp(key, targets[i].name)) {
				*provisioning_info_present_ptr = true;
				break;
			}
		}
	}

	return 0;
}

bool is_factory_provisioning_info_present(void)
{
	bool provisioning_info_present = false;

	settings_load_subtree_direct(FACTORY_PROVISIONING_ROOT_NAME, provisioning_info_present_cb,
				     (void *)&provisioning_info_present);
	return provisioning_info_present;
}

int persist_factory_provisioning_info(anjay_t *anjay)
{
	assert(anjay);

	for (size_t i = 0; i < AVS_ARRAY_SIZE(targets); i++) {
		int result = persist_target_to_settings(anjay, FACTORY_PROVISIONING_ROOT_NAME,
							&targets[i]);

		if (result) {
			settings_purge(FACTORY_PROVISIONING_ROOT_NAME);
			return result;
		}
	}

	previous_attempt_failed = false;
	return 0;
}
#endif // CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING_INITIAL_FLASH
