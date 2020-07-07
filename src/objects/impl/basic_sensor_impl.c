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
#include <avsystem/commons/avs_log.h>
#include <avsystem/commons/avs_memory.h>

#include <drivers/sensor.h>
#include <zephyr.h>

#include "basic_sensor_impl.h"

/**
 * Min Measured Value: R, Single, Optional
 * type: float, range: N/A, unit: N/A
 * The minimum value measured by the sensor since power ON or reset.
 */
#define RID_MIN_MEASURED_VALUE 5601

/**
 * Max Measured Value: R, Single, Optional
 * type: float, range: N/A, unit: N/A
 * The maximum value measured by the sensor since power ON or reset.
 */
#define RID_MAX_MEASURED_VALUE 5602

/**
 * Reset Min and Max Measured Values: E, Single, Optional
 * type: N/A, range: N/A, unit: N/A
 * Reset the Min and Max Measured Values to Current Value.
 */
#define RID_RESET_MIN_AND_MAX_MEASURED_VALUES 5605

/**
 * Sensor Value: R, Single, Mandatory
 * type: float, range: N/A, unit: N/A
 * Last or Current Measured Value from the Sensor.
 */
#define RID_SENSOR_VALUE 5700

/**
 * Sensor Units: R, Single, Optional
 * type: string, range: N/A, unit: N/A
 * Measurement Units Definition.
 */
#define RID_SENSOR_UNITS 5701

#define BASIC_SENSOR_OBJ_DEF(Oid)                             \
    (anjay_dm_object_def_t) {                                 \
        .oid = (Oid),                                         \
        .handlers = {                                         \
            .list_instances = anjay_dm_list_instances_SINGLE, \
                                                              \
            .list_resources = basic_sensor_list_resources,    \
            .resource_read = basic_sensor_resource_read,      \
            .resource_execute = basic_sensor_resource_execute \
        }                                                     \
    }

typedef struct basic_sensor_object_struct {
    const anjay_dm_object_def_t *def_ptr;
    anjay_dm_object_def_t def;

    struct device *dev;
    enum sensor_channel channel;

    float curr_value;
    float min_value;
    float max_value;
    char unit[8];
} basic_sensor_object_t;

static basic_sensor_object_t *
get_obj(const anjay_dm_object_def_t *const *obj_ptr) {
    assert(obj_ptr);
    return AVS_CONTAINER_OF(obj_ptr, basic_sensor_object_t, def_ptr);
}

static int
basic_sensor_list_resources(anjay_t *anjay,
                            const anjay_dm_object_def_t *const *obj_ptr,
                            anjay_iid_t iid,
                            anjay_dm_resource_list_ctx_t *ctx) {
    (void) anjay;
    (void) obj_ptr;
    (void) iid;

    anjay_dm_emit_res(ctx, RID_MIN_MEASURED_VALUE, ANJAY_DM_RES_R,
                      ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_MAX_MEASURED_VALUE, ANJAY_DM_RES_R,
                      ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_RESET_MIN_AND_MAX_MEASURED_VALUES,
                      ANJAY_DM_RES_E, ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_SENSOR_VALUE, ANJAY_DM_RES_R,
                      ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_SENSOR_UNITS, ANJAY_DM_RES_R,
                      ANJAY_DM_RES_PRESENT);
    return 0;
}

static int
basic_sensor_resource_read(anjay_t *anjay,
                           const anjay_dm_object_def_t *const *obj_ptr,
                           anjay_iid_t iid,
                           anjay_rid_t rid,
                           anjay_riid_t riid,
                           anjay_output_ctx_t *ctx) {
    (void) anjay;

    basic_sensor_object_t *obj = get_obj(obj_ptr);
    assert(obj);
    assert(iid == 0);

    switch (rid) {
    case RID_MIN_MEASURED_VALUE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_float(ctx, obj->min_value);

    case RID_MAX_MEASURED_VALUE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_float(ctx, obj->max_value);

    case RID_SENSOR_VALUE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_float(ctx, obj->curr_value);

    case RID_SENSOR_UNITS:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_string(ctx, obj->unit);

    default:
        return ANJAY_ERR_METHOD_NOT_ALLOWED;
    }
}

static int
basic_sensor_resource_execute(anjay_t *anjay,
                              const anjay_dm_object_def_t *const *obj_ptr,
                              anjay_iid_t iid,
                              anjay_rid_t rid,
                              anjay_execute_ctx_t *arg_ctx) {
    (void) arg_ctx;

    basic_sensor_object_t *obj = get_obj(obj_ptr);
    assert(obj);
    assert(iid == 0);

    switch (rid) {
    case RID_RESET_MIN_AND_MAX_MEASURED_VALUES:
        obj->max_value = obj->curr_value;
        obj->min_value = obj->curr_value;
        return 0;

    default:
        return ANJAY_ERR_METHOD_NOT_ALLOWED;
    }
}

static int
get_value(struct device *dev, enum sensor_channel channel, float *out_value) {
    struct sensor_value value;
    if (sensor_sample_fetch_chan(dev, channel)
            || sensor_channel_get(dev, channel, &value)) {
        avs_log(sensor, ERROR, "Failed to read from %s", dev->name);
        return -1;
    }

    *out_value = (float) sensor_value_to_double(&value);
    return 0;
}

const anjay_dm_object_def_t **
basic_sensor_object_create(const char *name,
                           enum sensor_channel channel,
                           const char *unit,
                           anjay_oid_t oid) {
    struct device *dev = device_get_binding(name);
    if (!dev) {
        return NULL;
    }

    float value;
    if (get_value(dev, channel, &value)) {
        return NULL;
    }

    basic_sensor_object_t *obj =
            (basic_sensor_object_t *) avs_calloc(1,
                                                 sizeof(basic_sensor_object_t));
    if (!obj) {
        return NULL;
    }
    obj->def = BASIC_SENSOR_OBJ_DEF(oid);
    obj->def_ptr = &obj->def;
    obj->dev = dev;
    obj->channel = channel;
    obj->curr_value = value;
    obj->min_value = value;
    obj->max_value = value;

    assert(sizeof(obj->unit) > strlen(unit));
    strcpy(obj->unit, unit);

    return &obj->def_ptr;
}

void basic_sensor_object_release(const anjay_dm_object_def_t **def) {
    if (def) {
        basic_sensor_object_t *obj = get_obj(def);
        avs_free(obj);
    }
}

void basic_sensor_object_update(anjay_t *anjay,
                                const anjay_dm_object_def_t *const *def) {
    if (!anjay || !def) {
        return;
    }

    basic_sensor_object_t *obj = get_obj(def);

    float value;
    if (get_value(obj->dev, obj->channel, &value) || value == obj->curr_value) {
        return;
    }

    obj->curr_value = value;
    (void) anjay_notify_changed(anjay, obj->def.oid, 0, RID_SENSOR_VALUE);

    const float min_value = AVS_MIN(obj->min_value, obj->curr_value);
    const float max_value = AVS_MAX(obj->max_value, obj->curr_value);
    if (min_value != obj->min_value) {
        (void) anjay_notify_changed(anjay, obj->def.oid, 0,
                                    RID_MIN_MEASURED_VALUE);
        obj->min_value = min_value;
    }
    if (max_value != obj->max_value) {
        (void) anjay_notify_changed(anjay, obj->def.oid, 0,
                                    RID_MAX_MEASURED_VALUE);
        obj->max_value = max_value;
    }
}
