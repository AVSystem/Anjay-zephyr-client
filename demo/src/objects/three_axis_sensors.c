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

#include <drivers/sensor.h>
#include <zephyr.h>
#include <logging/log.h>

#include "objects.h"

LOG_MODULE_REGISTER(three_axis_sensors);

struct sensor_sync_context {
	/**
	 * On some platform, access to buses like I2C is not inherently synchronized.
	 * To allow accessing peripherals from multiple contexts (e.g. react to GPS
	 * messages), we only access those buses through k_sys_work_q by convention.
	 */
	struct k_work work;
	struct k_sem sem;
	volatile double x;
	volatile double y;
	volatile double z;
};

struct sensor_context {
	const char *name;
	const char *unit;
	anjay_oid_t oid;
	const struct device *device;
	enum sensor_channel channel;
	double scale_factor;
	struct sensor_sync_context sync;
};

#define GAUSS_TO_TESLA_FACTOR 1e-4

static struct sensor_context three_axis_sensors_def[] = {
#if ACCELEROMETER_AVAILABLE
	{ .name = "Accelerometer",
	  .unit = "m/s2",
	  .oid = 3313,
	  .device = DEVICE_DT_GET(ACCELEROMETER_NODE),
	  .channel = SENSOR_CHAN_ACCEL_XYZ },
#endif // ACCELEROMETER_AVAILABLE
#if MAGNETOMETER_AVAILABLE
	{ .name = "Magnetometer",
	  .unit = "T",
	  .oid = 3314,
	  .device = DEVICE_DT_GET(MAGNETOMETER_NODE),
	  .channel = SENSOR_CHAN_MAGN_XYZ,
	  .scale_factor = GAUSS_TO_TESLA_FACTOR },
#endif // MAGNETOMETER_AVAILABLE
#if GYROMETER_AVAILABLE
	{ .name = "Gyrometer",
	  .unit = "deg/s",
	  .oid = 3334,
	  .device = DEVICE_DT_GET(GYROMETER_NODE),
	  .channel = SENSOR_CHAN_GYRO_XYZ },
#endif // GYROMETER_AVAILABLE
};

static void three_axis_sensor_work_handler(struct k_work *work)
{
	struct sensor_sync_context *sync_ctx = CONTAINER_OF(work, struct sensor_sync_context, work);
	struct sensor_context *ctx = CONTAINER_OF(sync_ctx, struct sensor_context, sync);

	struct sensor_value values[3];

	if (sensor_sample_fetch_chan(ctx->device, ctx->channel) ||
	    sensor_channel_get(ctx->device, ctx->channel, values)) {
		sync_ctx->x = NAN;
		sync_ctx->y = NAN;
		sync_ctx->z = NAN;
	} else {
		sync_ctx->x = sensor_value_to_double(&values[0]);
		sync_ctx->y = sensor_value_to_double(&values[1]);
		sync_ctx->z = sensor_value_to_double(&values[2]);
	}

	k_sem_give(&sync_ctx->sem);
}

int three_axis_sensor_get_values(anjay_iid_t iid, void *_ctx, double *x_value, double *y_value,
				 double *z_value)
{
	struct sensor_context *ctx = (struct sensor_context *)_ctx;

	k_work_submit(&ctx->sync.work);
	k_sem_take(&ctx->sync.sem, K_FOREVER);

	*x_value = ctx->sync.x;
	*y_value = ctx->sync.y;
	*z_value = ctx->sync.z;

	if (isnan(*x_value) && isnan(*y_value) && isnan(*z_value)) {
		return -1;
	}

	if (ctx->scale_factor) {
		*x_value = (*x_value) * ctx->scale_factor;
		*y_value = (*y_value) * ctx->scale_factor;
		*z_value = (*z_value) * ctx->scale_factor;
	}

	return 0;
}

static int sensor_sync_context_init(struct sensor_sync_context *ctx)
{
	ctx->x = NAN;
	ctx->y = NAN;
	ctx->z = NAN;
	k_work_init(&ctx->work, three_axis_sensor_work_handler);
	return k_sem_init(&ctx->sem, 0, 1);
}

void three_axis_sensors_install(anjay_t *anjay)
{
	for (int i = 0; i < AVS_ARRAY_SIZE(three_axis_sensors_def); i++) {
		struct sensor_context *ctx = &three_axis_sensors_def[i];

		if (!device_is_ready(ctx->device)) {
			LOG_WRN("Object: %s could not be installed", ctx->name);
			continue;
		}

		if (sensor_sync_context_init(&ctx->sync)) {
			LOG_WRN("Object: %s could not be installed", ctx->name);
			continue;
		}

		if (anjay_ipso_3d_sensor_install(anjay, ctx->oid, 1)) {
			LOG_WRN("Object: %s could not be installed", ctx->name);
			continue;
		}
		anjay_ipso_3d_sensor_impl_t impl = { .unit = ctx->unit,
						     .use_y_value = true,
						     .use_z_value = true,
						     .user_context = ctx,
						     .min_range_value = NAN,
						     .max_range_value = NAN,
						     .get_values = three_axis_sensor_get_values };
		if (anjay_ipso_3d_sensor_instance_add(anjay, ctx->oid, 0, impl)) {
			LOG_WRN("Instance of %s object could not be added", ctx->name);
		}
	}
}

void three_axis_sensors_update(anjay_t *anjay)
{
	for (int i = 0; i < AVS_ARRAY_SIZE(three_axis_sensors_def); i++) {
		anjay_ipso_3d_sensor_update(anjay, three_axis_sensors_def[i].oid, 0);
	}
}
