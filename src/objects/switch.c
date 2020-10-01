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

#define FLAGS_OR_ZERO(node)                          \
    COND_CODE_1(DT_PHA_HAS_CELL(node, gpios, flags), \
                (DT_GPIO_FLAGS(node, gpios)), (0))

#define SW2_NODE DT_ALIAS(sw2)
#if DT_NODE_HAS_STATUS(SW2_NODE, okay)
#    define SW2_GPIO_LABEL DT_GPIO_LABEL(SW2_NODE, gpios)
#    define SW2_GPIO_PIN DT_GPIO_PIN(SW2_NODE, gpios)
#    define SW2_GPIO_FLAGS (GPIO_INPUT | DT_GPIO_FLAGS(SW2_NODE, gpios))
#endif // DT_NODE_HAS_STATUS(SW2_NODE, okay)

#define SW3_NODE DT_ALIAS(sw3)
#if DT_NODE_HAS_STATUS(SW3_NODE, okay)
#    define SW3_GPIO_LABEL DT_GPIO_LABEL(SW3_NODE, gpios)
#    define SW3_GPIO_PIN DT_GPIO_PIN(SW3_NODE, gpios)
#    define SW3_GPIO_FLAGS (GPIO_INPUT | DT_GPIO_FLAGS(SW3_NODE, gpios))
#endif // DT_NODE_HAS_STATUS(SW3_NODE, okay)

/**
 * Digital Input State: R, Single, Mandatory
 * type: boolean, range: N/A, unit: N/A
 * The current state of a digital input.
 */
#define RID_DIGITAL_INPUT_STATE 5500

typedef struct switch_instance_struct {
    anjay_iid_t iid;

    struct device *dev;
    int gpio_pin;

    bool digital_input_state;
} switch_instance_t;

typedef struct switch_object_struct {
    const anjay_dm_object_def_t *def;
    AVS_LIST(switch_instance_t) instances;
} switch_object_t;

static inline switch_object_t *
get_obj(const anjay_dm_object_def_t *const *obj_ptr) {
    assert(obj_ptr);
    return AVS_CONTAINER_OF(obj_ptr, switch_object_t, def);
}

static switch_instance_t *find_instance(const switch_object_t *obj,
                                        anjay_iid_t iid) {
    AVS_LIST(switch_instance_t) it;
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

    AVS_LIST(switch_instance_t) it;
    AVS_LIST_FOREACH(it, get_obj(obj_ptr)->instances) {
        anjay_dm_emit(ctx, it->iid);
    }

    return 0;
}

static int init_instance(switch_instance_t *inst, anjay_iid_t iid) {
    assert(iid != ANJAY_ID_INVALID);

    inst->iid = iid;
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
    return 0;
}

static int resource_read(anjay_t *anjay,
                         const anjay_dm_object_def_t *const *obj_ptr,
                         anjay_iid_t iid,
                         anjay_rid_t rid,
                         anjay_riid_t riid,
                         anjay_output_ctx_t *ctx) {
    (void) anjay;

    switch_object_t *obj = get_obj(obj_ptr);
    assert(obj);
    switch_instance_t *inst = find_instance(obj, iid);
    assert(inst);

    switch (rid) {
    case RID_DIGITAL_INPUT_STATE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_bool(ctx, inst->digital_input_state);

    default:
        return ANJAY_ERR_METHOD_NOT_ALLOWED;
    }
}

static switch_instance_t *add_instance(switch_object_t *obj, anjay_iid_t iid) {
    assert(find_instance(obj, iid) == NULL);

    AVS_LIST(switch_instance_t) created =
            AVS_LIST_NEW_ELEMENT(switch_instance_t);
    if (!created) {
        return NULL;
    }

    int result = init_instance(created, iid);
    if (result) {
        AVS_LIST_CLEAR(&created);
        return NULL;
    }

    AVS_LIST(switch_instance_t) *ptr;
    AVS_LIST_FOREACH_PTR(ptr, &obj->instances) {
        if ((*ptr)->iid > created->iid) {
            break;
        }
    }

    AVS_LIST_INSERT(ptr, created);
    return created;
}

static int configure_switch(switch_object_t *obj,
                            const char *gpio_label,
                            int gpio_pin,
                            int gpio_flags,
                            anjay_iid_t iid) {
    struct device *dev = device_get_binding(gpio_label);
    if (!dev || gpio_pin_configure(dev, gpio_pin, gpio_flags)) {
        return -1;
    }

    switch_instance_t *inst = add_instance(obj, iid);
    if (inst) {
        inst->dev = dev;
        inst->digital_input_state = gpio_pin_get(inst->dev, gpio_pin);
        inst->gpio_pin = gpio_pin;
    } else {
        return -1;
    }

    return 0;
}

static const anjay_dm_object_def_t OBJ_DEF = {
    .oid = 3342,
    .handlers = {
        .list_instances = list_instances,
        .list_resources = list_resources,
        .resource_read = resource_read
    }
};

static const anjay_dm_object_def_t *OBJ_DEF_PTR = &OBJ_DEF;

void switch_object_release(const anjay_dm_object_def_t **def) {
    if (def) {
        switch_object_t *obj = get_obj(def);
        AVS_LIST_CLEAR(&obj->instances);
        avs_free(obj);
    }
}

const anjay_dm_object_def_t **switch_object_create(void) {
    switch_object_t *obj =
            (switch_object_t *) avs_calloc(1, sizeof(switch_object_t));
    if (!obj) {
        return NULL;
    }

#if DT_NODE_HAS_STATUS(SW2_NODE, okay)
    if (configure_switch(obj, SW2_GPIO_LABEL, SW2_GPIO_PIN, SW2_GPIO_FLAGS,
                         0)) {
        switch_object_release(&OBJ_DEF_PTR);
        return NULL;
    }
#endif // DT_NODE_HAS_STATUS(SW2_NODE, okay)

#if DT_NODE_HAS_STATUS(SW3_NODE, okay)
    if (configure_switch(obj, SW3_GPIO_LABEL, SW3_GPIO_PIN, SW3_GPIO_FLAGS,
                         1)) {
        switch_object_release(&OBJ_DEF_PTR);
        return NULL;
    }
#endif // DT_NODE_HAS_STATUS(SW3_NODE, okay)

    obj->def = OBJ_DEF_PTR;
    return &obj->def;
}

void switch_object_update(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *def) {
    if (!anjay || !def) {
        return;
    }

    switch_object_t *obj = get_obj(def);

    AVS_LIST(switch_instance_t) inst;
    AVS_LIST_FOREACH(inst, obj->instances) {
        int switch_state = gpio_pin_get(inst->dev, inst->gpio_pin);
        if (switch_state >= 0 && switch_state != inst->digital_input_state) {
            inst->digital_input_state = switch_state;
            (void) anjay_notify_changed(anjay, obj->def->oid, inst->iid,
                                        RID_DIGITAL_INPUT_STATE);
        }
    }
}
