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
#include <avsystem/commons/avs_log.h>
#include <avsystem/commons/avs_memory.h>

#include <drivers/sensor.h>
#include <zephyr.h>

#include "three_axis_sensor_impl.h"

/**
 * Sensor Units: R, Single, Optional
 * type: string, range: N/A, unit: N/A
 * Measurement Units Definition.
 */
#define RID_SENSOR_UNITS 5701

/**
 * X Value: R, Single, Mandatory
 * type: float, range: N/A, unit: N/A
 * The measured value along the X axis.
 */
#define RID_X_VALUE 5702

/**
 * Y Value: R, Single, Optional
 * type: float, range: N/A, unit: N/A
 * The measured value along the Y axis.
 */
#define RID_Y_VALUE 5703

/**
 * Z Value: R, Single, Optional
 * type: float, range: N/A, unit: N/A
 * The measured value along the Z axis.
 */
#define RID_Z_VALUE 5704

#define THREE_AXIS_SENSOR_OBJ_DEF(Oid)                          \
    (anjay_dm_object_def_t) {                                   \
        .oid = (Oid),                                           \
        .handlers = {                                           \
            .list_instances = anjay_dm_list_instances_SINGLE,   \
            .list_resources = three_axis_sensor_list_resources, \
            .resource_read = three_axis_sensor_resource_read    \
        }                                                       \
    }

typedef struct accelerometer_object_struct {
    const anjay_dm_object_def_t *def_ptr;
    anjay_dm_object_def_t def;

    const struct device *dev;
    enum sensor_channel channel;

    float x_value;
    float y_value;
    float z_value;
    char unit[8];
} three_axis_sensor_object_t;

static inline three_axis_sensor_object_t *
get_obj(const anjay_dm_object_def_t *const *obj_ptr) {
    assert(obj_ptr);
    return AVS_CONTAINER_OF(obj_ptr, three_axis_sensor_object_t, def_ptr);
}

static int
three_axis_sensor_list_resources(anjay_t *anjay,
                                 const anjay_dm_object_def_t *const *obj_ptr,
                                 anjay_iid_t iid,
                                 anjay_dm_resource_list_ctx_t *ctx) {
    (void) anjay;
    (void) obj_ptr;
    (void) iid;

    anjay_dm_emit_res(ctx, RID_SENSOR_UNITS, ANJAY_DM_RES_R,
                      ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_X_VALUE, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_Y_VALUE, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_Z_VALUE, ANJAY_DM_RES_R, ANJAY_DM_RES_PRESENT);
    return 0;
}

static int
three_axis_sensor_resource_read(anjay_t *anjay,
                                const anjay_dm_object_def_t *const *obj_ptr,
                                anjay_iid_t iid,
                                anjay_rid_t rid,
                                anjay_riid_t riid,
                                anjay_output_ctx_t *ctx) {
    (void) anjay;

    three_axis_sensor_object_t *obj = get_obj(obj_ptr);
    assert(obj);
    assert(iid == 0);

    switch (rid) {
    case RID_SENSOR_UNITS:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_string(ctx, obj->unit);

    case RID_X_VALUE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_float(ctx, obj->x_value);

    case RID_Y_VALUE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_float(ctx, obj->y_value);

    case RID_Z_VALUE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_float(ctx, obj->z_value);

    default:
        return ANJAY_ERR_METHOD_NOT_ALLOWED;
    }
}

static int get_values(const struct device *dev,
                      enum sensor_channel channel,
                      float *out_x_value,
                      float *out_y_value,
                      float *out_z_value) {
    struct sensor_value values[3];
    if (sensor_sample_fetch_chan(dev, channel)
            || sensor_channel_get(dev, channel, values)) {
        avs_log(sensor, ERROR, "Failed to read from %s", dev->name);
        return -1;
    }

    *out_x_value = (float) sensor_value_to_double(&values[0]);
    *out_y_value = (float) sensor_value_to_double(&values[1]);
    *out_z_value = (float) sensor_value_to_double(&values[2]);
    return 0;
}

const anjay_dm_object_def_t **
three_axis_sensor_object_create(const struct device *dev,
                                enum sensor_channel channel,
                                const char *unit,
                                anjay_oid_t oid) {
    if (!device_is_ready(dev)) {
        return NULL;
    }

    float x_value, y_value, z_value;
    if (get_values(dev, channel, &x_value, &y_value, &z_value)) {
        return NULL;
    }

    three_axis_sensor_object_t *obj = (three_axis_sensor_object_t *) avs_calloc(
            1, sizeof(three_axis_sensor_object_t));
    if (!obj) {
        return NULL;
    }
    obj->def = THREE_AXIS_SENSOR_OBJ_DEF(oid);
    obj->def_ptr = &obj->def;
    obj->dev = dev;
    obj->channel = channel;
    obj->x_value = x_value;
    obj->y_value = y_value;
    obj->z_value = z_value;

    assert(sizeof(obj->unit) > strlen(unit));
    strcpy(obj->unit, unit);

    return &obj->def_ptr;
}

void three_axis_sensor_object_release(const anjay_dm_object_def_t ***out_def) {
    const anjay_dm_object_def_t **def = *out_def;
    if (def) {
        three_axis_sensor_object_t *obj = get_obj(def);
        avs_free(obj);
        *out_def = NULL;
    }
}

void three_axis_sensor_object_update(anjay_t *anjay,
                                     const anjay_dm_object_def_t *const *def) {
    if (!anjay || !def) {
        return;
    }

    three_axis_sensor_object_t *obj = get_obj(def);

    float x_value, y_value, z_value;
    if (get_values(obj->dev, obj->channel, &x_value, &y_value, &z_value)) {
        return;
    }

    if (x_value != obj->x_value) {
        obj->x_value = x_value;
        (void) anjay_notify_changed(anjay, obj->def.oid, 0, RID_X_VALUE);
    }

    if (y_value != obj->y_value) {
        obj->y_value = y_value;
        (void) anjay_notify_changed(anjay, obj->def.oid, 0, RID_Y_VALUE);
    }

    if (z_value != obj->z_value) {
        obj->z_value = z_value;
        (void) anjay_notify_changed(anjay, obj->def.oid, 0, RID_Z_VALUE);
    }
}
