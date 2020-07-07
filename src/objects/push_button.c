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

#include <assert.h>
#include <stdbool.h>

#include <anjay/anjay.h>
#include <avsystem/commons/avs_defs.h>
#include <avsystem/commons/avs_list.h>
#include <avsystem/commons/avs_memory.h>

#include <devicetree.h>
#include <drivers/gpio.h>

#define SW0_NODE DT_ALIAS(sw0)

#define FLAGS_OR_ZERO(node)                          \
    COND_CODE_1(DT_PHA_HAS_CELL(node, gpios, flags), \
                (DT_GPIO_FLAGS(node, gpios)), (0))

#define SW0_GPIO_LABEL DT_GPIO_LABEL(SW0_NODE, gpios)
#define SW0_GPIO_PIN DT_GPIO_PIN(SW0_NODE, gpios)
#define SW0_GPIO_FLAGS (GPIO_INPUT | DT_GPIO_FLAGS(SW0_NODE, gpios))

/**
 * Digital Input State: R, Single, Mandatory
 * type: boolean, range: N/A, unit: N/A
 * The current state of a digital input.
 */
#define RID_DIGITAL_INPUT_STATE 5500

/**
 * Digital Input Counter: R, Single, Optional
 * type: integer, range: N/A, unit: N/A
 * The cumulative value of active state detected.
 */
#define RID_DIGITAL_INPUT_COUNTER 5501

/**
 * Application Type: RW, Single, Optional
 * type: string, range: N/A, unit: N/A
 * The application type of the sensor or actuator as a string, for
 * instance, "Air Pressure".
 */
#define RID_APPLICATION_TYPE 5750

typedef struct push_button_object_struct {
    const anjay_dm_object_def_t *def;

    struct device *dev;

    struct gpio_callback push_button_callback;

    bool digital_input_state;
    int digital_input_counter;
    bool digital_input_counter_changed;
    char application_type[128];
} push_button_object_t;

static void
button_pressed(struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    push_button_object_t *obj =
            AVS_CONTAINER_OF(cb, push_button_object_t, push_button_callback);
    obj->digital_input_counter++;
    obj->digital_input_counter_changed = true;
}

static inline push_button_object_t *
get_obj(const anjay_dm_object_def_t *const *obj_ptr) {
    assert(obj_ptr);
    return AVS_CONTAINER_OF(obj_ptr, push_button_object_t, def);
}

static int list_resources(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *obj_ptr,
                          anjay_iid_t iid,
                          anjay_dm_resource_list_ctx_t *ctx) {
    (void) anjay;
    (void) obj_ptr;
    (void) iid;

    anjay_dm_emit_res(ctx, RID_DIGITAL_INPUT_STATE, ANJAY_DM_RES_R,
                      ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_DIGITAL_INPUT_COUNTER, ANJAY_DM_RES_R,
                      ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_APPLICATION_TYPE, ANJAY_DM_RES_RW,
                      ANJAY_DM_RES_PRESENT);
    return 0;
}

static int resource_read(anjay_t *anjay,
                         const anjay_dm_object_def_t *const *obj_ptr,
                         anjay_iid_t iid,
                         anjay_rid_t rid,
                         anjay_riid_t riid,
                         anjay_output_ctx_t *ctx) {
    (void) anjay;

    push_button_object_t *obj = get_obj(obj_ptr);
    assert(obj);
    assert(iid == 0);

    switch (rid) {
    case RID_DIGITAL_INPUT_STATE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_bool(ctx, obj->digital_input_state);

    case RID_DIGITAL_INPUT_COUNTER:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_i32(ctx, obj->digital_input_counter);

    case RID_APPLICATION_TYPE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_string(ctx, obj->application_type);

    default:
        return ANJAY_ERR_METHOD_NOT_ALLOWED;
    }
}

static int resource_write(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *obj_ptr,
                          anjay_iid_t iid,
                          anjay_rid_t rid,
                          anjay_riid_t riid,
                          anjay_input_ctx_t *ctx) {
    (void) anjay;

    push_button_object_t *obj = get_obj(obj_ptr);
    assert(obj);
    assert(iid == 0);

    switch (rid) {
    case RID_APPLICATION_TYPE: {
        assert(riid == ANJAY_ID_INVALID);
        return anjay_get_string(ctx, obj->application_type,
                                sizeof(obj->application_type));
    }

    default:
        return ANJAY_ERR_METHOD_NOT_ALLOWED;
    }
}

static const anjay_dm_object_def_t OBJ_DEF = {
    .oid = 3347,
    .handlers = {
        .list_instances = anjay_dm_list_instances_SINGLE,
        .list_resources = list_resources,
        .resource_read = resource_read,
        .resource_write = resource_write,

        .transaction_begin = anjay_dm_transaction_NOOP,
        .transaction_validate = anjay_dm_transaction_NOOP,
        .transaction_commit = anjay_dm_transaction_NOOP,
        .transaction_rollback = anjay_dm_transaction_NOOP
    }
};

const anjay_dm_object_def_t **push_button_object_create(void) {
    struct device *dev = device_get_binding(SW0_GPIO_LABEL);
    if (!dev) {
        return NULL;
    }

    if (gpio_pin_configure(dev, SW0_GPIO_PIN, SW0_GPIO_FLAGS)) {
        return NULL;
    }

    if (gpio_pin_interrupt_configure(dev, SW0_GPIO_PIN,
                                     GPIO_INT_EDGE_TO_ACTIVE)) {
        return NULL;
    }

    push_button_object_t *obj =
            (push_button_object_t *) avs_calloc(1,
                                                sizeof(push_button_object_t));
    if (!obj) {
        gpio_pin_interrupt_configure(obj->dev, SW0_GPIO_PIN, GPIO_INT_DISABLE);
        return NULL;
    }

    obj->def = &OBJ_DEF;
    obj->dev = dev;
    obj->digital_input_counter = 0;
    obj->digital_input_counter_changed = false;
    obj->digital_input_state = gpio_pin_get(obj->dev, SW0_GPIO_PIN);

    gpio_init_callback(&obj->push_button_callback, button_pressed,
                       BIT(SW0_GPIO_PIN));
    if (gpio_add_callback(dev, &obj->push_button_callback)) {
        gpio_pin_interrupt_configure(obj->dev, SW0_GPIO_PIN, GPIO_INT_DISABLE);
        avs_free(obj);
        return NULL;
    }

    return &obj->def;
}

void push_button_object_release(const anjay_dm_object_def_t **def) {
    if (def) {
        push_button_object_t *obj = get_obj(def);
        gpio_pin_interrupt_configure(obj->dev, SW0_GPIO_PIN, GPIO_INT_DISABLE);
        gpio_remove_callback(obj->dev, &obj->push_button_callback);
        avs_free(obj);
    }
}

void push_button_object_update(anjay_t *anjay,
                               const anjay_dm_object_def_t *const *def) {
    if (!anjay || !def) {
        return;
    }

    push_button_object_t *obj = get_obj(def);

    if (obj->digital_input_counter_changed) {
        obj->digital_input_counter_changed = false;
        (void) anjay_notify_changed(anjay, obj->def->oid, 0,
                                    RID_DIGITAL_INPUT_COUNTER);
    }

    int button_state = gpio_pin_get(obj->dev, SW0_GPIO_PIN);
    if (button_state >= 0 && button_state != obj->digital_input_state) {
        obj->digital_input_state = button_state;
        (void) anjay_notify_changed(anjay, obj->def->oid, 0,
                                    RID_DIGITAL_INPUT_STATE);
    }
}
