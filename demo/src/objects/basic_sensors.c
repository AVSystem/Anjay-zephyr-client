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

LOG_MODULE_REGISTER(basic_sensors);

struct sensor_sync_context {
	/**
	 * On some platform, access to buses like I2C is not inherently synchronized.
	 * To allow accessing peripherals from multiple contexts (e.g. react to GPS
	 * messages), we only access those buses through k_sys_work_q by convention.
	 */
	struct k_work work;
	struct k_sem sem;
	volatile double value;
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

#define KPA_TO_PA_FACTOR 1e3

static struct sensor_context basic_sensors_def[] = {
#if TEMPERATURE_AVAILABLE
	{ .name = "Temperature",
	  .unit = "Cel",
	  .oid = 3303,
	  .device = DEVICE_DT_GET(TEMPERATURE_NODE),
	  .channel = SENSOR_CHAN_AMBIENT_TEMP },
#endif // TEMPERATURE_AVAILABLE
#if HUMIDITY_AVAILABLE
	{ .name = "Humidity",
	  .unit = "%RH",
	  .oid = 3304,
	  .device = DEVICE_DT_GET(HUMIDITY_NODE),
	  .channel = SENSOR_CHAN_HUMIDITY },
#endif // HUMIDITY_AVAILABLE
#if BAROMETER_AVAILABLE
	{ .name = "Barometer",
	  .unit = "Pa",
	  .oid = 3315,
	  .device = DEVICE_DT_GET(BAROMETER_NODE),
	  .channel = SENSOR_CHAN_PRESS,
	  .scale_factor = KPA_TO_PA_FACTOR },
#endif // BAROMETER_AVAILABLE
#if DISTANCE_AVAILABLE
	{ .name = "Distance",
	  .unit = "m",
	  .oid = 3330,
	  .device = DEVICE_DT_GET(DISTANCE_NODE),
	  .channel = SENSOR_CHAN_DISTANCE },
#endif // DISTANCE_AVAILABLE
#if ILLUMINANCE_AVAILABLE
	{ .name = "Illuminance",
	  .unit = "lx",
	  .oid = 3301,
	  .device = DEVICE_DT_GET(ILLUMINANCE_NODE),
	  .channel = SENSOR_CHAN_LIGHT },
#endif // ILLUMINANCE_AVAILABLE
};

static void basic_sensor_work_handler(struct k_work *work)
{
	struct sensor_sync_context *sync_ctx = CONTAINER_OF(work, struct sensor_sync_context, work);
	struct sensor_context *ctx = CONTAINER_OF(sync_ctx, struct sensor_context, sync);

	struct sensor_value tmp_value;

	if (sensor_sample_fetch_chan(ctx->device, ctx->channel) ||
	    sensor_channel_get(ctx->device, ctx->channel, &tmp_value)) {
		sync_ctx->value = NAN;
	} else {
		sync_ctx->value = sensor_value_to_double(&tmp_value);
	}

	k_sem_give(&sync_ctx->sem);
}

int basic_sensor_get_value(anjay_iid_t iid, void *_ctx, double *value)
{
	struct sensor_context *ctx = (struct sensor_context *)_ctx;

	k_work_submit(&ctx->sync.work);
	k_sem_take(&ctx->sync.sem, K_FOREVER);

	*value = ctx->sync.value;

	if (isnan(*value)) {
		return -1;
	}

	if (ctx->scale_factor) {
		*value = (*value) * ctx->scale_factor;
	}

	return 0;
}

static int sensor_sync_context_init(struct sensor_sync_context *ctx)
{
	ctx->value = NAN;
	k_work_init(&ctx->work, basic_sensor_work_handler);
	return k_sem_init(&ctx->sem, 0, 1);
}

void basic_sensors_install(anjay_t *anjay)
{
	for (int i = 0; i < AVS_ARRAY_SIZE(basic_sensors_def); i++) {
		struct sensor_context *ctx = &basic_sensors_def[i];

		if (!device_is_ready(ctx->device)) {
			LOG_WRN("Object: %s could not be installed", ctx->name);
			continue;
		}

		if (sensor_sync_context_init(&ctx->sync)) {
			LOG_WRN("Object: %s could not be installed", ctx->name);
			continue;
		}

		if (anjay_ipso_basic_sensor_install(anjay, ctx->oid, 1)) {
			LOG_WRN("Object: %s could not be installed", ctx->name);
			continue;
		}

		anjay_ipso_basic_sensor_impl_t impl = { .unit = ctx->unit,
							.user_context = ctx,
							.min_range_value = NAN,
							.max_range_value = NAN,
							.get_value = basic_sensor_get_value };
		if (anjay_ipso_basic_sensor_instance_add(anjay, ctx->oid, 0, impl)) {
			LOG_WRN("Instance of %s object could not be added", ctx->name);
		}
	}
}

void basic_sensors_update(anjay_t *anjay)
{
	for (int i = 0; i < AVS_ARRAY_SIZE(basic_sensors_def); i++) {
		anjay_ipso_basic_sensor_update(anjay, basic_sensors_def[i].oid, 0);
	}
}
