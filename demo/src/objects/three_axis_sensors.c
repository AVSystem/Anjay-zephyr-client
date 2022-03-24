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

#include <anjay/anjay.h>
#include <anjay/ipso_objects.h>

#include <avsystem/commons/avs_log.h>

#include <drivers/sensor.h>
#include <zephyr.h>

#include "objects.h"

typedef struct {
    const char *name;
    const char *unit;
    anjay_oid_t oid;
    const struct device *device;
    enum sensor_channel channel;
    double scale_factor;
} sensor_context_t;

#define GAUSS_TO_TESLA_FACTOR 1e-4

static sensor_context_t THREE_AXIS_SENSORS_DEF[] = {
#if ACCELEROMETER_AVAILABLE
    {
        .name = "Accelerometer",
        .unit = "m/s2",
        .oid = 3313,
        .device = DEVICE_DT_GET(ACCELEROMETER_NODE),
        .channel = SENSOR_CHAN_ACCEL_XYZ
    },
#endif // ACCELEROMETER_AVAILABLE
#if MAGNETOMETER_AVAILABLE
    {
        .name = "Magnetometer",
        .unit = "T",
        .oid = 3314,
        .device = DEVICE_DT_GET(MAGNETOMETER_NODE),
        .channel = SENSOR_CHAN_MAGN_XYZ,
        .scale_factor = GAUSS_TO_TESLA_FACTOR
    },
#endif // MAGNETOMETER_AVAILABLE
#if GYROMETER_AVAILABLE
    {
        .name = "Gyrometer",
        .unit = "deg/s",
        .oid = 3334,
        .device = DEVICE_DT_GET(GYROMETER_NODE),
        .channel = SENSOR_CHAN_GYRO_XYZ
    },
#endif // GYROMETER_AVAILABLE
};

int three_axis_sensor_get_values(anjay_iid_t iid,
                                 void *_ctx,
                                 double *x_value,
                                 double *y_value,
                                 double *z_value) {
    sensor_context_t *ctx = (sensor_context_t *) _ctx;

    struct sensor_value values[3];
    if (sensor_sample_fetch_chan(ctx->device, ctx->channel)
            || sensor_channel_get(ctx->device, ctx->channel, values)) {
        avs_log(sensor, ERROR, "Failed to read from %s", ctx->device->name);
        return -1;
    }

    *x_value = sensor_value_to_double(&values[0]);
    *y_value = sensor_value_to_double(&values[1]);
    *z_value = sensor_value_to_double(&values[2]);

    if (ctx->scale_factor) {
        *x_value = (*x_value) * ctx->scale_factor;
        *y_value = (*y_value) * ctx->scale_factor;
        *z_value = (*z_value) * ctx->scale_factor;
    }

    return 0;
}

void three_axis_sensors_install(anjay_t *anjay) {
    for (int i = 0; i < AVS_ARRAY_SIZE(THREE_AXIS_SENSORS_DEF); i++) {
        sensor_context_t *ctx = &THREE_AXIS_SENSORS_DEF[i];

        if (!device_is_ready(ctx->device)) {
            avs_log(ipso_object,
                    WARNING,
                    "Object: %s could not be installed",
                    ctx->name);
            continue;
        }

        if (anjay_ipso_3d_sensor_install(anjay, ctx->oid, 1)) {
            avs_log(ipso_object,
                    WARNING,
                    "Object: %s could not be installed",
                    ctx->name);
            continue;
        }
        if (anjay_ipso_3d_sensor_instance_add(
                    anjay,
                    ctx->oid,
                    0,
                    (anjay_ipso_3d_sensor_impl_t) {
                        .unit = ctx->unit,
                        .use_y_value = true,
                        .use_z_value = true,
                        .user_context = ctx,
                        .min_range_value = NAN,
                        .max_range_value = NAN,
                        .get_values = three_axis_sensor_get_values
                    })) {
            avs_log(ipso_object,
                    WARNING,
                    "Instance of %s object could not be added",
                    ctx->name);
        }
    }
}

void three_axis_sensors_update(anjay_t *anjay) {
    for (int i = 0; i < AVS_ARRAY_SIZE(THREE_AXIS_SENSORS_DEF); i++) {
        anjay_ipso_3d_sensor_update(anjay, THREE_AXIS_SENSORS_DEF[i].oid, 0);
    }
}
