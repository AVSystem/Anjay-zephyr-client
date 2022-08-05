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

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <fs/fs.h>
#include <fs/fs_sys.h>
#include <fs_mgmt/fs_mgmt.h>

#include <kernel.h>

#include <avsystem/commons/avs_errno_map.h>
#include <avsystem/commons/avs_stream_v_table.h>
#include <avsystem/commons/avs_utils.h>

#include "factory_flash.h"

/*
 * THE PROCESS FOR FLASHING FACTORY PROVISIONING INFORMATION
 *
 * High level flow from the user's standpoint:
 *
 * 1. Flash the board with firmware that has
 *    CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING_INITIAL_FLASH enabled.
 * 2. Wait for the board to boot. For best results, open some terminal program
 *    to flush any data sent by the board while booting that might be already
 *    buffered by the OS serial port handling.
 * 3. Run:
 *    mcumgr --conntype serial --connstring "dev=/dev/ttyACM0,baud=115200" \
 *           fs upload SenMLCBOR /factory/provision.cbor
 * 4. Run:
 *    mcumgr --conntype serial --connstring "dev=/dev/ttyACM0,baud=115200" \
 *           fs download /factory/result.txt result.txt
 * 5. Examine the code in result.txt. If it's "0", then the operation was
 *    successful.
 * 6. Flash the board with firmware that has
 *    CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING enabled, but
 *    CONFIG_ANJAY_CLIENT_FACTORY_PROVISIONING_INITIAL_FLASH disabled.
 *
 * What happens under the hood during steps 3 and 4:
 *
 * 1. run_anjay() in main.c calls factory_flash_input_stream_create(). This
 *    initializes the fake "provision_fs" file system and mounts it at /factory.
 * 2. mcumgr starts writing the /factory/provision.cbor file. This will call:
 *    - provision_fs_stat() - that will error out; mcumgr code only does this
 *      to check if the file already exists; we return that it doesn't
 *    - provision_fs_open()
 *    - provision_fs_lseek()
 *    - provision_fs_write()
 *    - close - it's not implemented, so it's a no-op
 *    - go back to provision_fs_open() again, and repeat until the whole file is
 *      transferred
 * 3. Controlled by factory_flash_mutex and factory_flash_condvar, the data
 *    written through these write operations is passed as the stream read by
 *    anjay_factory_provision().
 * 4. Unfortunately, mcumgr API does not inform the file system driver in any
 *    way whether EOF has been reached. For that reason, we wait either for
 *    timeout or for opening the result file.
 * 5. mcumgr starts reading the /factory/result.txt file. This will call:
 *    - provision_fs_stat() - this needs to success for the read operation to
 *      succeed; we need to know the file size at this point, so this operation
 *      will use the mutex and condvar to wait until the Anjay thread calls
 *      factory_flash_finished().
 *    - provision_fs_open()
 *    - provision_fs_lseek()
 *    - provision_fs_read()
 *    - close - it's not implemented, so it's a no-op
 *    - go back to provision_fs_open() again, and repeat until the whole file is
 *      transferred
 */

// circular buffer
static char received_data[512];
static size_t received_data_start;
static size_t received_data_length;
static size_t received_data_total;

static enum {
	FACTORY_FLASH_INITIAL,
	FACTORY_FLASH_EOF,
	FACTORY_FLASH_FINISHED
} factory_flash_state;
static char factory_flash_result[AVS_INT_STR_BUF_SIZE(int)];
static size_t factory_flash_result_offset;

static K_MUTEX_DEFINE(factory_flash_mutex);
static K_CONDVAR_DEFINE(factory_flash_condvar);

#define PROVISION_FS_TYPE FS_TYPE_EXTERNAL_BASE

#define PROVISION_FS_MOUNT_POINT "/factory"
#define PROVISION_FS_FLASH_FILE PROVISION_FS_MOUNT_POINT "/provision.cbor"
#define PROVISION_FS_RESULT_FILE PROVISION_FS_MOUNT_POINT "/result.txt"

// 15 seconds of inactivity is treated as EOF
#define PROVISION_FS_UPLOAD_TIMEOUT_MS 15000

static int provision_fs_open(struct fs_file_t *filp, const char *fs_path, fs_mode_t flags)
{
	(void)filp;
	(void)fs_path;
	(void)flags;
	if (strcmp(fs_path, PROVISION_FS_FLASH_FILE) == 0) {
		if ((flags & (FS_O_READ | FS_O_WRITE)) == FS_O_WRITE &&
		    factory_flash_state == FACTORY_FLASH_INITIAL) {
			return 0;
		}
	} else if (strcmp(fs_path, PROVISION_FS_RESULT_FILE) == 0) {
		if ((flags & (FS_O_READ | FS_O_WRITE)) == FS_O_READ) {
			if (factory_flash_state < FACTORY_FLASH_FINISHED) {
				factory_flash_state = FACTORY_FLASH_EOF;
				k_condvar_broadcast(&factory_flash_condvar);
			}
			return 0;
		}
	}
	return -ENOENT;
}

static ssize_t provision_fs_read(struct fs_file_t *filp, void *dest, size_t nbytes)
{
	// Read provisioning result
	(void)filp;
	ssize_t result = k_mutex_lock(&factory_flash_mutex, K_FOREVER);

	if (result) {
		return result;
	}

	while (factory_flash_state != FACTORY_FLASH_FINISHED) {
		k_condvar_wait(&factory_flash_condvar, &factory_flash_mutex, K_FOREVER);
	}

	size_t bytes_to_copy = strlen(factory_flash_result) - factory_flash_result_offset;

	if (nbytes < bytes_to_copy) {
		bytes_to_copy = nbytes;
	}

	memcpy((char *)dest, factory_flash_result + factory_flash_result_offset, bytes_to_copy);

	k_mutex_unlock(&factory_flash_mutex);

	return bytes_to_copy;
}

static ssize_t provision_fs_write(struct fs_file_t *filp, const void *src, size_t nbytes)
{
	// Write the actual provisioning data
	(void)filp;
	if (factory_flash_state != FACTORY_FLASH_INITIAL) {
		return -EBADF;
	}

	if (nbytes == 0) {
		return 0;
	}

	if (nbytes > sizeof(received_data)) {
		return -ENOSPC;
	}

	int result = k_mutex_lock(&factory_flash_mutex, K_FOREVER);

	if (result) {
		return result;
	}

	const char *src_ptr = (const char *)src;
	const char *const src_end = src_ptr + nbytes;

	while (src_ptr < src_end) {
		size_t write_start =
			(received_data_start + received_data_length) % sizeof(received_data);
		size_t immediate_size = AVS_MIN(
			AVS_MIN(sizeof(received_data) - received_data_length, src_end - src_ptr),
			sizeof(received_data) - write_start);

		if (!immediate_size) {
			k_condvar_wait(&factory_flash_condvar, &factory_flash_mutex, K_FOREVER);
		} else {
			memcpy(received_data + write_start, src_ptr, immediate_size);
			received_data_length += immediate_size;
			received_data_total += immediate_size;
			src_ptr += immediate_size;
			k_condvar_broadcast(&factory_flash_condvar);
		}
	}

	k_mutex_unlock(&factory_flash_mutex);

	return nbytes;
}

static int provision_fs_lseek(struct fs_file_t *filp, off_t off, int whence)
{
	(void)filp;
	(void)off;
	(void)whence;
	assert(whence == FS_SEEK_SET);
	if (filp->flags & FS_O_WRITE) {
		// The provisioning file
		assert(off == (size_t)received_data_total);
	} else {
		// The status file
		if (off > strlen(factory_flash_result)) {
			return -ENXIO;
		}
		factory_flash_result_offset = off;
	}
	return 0;
}

static int provision_fs_mount(struct fs_mount_t *mountp)
{
	(void)mountp;
	return 0;
}

static int provision_fs_unlink(struct fs_mount_t *mountp, const char *name)
{
	(void)mountp;
	(void)name;
	return 0;
}

static int provision_fs_stat(struct fs_mount_t *mountp, const char *path, struct fs_dirent *entry)
{
	(void)mountp;
	(void)path;
	(void)entry;
	if (strcmp(path, PROVISION_FS_RESULT_FILE) != 0) {
		return -ENOENT;
	}

	int result = k_mutex_lock(&factory_flash_mutex, K_FOREVER);

	if (!result) {
		if (factory_flash_state == FACTORY_FLASH_INITIAL) {
			factory_flash_state = FACTORY_FLASH_EOF;
			k_condvar_broadcast(&factory_flash_condvar);
		}

		while (factory_flash_state != FACTORY_FLASH_FINISHED) {
			k_condvar_wait(&factory_flash_condvar, &factory_flash_mutex, K_FOREVER);
		}

		entry->type = FS_DIR_ENTRY_FILE;
		entry->size = strlen(factory_flash_result);

		k_mutex_unlock(&factory_flash_mutex);
	}

	return result;
}

static const struct fs_file_system_t provision_fs = { .open = provision_fs_open,
						      .read = provision_fs_read,
						      .write = provision_fs_write,
						      .lseek = provision_fs_lseek,
						      .mount = provision_fs_mount,
						      .unlink = provision_fs_unlink,
						      .stat = provision_fs_stat };

static struct fs_mount_t provision_fs_mount_point = { .type = PROVISION_FS_TYPE,
						      .mnt_point = PROVISION_FS_MOUNT_POINT };

static avs_error_t provision_stream_read(avs_stream_t *stream, size_t *out_bytes_read,
					 bool *out_message_finished, void *buffer,
					 size_t buffer_length)
{
	(void)stream;
	assert(out_bytes_read);

	if (buffer_length == 0) {
		*out_bytes_read = 0;
		if (out_message_finished) {
			*out_message_finished = false;
		}
		return AVS_OK;
	}

	int result = k_mutex_lock(&factory_flash_mutex, K_FOREVER);

	if (result) {
		return avs_errno(avs_map_errno(-result));
	}

	k_timeout_t timeout = K_FOREVER;

	if (received_data_total > 0) {
		// Some data arrived, so let's treat timeout as EOF
		int64_t uptime = k_uptime_get();

		timeout = K_TIMEOUT_ABS_MS(uptime + PROVISION_FS_UPLOAD_TIMEOUT_MS);
	}

	while (!received_data_length && factory_flash_state == FACTORY_FLASH_INITIAL &&
	       (K_TIMEOUT_EQ(timeout, K_FOREVER) || k_uptime_ticks() < Z_TICK_ABS(timeout.ticks))) {
		k_condvar_wait(&factory_flash_condvar, &factory_flash_mutex, timeout);
	}

	*out_bytes_read = AVS_MIN(AVS_MIN(received_data_length, buffer_length),
				  sizeof(received_data) - received_data_start);
	memcpy(buffer, received_data + received_data_start, *out_bytes_read);
	received_data_start = (received_data_start + *out_bytes_read) % sizeof(received_data);
	received_data_length -= *out_bytes_read;

	k_mutex_unlock(&factory_flash_mutex);

	if (out_message_finished) {
		*out_message_finished = (*out_bytes_read == 0);
	}

	return AVS_OK;
}

static const avs_stream_v_table_t provision_stream_vtable = { .read = provision_stream_read };

static struct {
	const avs_stream_v_table_t *const vtable;
} provision_stream = { .vtable = &provision_stream_vtable };

avs_stream_t *factory_flash_input_stream_create(void)
{
	if (fs_register(PROVISION_FS_TYPE, &provision_fs) || fs_mount(&provision_fs_mount_point)) {
		return NULL;
	}

	fs_mgmt_register_group();

	return (avs_stream_t *)&provision_stream;
}

void factory_flash_finished(int result)
{
	k_mutex_lock(&factory_flash_mutex, K_FOREVER);
	avs_simple_snprintf(factory_flash_result, sizeof(factory_flash_result), "%d", result);
	factory_flash_state = FACTORY_FLASH_FINISHED;
	k_condvar_broadcast(&factory_flash_condvar);
	k_mutex_unlock(&factory_flash_mutex);
}
