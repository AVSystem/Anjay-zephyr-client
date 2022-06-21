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

struct persistence_load_args {
	size_t left_to_read;
	anjay_t *anjay;
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

static bool previous_attempt_failed;
static struct {
	const char *name;
	restore_fn_t *restore;
	persist_fn_t *persist;
	is_modified_fn_t *is_modified;
	purge_fn_t *purge;
} targets[] = { DECL_TARGET(access_control), DECL_TARGET(security_object),
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

int persistence_purge(void)
{
	for (size_t i = 0; i < AVS_ARRAY_SIZE(targets); i++) {
		char key_buf[64];

		if (avs_simple_snprintf(key_buf, sizeof(key_buf), PERSISTENCE_ROOT_NAME "/%s",
					targets[i].name) < 0 ||
		    settings_save_one(key_buf, NULL, 0)) {
			LOG_ERR("Couldn't delete persisted %s from storage", targets[i].name);
			return -1;
		}
	}
	return 0;
}

static int persistence_load(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg,
			    void *args_)
{
	if (!key) {
		return -ENOENT;
	}

	for (size_t i = 0; i < AVS_ARRAY_SIZE(targets); i++) {
		if (!strcmp(key, targets[i].name)) {
			struct persistence_load_args *args = (struct persistence_load_args *)args_;
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

	LOG_WRN("Unknown key: %s/%s, skipping", PERSISTENCE_ROOT_NAME, key);
	return 0;
}

int restore_anjay_from_persistence(anjay_t *anjay)
{
	assert(anjay);

	struct persistence_load_args args = { .anjay = anjay,
					      .left_to_read = AVS_ARRAY_SIZE(targets) };

	if (settings_load_subtree_direct(PERSISTENCE_ROOT_NAME, persistence_load, (void *)&args) ||
	    args.left_to_read > 0) {
		LOG_ERR("Couldn't restore Anjay from persistence");

		for (size_t i = 0; i < AVS_ARRAY_SIZE(targets); i++) {
			targets[i].purge(anjay);
		}
		persistence_purge();
		return -1;
	}

	LOG_INF("Anjay restored from persistence");
	return 0;
}

int persist_anjay_if_required(anjay_t *anjay)
{
	assert(anjay);

	bool anything_persisted = false;

	for (size_t i = 0; i < AVS_ARRAY_SIZE(targets); i++) {
		if (!previous_attempt_failed && !targets[i].is_modified(anjay)) {
			continue;
		}

		avs_stream_t *stream = avs_stream_membuf_create();
		void *collected_stream;
		size_t collected_stream_len;

		if (!stream) {
			return -ENOMEM;
		}

		int result = -1;

		if (avs_is_err(targets[i].persist(anjay, stream)) ||
		    avs_is_err(avs_stream_membuf_take_ownership(stream, &collected_stream,
								&collected_stream_len))) {
			LOG_ERR("Couldn't persist %s", targets[i].name);
			goto cleanup;
		}

		char key_buf[64];

		if (avs_simple_snprintf(key_buf, sizeof(key_buf), PERSISTENCE_ROOT_NAME "/%s",
					targets[i].name) < 0 ||
		    settings_save_one(key_buf, collected_stream, collected_stream_len)) {
			LOG_ERR("Couldn't save persisted %s to storage", targets[i].name);
			previous_attempt_failed = true;
			goto cleanup;
		}

		LOG_INF("%s persisted, len: %zu", targets[i].name, collected_stream_len);

		anything_persisted = true;
		result = 0;

		// clang-format off
cleanup:
		// clang-format on
		avs_stream_cleanup(&stream);
		if (result) {
			return result;
		}
	}

	if (anything_persisted) {
		previous_attempt_failed = false;
		LOG_INF("All targets successfully persisted");
	}
	return 0;
}
