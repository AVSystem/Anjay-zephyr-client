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

struct sensor_context {
	const char *name;
	const char *unit;
	anjay_oid_t oid;
	const struct device *device;
	enum sensor_channel channel;
	double scale_factor;
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
};

int basic_sensor_get_value(anjay_iid_t iid, void *_ctx, double *value)
{
	struct sensor_context *ctx = (struct sensor_context *)_ctx;

	struct sensor_value tmp_value;

	if (sensor_sample_fetch_chan(ctx->device, ctx->channel) ||
	    sensor_channel_get(ctx->device, ctx->channel, &tmp_value)) {
		LOG_ERR("Failed to read from %s", ctx->device->name);
		return -1;
	}

	*value = (float)sensor_value_to_double(&tmp_value);

	if (ctx->scale_factor) {
		*value = (*value) * ctx->scale_factor;
	}

	return 0;
}

void basic_sensors_install(anjay_t *anjay)
{
	for (int i = 0; i < AVS_ARRAY_SIZE(basic_sensors_def); i++) {
		struct sensor_context *ctx = &basic_sensors_def[i];

		if (!device_is_ready(ctx->device)) {
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
