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

#include <assert.h>
#include <stdbool.h>

#include <anjay/anjay.h>
#include <avsystem/commons/avs_defs.h>
#include <avsystem/commons/avs_list.h>
#include <avsystem/commons/avs_memory.h>

#include <devicetree.h>
#include <drivers/gpio.h>

#include "objects.h"

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

#if PUSH_BUTTON_AVAILABLE_ANY
typedef struct push_button_instance_struct {
    anjay_iid_t iid;

    const struct device *dev;
    struct gpio_callback push_button_callback;
    int gpio_pin;

    bool digital_input_state;
    int digital_input_counter;
    bool digital_input_counter_changed;
} push_button_instance_t;

typedef struct push_button_object_struct {
    const anjay_dm_object_def_t *def;
    AVS_LIST(push_button_instance_t) instances;
} push_button_object_t;

static void button_pressed(const struct device *dev,
                           struct gpio_callback *cb,
                           uint32_t pins) {
    push_button_instance_t *inst =
            AVS_CONTAINER_OF(cb, push_button_instance_t, push_button_callback);
    inst->digital_input_counter++;
    inst->digital_input_counter_changed = true;
}

static inline push_button_object_t *
get_obj(const anjay_dm_object_def_t *const *obj_ptr) {
    assert(obj_ptr);
    return AVS_CONTAINER_OF(obj_ptr, push_button_object_t, def);
}

static push_button_instance_t *find_instance(const push_button_object_t *obj,
                                             anjay_iid_t iid) {
    AVS_LIST(push_button_instance_t) it;
    AVS_LIST_FOREACH(it, obj->instances) {
        if (it->iid == iid) {
            return it;
        } else if (it->iid > iid) {
            break;
        }
    }
    return NULL;
}

static int list_instances(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *obj_ptr,
                          anjay_dm_list_ctx_t *ctx) {
    (void) anjay;

    AVS_LIST(push_button_instance_t) it;
    AVS_LIST_FOREACH(it, get_obj(obj_ptr)->instances) {
        anjay_dm_emit(ctx, it->iid);
    }

    return 0;
}

static int init_instance(push_button_instance_t *inst, anjay_iid_t iid) {
    assert(iid != ANJAY_ID_INVALID);

    inst->iid = iid;
    inst->digital_input_counter = 0;
    inst->digital_input_counter_changed = false;
    return 0;
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
    push_button_instance_t *inst = find_instance(obj, iid);
    assert(inst);

    switch (rid) {
    case RID_DIGITAL_INPUT_STATE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_bool(ctx, inst->digital_input_state);

    case RID_DIGITAL_INPUT_COUNTER:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_i32(ctx, inst->digital_input_counter);

    default:
        return ANJAY_ERR_METHOD_NOT_ALLOWED;
    }
}

static push_button_instance_t *add_instance(push_button_object_t *obj,
                                            anjay_iid_t iid) {
    assert(find_instance(obj, iid) == NULL);

    AVS_LIST(push_button_instance_t) created =
            AVS_LIST_NEW_ELEMENT(push_button_instance_t);
    if (!created) {
        return NULL;
    }

    int result = init_instance(created, iid);
    if (result) {
        AVS_LIST_CLEAR(&created);
        return NULL;
    }

    AVS_LIST(push_button_instance_t) *ptr;
    AVS_LIST_FOREACH_PTR(ptr, &obj->instances) {
        if ((*ptr)->iid > created->iid) {
            break;
        }
    }

    AVS_LIST_INSERT(ptr, created);
    return created;
}

static int configure_push_button(push_button_object_t *obj,
                                 const struct device *dev,
                                 int gpio_pin,
                                 int gpio_flags,
                                 anjay_iid_t iid) {
    if (!device_is_ready(dev) || gpio_pin_configure(dev, gpio_pin, gpio_flags)
            || gpio_pin_interrupt_configure(dev, gpio_pin,
                                            GPIO_INT_EDGE_TO_ACTIVE)) {
        return -1;
    }

    AVS_LIST(push_button_instance_t) inst = add_instance(obj, iid);
    if (inst) {
        inst->dev = dev;
        inst->digital_input_state = gpio_pin_get(inst->dev, gpio_pin);
        inst->gpio_pin = gpio_pin;
    } else {
        return -1;
    }

    gpio_init_callback(&inst->push_button_callback, button_pressed,
                       BIT(gpio_pin));
    if (gpio_add_callback(inst->dev, &inst->push_button_callback)) {
        gpio_pin_interrupt_configure(inst->dev, inst->gpio_pin,
                                     GPIO_INT_DISABLE);
        AVS_LIST_DELETE(&inst);
        return -1;
    }
    return 0;
}

static const anjay_dm_object_def_t OBJ_DEF = {
    .oid = 3347,
    .handlers = {
        .list_instances = list_instances,
        .list_resources = list_resources,
        .resource_read = resource_read
    }
};

static const anjay_dm_object_def_t *OBJ_DEF_PTR = &OBJ_DEF;

void push_button_object_release(const anjay_dm_object_def_t ***out_def) {
    const anjay_dm_object_def_t **def = *out_def;
    if (def) {
        push_button_object_t *obj = get_obj(def);
        AVS_LIST_CLEAR(&obj->instances) {
            gpio_pin_interrupt_configure(obj->instances->dev,
                                         obj->instances->gpio_pin,
                                         GPIO_INT_DISABLE);
            gpio_remove_callback(obj->instances->dev,
                                 &obj->instances->push_button_callback);
        }
        avs_free(obj);
        *out_def = NULL;
    }
}

const anjay_dm_object_def_t **push_button_object_create(void) {
    push_button_object_t *obj =
            (push_button_object_t *) avs_calloc(1,
                                                sizeof(push_button_object_t));
    if (!obj) {
        return NULL;
    }
    obj->def = OBJ_DEF_PTR;

#    define CONFIGURE(idx)                                                  \
        configure_push_button(                                              \
                obj,                                                        \
                DEVICE_DT_GET(DT_GPIO_CTLR(PUSH_BUTTON_NODE(idx), gpios)),  \
                DT_GPIO_PIN(PUSH_BUTTON_NODE(idx), gpios),                  \
                (GPIO_INPUT | DT_GPIO_FLAGS(PUSH_BUTTON_NODE(idx), gpios)), \
                idx)

#    if PUSH_BUTTON_AVAILABLE(0)
    CONFIGURE(0);
#    endif // PUSH_BUTTON_AVAILABLE(0)
#    if PUSH_BUTTON_AVAILABLE(1)
    CONFIGURE(1);
#    endif // PUSH_BUTTON_AVAILABLE(1)
#    if PUSH_BUTTON_AVAILABLE(2)
    CONFIGURE(2);
#    endif // PUSH_BUTTON_AVAILABLE(2)

#    undef CONFIGURE

    if (!obj->instances) {
        const anjay_dm_object_def_t **out_def = &OBJ_DEF_PTR;
        push_button_object_release(&out_def);
        return NULL;
    }

    return &obj->def;
}

void push_button_object_update(anjay_t *anjay,
                               const anjay_dm_object_def_t *const *def) {
    if (!anjay || !def) {
        return;
    }

    push_button_object_t *obj = get_obj(def);

    AVS_LIST(push_button_instance_t) inst;
    AVS_LIST_FOREACH(inst, obj->instances) {
        if (inst->digital_input_counter_changed) {
            inst->digital_input_counter_changed = false;
            (void) anjay_notify_changed(anjay, obj->def->oid, inst->iid,
                                        RID_DIGITAL_INPUT_COUNTER);
        }

        int button_state = gpio_pin_get(inst->dev, inst->gpio_pin);
        if (button_state >= 0 && button_state != inst->digital_input_state) {
            inst->digital_input_state = button_state;
            (void) anjay_notify_changed(anjay, obj->def->oid, inst->iid,
                                        RID_DIGITAL_INPUT_STATE);
        }
    }
}
#else  // PUSH_BUTTON_AVAILABLE_ANY
const anjay_dm_object_def_t **push_button_object_create(void) {
    return NULL;
}

void push_button_object_release(const anjay_dm_object_def_t ***out_def) {
    (void) out_def;
}

void push_button_object_update(anjay_t *anjay,
                               const anjay_dm_object_def_t *const *def) {
    (void) anjay;
    (void) def;
}
#endif // PUSH_BUTTON_AVAILABLE_ANY
