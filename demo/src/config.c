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

#include <stdio.h>

#include <avsystem/commons/avs_log.h>
#include <avsystem/commons/avs_time.h>
#include <avsystem/commons/avs_utils.h>

#include <device.h>
#include <shell/shell.h>
#include <shell/shell_uart.h>
#include <zephyr.h>

#include <console/console.h>
#include <drivers/flash.h>
#include <drivers/hwinfo.h>
#include <drivers/uart.h>
#include <fs/fs.h>
#include <fs/littlefs.h>
#include <storage/flash_map.h>

#include "config.h"
#include "default_config.h"
#include "utils.h"

#define MOUNT_POINT "/lfs"
#define CONFIG_FILE_NAME "config"
#define CONFIG_FILE_PATH (MOUNT_POINT "/" CONFIG_FILE_NAME)

#define EP_NAME_PREFIX "anjay-zephyr-demo"

// storage is defined in devicetree
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);

typedef struct {
#ifdef CONFIG_WIFI
    char ssid[32];
    char password[32];
#endif // CONFIG_WIFI
    char uri[128];
    char ep_name[64];
    char psk[32];
    char bootstrap[2];
} app_config_t;

static app_config_t APP_CONFIG;

typedef struct {
    const char *const desc;
    char *const value;
    size_t value_capacity;
    bool flag;
} option_t;

static option_t STRING_OPTIONS[] = {
#ifdef CONFIG_WIFI
    [OPTION_SSID] = { "Wi-Fi SSID", APP_CONFIG.ssid, sizeof(APP_CONFIG.ssid) },
    [OPTION_PASSWORD] = { "Wi-Fi password", APP_CONFIG.password,
                          sizeof(APP_CONFIG.password) },
#endif // CONFIG_WIFI
    [OPTION_URI] = { "LwM2M Server URI", APP_CONFIG.uri,
                     sizeof(APP_CONFIG.uri) },
    [OPTION_EP_NAME] = { "Endpoint name", APP_CONFIG.ep_name,
                         sizeof(APP_CONFIG.ep_name) },
    [OPTION_PSK] = { "PSK", APP_CONFIG.psk, sizeof(APP_CONFIG.psk) },
    [OPTION_BOOTSTRAP] = { "Bootstrap", APP_CONFIG.bootstrap,
                           sizeof(APP_CONFIG.bootstrap), true }
};

void config_print_summary(const struct shell *shell) {
    shell_print(shell, "\nCurrent Anjay config:\n");
    int id = 0;
    for (int i = 0; i < _OPTION_STRING_END; i++) {
        id++;
        shell_print(shell, " %s: %s", STRING_OPTIONS[i].desc,
                    STRING_OPTIONS[i].value);
    }
}

static int
write_to_file(struct fs_file_t *file, const void *value, size_t value_len) {
    return (fs_write(file, value, value_len) == value_len) ? 0 : -1;
}

static int open_config_file(struct fs_file_t *file, fs_mode_t flags) {
    fs_file_t_init(file);
    if (fs_open(file, CONFIG_FILE_PATH, flags)) {
        avs_log(fs, ERROR, "Failed to open %s", CONFIG_FILE_PATH);
        return -1;
    }
    return 0;
}

static int write_config_to_flash(void) {
    // If file isn't removed before the write operation, sometimes it may be in
    // invalid state after call to fs_close().
    fs_unlink(CONFIG_FILE_PATH);

    struct fs_file_t file;
    if (open_config_file(&file, FS_O_WRITE | FS_O_CREATE)) {
        return -1;
    }

    for (int i = 0; i < _OPTION_STRING_END; i++) {
        size_t opt_len = strlen(STRING_OPTIONS[i].value);
        if (write_to_file(&file, &i, sizeof(i))
                || write_to_file(&file, &opt_len, sizeof(opt_len))
                || write_to_file(&file, STRING_OPTIONS[i].value, opt_len)) {
            fs_close(&file);
            fs_unlink(CONFIG_FILE_PATH);
            return -1;
        }
    }

    return fs_close(&file) ? -1 : 0;
}

void config_save(const struct shell *shell) {
    struct fs_mount_t lfs_storage_mnt = {
        .type = FS_LITTLEFS,
        .fs_data = &storage,
        .storage_dev = (void *) FLASH_AREA_ID(storage),
        .mnt_point = MOUNT_POINT,
    };
    struct fs_mount_t *mp = &lfs_storage_mnt;

    if (fs_mount(mp) || write_config_to_flash()) {
        shell_warn(shell, "Cannot save the config");
    } else {
        shell_print(shell, "Configuration successfuly saved");
    }

    fs_unmount(mp);
}

typedef enum { READ_SUCCESS, READ_EOF, READ_ERROR } read_result_t;

static read_result_t
read_from_file(struct fs_file_t *file, void *value, size_t value_len) {
    ssize_t result = fs_read(file, value, value_len);
    if (result == value_len) {
        return READ_SUCCESS;
    } else if (result == 0) {
        return READ_EOF;
    } else if (result > 0) {
        avs_log(fs, ERROR, "Unexpected end of file");
        return READ_ERROR;
    } else { // result < 0
        avs_log(fs, ERROR, "Read error: %d", (int) result);
        return READ_ERROR;
    }
}

static read_result_t read_option_id_from_file(struct fs_file_t *file,
                                              option_id_t *out_id) {
    int id;
    read_result_t result = read_from_file(file, &id, sizeof(id));
    if (result != READ_SUCCESS) {
        return result;
    }

    if (id < 0 || id >= _OPTION_STRING_END) {
        avs_log(fs, ERROR, "Invalid persisted option ID");
        return READ_ERROR;
    }

    *out_id = id;
    return READ_SUCCESS;
}

static read_result_t read_option_value_from_file(struct fs_file_t *file,
                                                 option_id_t id) {
    size_t opt_size;
    read_result_t result = read_from_file(file, &opt_size, sizeof(opt_size));
    if (result != READ_SUCCESS) {
        return result;
    }

    if (STRING_OPTIONS[id].value_capacity <= opt_size) {
        avs_log(fs, ERROR, "Persisted value too long");
        return READ_ERROR;
    }

    result = read_from_file(file, STRING_OPTIONS[id].value, opt_size);
    if (result == READ_SUCCESS) {
        STRING_OPTIONS[id].value[opt_size] = '\0';
    }

    return result;
}

// Returns 0 if the configuration is in consistent state and -1 if it should be
// replaced by default settings.
static int read_config_from_flash(void) {
    struct fs_dirent entry;
    if (fs_stat(CONFIG_FILE_PATH, &entry) || entry.size == 0) {
        avs_log(fs, WARNING, "%s doesn't exist", CONFIG_FILE_PATH);
        return 0;
    }

    struct fs_file_t file;
    if (open_config_file(&file, FS_O_READ)) {
        return -1;
    }

    int result = 0;
    while (1) {
        option_id_t id = 0;
        read_result_t read_result = read_option_id_from_file(&file, &id);
        if (read_result == READ_ERROR) {
            result = -1;
            break;
        } else if (read_result == READ_EOF) {
            // EOF is expected here
            result = 0;
            break;
        }

        read_result = read_option_value_from_file(&file, id);
        if (read_result != READ_SUCCESS) {
            // EOF here means invalid persisted configuration
            result = -1;
            break;
        }
    }

    fs_close(&file);
    return result;
}

void config_default_init(void) {
    APP_CONFIG = (app_config_t) {
#ifdef CONFIG_WIFI
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
#endif // CONFIG_WIFI
        .uri = SERVER_URI,
        .psk = PSK_KEY,
        .bootstrap = BOOTSTRAP
    };

    device_id_t id;
    AVS_STATIC_ASSERT(sizeof(APP_CONFIG.ep_name)
                              >= sizeof(id.value) + sizeof(EP_NAME_PREFIX)
                                         - sizeof('\0') + sizeof('-'),
                      ep_name_buffer_too_small);
    if (!get_device_id(&id)) {
        (void) avs_simple_snprintf(APP_CONFIG.ep_name,
                                   sizeof(APP_CONFIG.ep_name),
                                   EP_NAME_PREFIX "-%s",
                                   id.value);
    } else {
        memcpy(APP_CONFIG.ep_name, EP_NAME_PREFIX, sizeof(EP_NAME_PREFIX));
    }
}

void config_init(const struct shell *shell) {
    config_default_init();

    struct fs_mount_t lfs_storage_mnt = {
        .type = FS_LITTLEFS,
        .fs_data = &storage,
        .storage_dev = (void *) FLASH_AREA_ID(storage),
        .mnt_point = MOUNT_POINT,
    };

    struct fs_mount_t *mp = &lfs_storage_mnt;

    if (fs_mount(mp)) {
        shell_warn(shell,
                   "Failed to mount %s, config persistence disabled",
                   mp->mnt_point);
        mp = NULL;
    } else if (read_config_from_flash()) {
        shell_warn(shell, "Restoring default configuration");
        config_default_init();
    } else {
        shell_print(shell, "Configuration successfuly restored");
    }

    if (mp) {
        fs_unmount(mp);
    }
}

int config_set_option(const struct shell *shell,
                      size_t argc,
                      char **argv,
                      option_id_t option) {
    if (argc != 2) {
        shell_error(shell, "Wrong number of arguments.\n");
    }

    const char *value = argv[1];
    size_t value_len = strlen(value) + 1;

    if (STRING_OPTIONS[option].flag) {
        if (value_len != STRING_OPTIONS[option].value_capacity
                || (value[0] != 'y' && value[0] != 'n')) {
            shell_print(shell, "Value invalid, 'y' or 'n' is allowed\n");
            return -1;
        }
    } else if (value_len > STRING_OPTIONS[option].value_capacity) {
        shell_print(shell, "Value too long, maximum length is %d\n",
                    STRING_OPTIONS[option].value_capacity - 1);
        return -1;
    }

    memcpy(STRING_OPTIONS[option].value, value, value_len);

    return 0;
}

const char *config_get_endpoint_name(void) {
    return APP_CONFIG.ep_name;
}

#ifdef CONFIG_WIFI
const char *config_get_wifi_ssid(void) {
    return APP_CONFIG.ssid;
}

const char *config_get_wifi_password(void) {
    return APP_CONFIG.password;
}
#endif // CONFIG_WIFI

const char *config_get_server_uri(void) {
    return APP_CONFIG.uri;
}

const char *config_get_psk(void) {
    return APP_CONFIG.psk;
}

bool config_is_bootstrap(void) {
    return APP_CONFIG.bootstrap[0] == 'y';
}
