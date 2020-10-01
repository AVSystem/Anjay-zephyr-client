/*
 * Copyright 2020 AVSystem <avsystem@avsystem.com>
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
#include <zephyr.h>

#include <console/console.h>
#include <drivers/flash.h>
#include <drivers/hwinfo.h>
#include <drivers/uart.h>
#include <fs/fs.h>
#include <fs/littlefs.h>
#include <storage/flash_map.h>

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
} app_config_t;

static app_config_t APP_CONFIG;

// To preserve backward compatibility, do not remove any of these and add new
// options IDs right before _OPTION_STRING_END.
typedef enum {
#ifdef CONFIG_WIFI
    OPTION_SSID,
    OPTION_PASSWORD,
#endif // CONFIG_WIFI
    OPTION_URI,
    OPTION_EP_NAME,
    OPTION_PSK,
    _OPTION_STRING_END
} option_string_id_t;

typedef enum {
    OPTION_FACTORY_RESET,
    OPTION_EXIT,
    _OPTION_CONTROL_END
} option_control_id_t;

typedef struct {
    const char *const desc;
    char *const value;
    size_t value_capacity;
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
    [OPTION_PSK] = { "PSK", APP_CONFIG.psk, sizeof(APP_CONFIG.psk) }
};

static option_t CONTROL_OPTIONS[] = {
    [OPTION_FACTORY_RESET] = { "Reset to defaults", NULL, 0 },
    [OPTION_EXIT] = { "Save and exit", NULL, 0 }
};

static int get_value(option_string_id_t id) {
    printf("Enter value:\n");
    char *value = console_getline();
    // +1 for the NULL-byte
    size_t value_len = strlen(value) + 1;

    if (value_len > STRING_OPTIONS[id].value_capacity) {
        printf("Value too long, maximum length is %d\n",
               STRING_OPTIONS[id].value_capacity - 1);
        return -1;
    }

    memcpy(STRING_OPTIONS[id].value, value, value_len);
    return 0;
}

static void print_menu(void) {
    printf("\nAvailable options:\n");
    int id = 0;
    for (int i = 0; i < _OPTION_STRING_END; i++) {
        id++;
        printf("%d. %s: %s\n",
               id,
               STRING_OPTIONS[i].desc,
               STRING_OPTIONS[i].value);
    }

    for (int i = 0; i < _OPTION_CONTROL_END; i++) {
        id++;
        printf("%d. %s\n", id, CONTROL_OPTIONS[i].desc);
    }

    printf("\nSelect option (1 - %d):\n", id);
}

static int
write_to_file(struct fs_file_t *file, const void *value, size_t value_len) {
    return (fs_write(file, value, value_len) == value_len) ? 0 : -1;
}

static int write_config_to_flash(void) {
    // If file isn't removed before the write operation, sometimes it may be in
    // invalid state after call to fs_close().
    fs_unlink(CONFIG_FILE_PATH);

    struct fs_file_t file;
    if (fs_open(&file, CONFIG_FILE_PATH)) {
        avs_log(fs, ERROR, "Failed to open %s", CONFIG_FILE_PATH);
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
                                              option_string_id_t *out_id) {
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
                                                 option_string_id_t id) {
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
    if (fs_open(&file, CONFIG_FILE_PATH)) {
        avs_log(fs, ERROR, "Failed to open %s", CONFIG_FILE_PATH);
        return 0;
    }

    int result = 0;
    while (1) {
        option_string_id_t id = 0;
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

static bool get_confirmation(void) {
    printf("Are you sure? ('y' to confirm)\n");
    char *selection_str = console_getline();
    return strlen(selection_str) == 1 && selection_str[0] == 'y';
}

typedef struct {
    struct {
        bool set;
        option_string_id_t id;
    } string_option;
    struct {
        bool set;
        option_control_id_t id;
    } control_option;
} option_selection_t;

static int get_option_selection(option_selection_t *selection) {
    memset(selection, 0, sizeof(*selection));
    char *selection_str = console_getline();
    int id;
    if (sscanf(selection_str, "%d", &id) != 1) {
        return -1;
    }

    if (id < 1 || id > _OPTION_STRING_END + _OPTION_CONTROL_END) {
        return -1;
    }

    const int menu_option_id_offset = 1;
    id -= menu_option_id_offset;

    if (id < _OPTION_STRING_END) {
        selection->string_option.set = true;
        selection->string_option.id = id;
    } else {
        selection->control_option.set = true;
        selection->control_option.id = id - _OPTION_STRING_END;
    }

    return 0;
}

static void default_config_init(void) {
    APP_CONFIG = (app_config_t) {
#ifdef CONFIG_WIFI
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
#endif // CONFIG_WIFI
        .uri = SERVER_URI,
        .psk = PSK_KEY
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

static void enter_menu(void) {
    console_getline_init();
    // Save flash cycles by performing write only if it's possible that
    // something was changed.
    bool changed = false;

    while (true) {
        print_menu();

        option_selection_t selection;
        if (get_option_selection(&selection)) {
            printf("Invalid choice\n");
            continue;
        }

        if (selection.string_option.set) {
            if (!get_value(selection.string_option.id)) {
                changed = true;
            }
        } else {
            if (selection.control_option.id == OPTION_FACTORY_RESET) {
                if (get_confirmation()) {
                    default_config_init();
                    changed = true;
                }
            } else if (selection.control_option.id == OPTION_EXIT) {
                if (changed && write_config_to_flash()) {
                    avs_log(fs, WARNING, "Failed to persist configuration");
                }
                return;
            }
        }
    }
}

void config_init(void) {
    default_config_init();

    struct fs_mount_t lfs_storage_mnt = {
        .type = FS_LITTLEFS,
        .fs_data = &storage,
        .storage_dev = (void *) FLASH_AREA_ID(storage),
        .mnt_point = MOUNT_POINT,
    };
    struct fs_mount_t *mp = &lfs_storage_mnt;

    if (fs_mount(mp)) {
        avs_log(fs,
                WARNING,
                "Failed to mount %s, config persistence disabled",
                mp->mnt_point);
        mp = NULL;
    } else if (read_config_from_flash()) {
        avs_log(fs, WARNING, "Restoring default configuration");
        default_config_init();
    }

    struct device *uart = device_get_binding(CONFIG_UART_CONSOLE_ON_DEV_NAME);
    if (!uart) {
        avs_log(menu,
                WARNING,
                "Failed to get %s binding",
                CONFIG_UART_CONSOLE_ON_DEV_NAME);
        return;
    }

    printf("Press any key to enter config menu...\n");
    const avs_time_real_t time_limit =
            avs_time_real_add(avs_time_real_now(),
                              avs_time_duration_from_scalar(3, AVS_TIME_S));

    while (avs_time_real_before(avs_time_real_now(), time_limit)) {
        char byte;
        if (!uart_poll_in(uart, &byte)) {
            enter_menu();
            break;
        }
        k_sleep(K_MSEC(10));
    }

    if (mp) {
        fs_unmount(mp);
    }
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
