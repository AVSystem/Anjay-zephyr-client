/*
 * Copyright 2020-2024 AVSystem <avsystem@avsystem.com>
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

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/w1.h>
#include <zephyr/drivers/sensor/w1_sensor.h>

#include "sensors.h"
#include "peripherals.h"

#if TEMPERATURE_0_AVAILABLE && TEMPERATURE_1_AVAILABLE
#include <avsystem/commons/avs_init_once.h>
#endif // TEMPERATURE_0_AVAILABLE && TEMPERATURE_1_AVAILABLE

LOG_MODULE_REGISTER(sensor);

#define ADC_HAS_SENSOR(Sensor) DT_PROP_HAS_NAME(DT_PATH(zephyr_user), io_channels, Sensor)

#define PRESSURE_0_AVAILABLE ADC_HAS_SENSOR(pressure0)
#define PRESSURE_1_AVAILABLE ADC_HAS_SENSOR(pressure1)
#define ACIDITY_0_AVAILABLE ADC_HAS_SENSOR(acidity0)
#define ACIDITY_1_AVAILABLE ADC_HAS_SENSOR(acidity1)

enum adc_channels {
#if PRESSURE_0_AVAILABLE
	ADC_CHANNEL_PRESSURE_0,
#endif // PRESSURE_0_AVAILABLE
#if PRESSURE_1_AVAILABLE
	ADC_CHANNEL_PRESSURE_1,
#endif // PRESSURE_1_AVAILABLE
#if ACIDITY_0_AVAILABLE
	ADC_CHANNEL_ACIDITY_0,
#endif // ACIDITY_0_AVAILABLE
#if ACIDITY_1_AVAILABLE
	ADC_CHANNEL_ACIDITY_1,
#endif // ACIDITY_1_AVAILABLE
	_ADC_CHANNEL_NONE
};

static const struct adc_dt_spec available_adc_channels[] = {
#if PRESSURE_0_AVAILABLE
	ADC_DT_SPEC_GET_BY_NAME(DT_PATH(zephyr_user), pressure0),
#endif // PRESSURE_0_AVAILABLE
#if PRESSURE_1_AVAILABLE
	ADC_DT_SPEC_GET_BY_NAME(DT_PATH(zephyr_user), pressure1),
#endif // PRESSURE_1_AVAILABLE
#if ACIDITY_0_AVAILABLE
	ADC_DT_SPEC_GET_BY_NAME(DT_PATH(zephyr_user), acidity0),
#endif // ACIDITY_0_AVAILABLE
#if ACIDITY_1_AVAILABLE
	ADC_DT_SPEC_GET_BY_NAME(DT_PATH(zephyr_user), acidity1),
#endif // ACIDITY_1_AVAILABLE
};

#if TEMPERATURE_0_AVAILABLE && TEMPERATURE_1_AVAILABLE
static avs_init_once_handle_t ds18b20_init_handle;
static uint64_t ds18b20_rom[2];
#endif // TEMPERATURE_0_AVAILABLE && TEMPERATURE_1_AVAILABLE
#if TEMPERATURE_0_AVAILABLE
const struct device *const temperature_dev_0 = DEVICE_DT_GET(TEMPERATURE_0_NODE);
#endif // TEMPERATURE_0_AVAILABLE
#if TEMPERATURE_1_AVAILABLE
const struct device *const temperature_dev_1 = DEVICE_DT_GET(TEMPERATURE_1_NODE);
#endif // TEMPERATURE_1_AVAILABLE

struct basic_sensor_driver {
	int (*init)(void);
	int (*read)(double *out_value);
	bool installed;
};

struct sensor_context {
	anjay_oid_t oid;
	size_t instances_count;
	struct basic_sensor_driver *drivers;
	const char *unit;
};

static int32_t adc_max_possible_value(enum adc_channels channel)
{
	return (1 << available_adc_channels[channel].resolution) - 1;
}

static int32_t adc_get_raw_value(enum adc_channels channel)
{
	union {
		int16_t i16;
		uint16_t u16;
	} buf;

	struct adc_sequence sequence = {
		.buffer = &buf,
		.buffer_size = sizeof(buf),
	};
	int32_t ret_val;
	int err;

	(void)adc_sequence_init_dt(&available_adc_channels[channel], &sequence);
	err = adc_read(available_adc_channels[channel].dev, &sequence);
	if (err < 0) {
		LOG_ERR("Could not read %s (%d)", available_adc_channels[channel].dev->name, err);
		return -1;
	}
	ret_val = available_adc_channels[channel].channel_cfg.differential ? buf.i16 : buf.u16;

	int32_t max_possible_value = adc_max_possible_value(channel);

	if (ret_val > max_possible_value) {
		ret_val = -1;
	}

	return ret_val;
}

static int adc_channel_init(enum adc_channels channel)
{
	int err;

	if (!device_is_ready(available_adc_channels[channel].dev)) {
		LOG_ERR("ADC controller device %s not ready",
			available_adc_channels[channel].dev->name);
		return -1;
	}
	err = adc_channel_setup_dt(&available_adc_channels[channel]);
	if (err < 0) {
		LOG_ERR("Could not setup channel #%d, (%d)", channel, err);
		return -1;
	}

	return 0;
}

#if PRESSURE_0_AVAILABLE || PRESSURE_1_AVAILABLE
static int pressure_get(enum adc_channels channel, double *out_pressure)
{
	int32_t val_raw = adc_get_raw_value(channel);

	if (val_raw == -1) {
		return -1;
	}

	int32_t val_mv = val_raw;

	adc_raw_to_millivolts_dt(&available_adc_channels[channel], &val_mv);

	// sensor output pressure range: 0-30 psi
	static const double SENSOR_PRESSURE_RANGE_PSI = 30.;
	// sensor output voltage: 0.5-4.5 V with 5 V source
	static const double SENSOR_VOLTAGE_MIN = 0.5;
	static const double SENSOR_VOLTAGE_MAX = 4.5;
	// 1 psi = 6.895 kPa
	static const double SENSOR_PSI_TO_KPA = 6.895;
	// 1 atm = 101.325 kPa
	static const double SENSOR_ATM_IN_KPA = 101.325;

	*out_pressure =
		((double)val_mv / 1000. - SENSOR_VOLTAGE_MIN) *
			(SENSOR_PRESSURE_RANGE_PSI / (SENSOR_VOLTAGE_MAX - SENSOR_VOLTAGE_MIN)) *
			SENSOR_PSI_TO_KPA +
		SENSOR_ATM_IN_KPA; // kPa
	*out_pressure = *out_pressure * 1000.; // Pa

	return 0;
}
#endif // PRESSURE_0_AVAILABLE || PRESSURE_1_AVAILABLE

#if ACIDITY_0_AVAILABLE || ACIDITY_1_AVAILABLE
static int acidity_get(enum adc_channels channel, double *out_acidity)
{
	int32_t val_raw = adc_get_raw_value(channel);

	if (val_raw == -1) {
		return -1;
	}

	int32_t val_mv = val_raw;

	adc_raw_to_millivolts_dt(&available_adc_channels[channel], &val_mv);

	// based on: https://wiki.dfrobot.com/PH_meter_SKU__SEN0161_
	*out_acidity = (double)val_mv / 1000.;
	*out_acidity *= 3.5;

	return 0;
}
#endif // ACIDITY_0_AVAILABLE || ACIDITY_1_AVAILABLE

#if PRESSURE_0_AVAILABLE
static int pressure_0_init(void)
{
	return adc_channel_init(ADC_CHANNEL_PRESSURE_0);
}

static int pressure_0_get(double *out_pressure)
{
	return pressure_get(ADC_CHANNEL_PRESSURE_0, out_pressure);
}
#endif // PRESSURE_0_AVAILABLE

#if ACIDITY_0_AVAILABLE
static int acidity_0_init(void)
{
	return adc_channel_init(ADC_CHANNEL_ACIDITY_0);
}

static int acidity_0_get(double *out_acidity)
{
	return acidity_get(ADC_CHANNEL_ACIDITY_0, out_acidity);
}
#endif // ACIDITY_0_AVAILABLE

#if PRESSURE_1_AVAILABLE
static int pressure_1_init(void)
{
	return adc_channel_init(ADC_CHANNEL_PRESSURE_1);
}

static int pressure_1_get(double *out_pressure)
{
	return pressure_get(ADC_CHANNEL_PRESSURE_1, out_pressure);
}
#endif // PRESSURE_1_AVAILABLE

#if ACIDITY_1_AVAILABLE
static int acidity_1_init(void)
{
	return adc_channel_init(ADC_CHANNEL_ACIDITY_1);
}

static int acidity_1_get(double *out_acidity)
{
	return acidity_get(ADC_CHANNEL_ACIDITY_1, out_acidity);
}
#endif // ACIDITY_1_AVAILABLE

#if TEMPERATURE_0_AVAILABLE && TEMPERATURE_1_AVAILABLE
static int lower_rom_index(void)
{
	return ds18b20_rom[0] < ds18b20_rom[1] ? 0 : 1;
}

static int higher_rom_index(void)
{
	return ds18b20_rom[0] > ds18b20_rom[1] ? 0 : 1;
}

void w1_search_callback(struct w1_rom rom, void *user_data)
{
	static int i;

	AVS_ASSERT(
		i <= 1,
		"Found two ds18b20 sensors in devicetree but at least three exist on 1Wire line");

	ds18b20_rom[i++] = w1_rom_to_uint64(&rom);
}

static int ds18b20_init(void *dummy)
{
	const struct device *const w1_dev = DEVICE_DT_GET(DT_NODELABEL(w1));

	if (!device_is_ready(w1_dev)) {
		LOG_ERR("W1 device not ready");
		return -1;
	}
	w1_search_rom(w1_dev, w1_search_callback, NULL);

	// w1_search_callback is not executed immediately, wait 1 second for its execution
	k_sleep(K_SECONDS(1));

	AVS_ASSERT(
		ds18b20_rom[0] != 0 && ds18b20_rom[1] != 0,
		"Found two ds18b20 sensors in devicetree but at least one haven't been found on 1Wire line");

	return 0;
}
#endif // TEMPERATURE_0_AVAILABLE && TEMPERATURE_1_AVAILABLE

#if TEMPERATURE_0_AVAILABLE
static int temperature_0_init(void)
{
	if (temperature_dev_0 == NULL) {
		LOG_ERR("temperature_dev_0 not found");
		return -1;
	}
	if (!device_is_ready(temperature_dev_0)) {
		LOG_ERR("temperature_dev_0 device not ready");
	}
#if TEMPERATURE_0_AVAILABLE && TEMPERATURE_1_AVAILABLE
	if (avs_init_once(&ds18b20_init_handle, ds18b20_init, NULL)) {
		return -1;
	}

	struct sensor_value romval;
	struct w1_rom w1rom;

	w1_uint64_to_rom(ds18b20_rom[lower_rom_index()], &w1rom);
	w1_rom_to_sensor_value(&w1rom, &romval);
	sensor_attr_set(temperature_dev_0, 0, SENSOR_ATTR_W1_ROM, &romval);
#endif // TEMPERATURE_0_AVAILABLE && TEMPERATURE_1_AVAILABLE
	return 0;
}

static int temperature_0_get(double *out_temperature)
{
	struct sensor_value temperature;

	sensor_sample_fetch(temperature_dev_0);
	sensor_channel_get(temperature_dev_0, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
	*out_temperature = sensor_value_to_double(&temperature);
	return 0;
}
#endif // TEMPERATURE_0_AVAILABLE

#if TEMPERATURE_1_AVAILABLE
static int temperature_1_init(void)
{
	if (temperature_dev_1 == NULL) {
		LOG_ERR("temperature_dev_1 not found");
		return -1;
	}
	if (!device_is_ready(temperature_dev_1)) {
		LOG_ERR("temperature_dev_1 device not ready");
	}
#if TEMPERATURE_0_AVAILABLE && TEMPERATURE_1_AVAILABLE
	if (avs_init_once(&ds18b20_init_handle, ds18b20_init, NULL)) {
		return -1;
	}

	struct sensor_value romval;
	struct w1_rom w1rom;

	w1_uint64_to_rom(ds18b20_rom[higher_rom_index()], &w1rom);
	w1_rom_to_sensor_value(&w1rom, &romval);
	sensor_attr_set(temperature_dev_1, 0, SENSOR_ATTR_W1_ROM, &romval);
#endif // TEMPERATURE_0_AVAILABLE && TEMPERATURE_1_AVAILABLE
	return 0;
}

static int temperature_1_get(double *out_temperature)
{
	struct sensor_value temperature;

	sensor_sample_fetch(temperature_dev_1);
	sensor_channel_get(temperature_dev_1, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
	*out_temperature = sensor_value_to_double(&temperature);
	return 0;
}
#endif // TEMPERATURE_1_AVAILABLE

struct basic_sensor_driver PRESSURE_DRIVER[] = {
#if PRESSURE_0_AVAILABLE
	{ .init = pressure_0_init, .read = pressure_0_get },
#endif // PRESSURE_0_AVAILABLE
#if PRESSURE_1_AVAILABLE
	{ .init = pressure_1_init, .read = pressure_1_get },
#endif // PRESSURE_1_AVAILABLE
};
struct basic_sensor_driver ACIDITY_DRIVER[] = {
#if ACIDITY_0_AVAILABLE
	{ .init = acidity_0_init, .read = acidity_0_get },
#endif // ACIDITY_0_AVAILABLE
#if ACIDITY_1_AVAILABLE
	{ .init = acidity_1_init, .read = acidity_1_get },
#endif // ACIDITY_1_AVAILABLE
};
struct basic_sensor_driver TEMPERATURE_DRIVER[] = {
#if TEMPERATURE_0_AVAILABLE
	{ .init = temperature_0_init, .read = temperature_0_get },
#endif // TEMPERATURE_0_AVAILABLE
#if TEMPERATURE_1_AVAILABLE
	{ .init = temperature_1_init, .read = temperature_1_get },
#endif // TEMPERATURE_1_AVAILABLE
};

static const struct sensor_context temperature_sensors_def = { .oid = 3303,
							       .instances_count = AVS_ARRAY_SIZE(
								       TEMPERATURE_DRIVER),
							       .drivers = TEMPERATURE_DRIVER,
							       .unit = "Cel" };

static const struct sensor_context pressure_sensors_def = { .oid = 3323,
							    .instances_count =
								    AVS_ARRAY_SIZE(PRESSURE_DRIVER),
							    .drivers = PRESSURE_DRIVER,
							    .unit = "Pa" };

static const struct sensor_context acidity_sensors_def = { .oid = 3326,
							   .instances_count =
								   AVS_ARRAY_SIZE(ACIDITY_DRIVER),
							   .drivers = ACIDITY_DRIVER,
							   .unit = "-" };

static struct sensor_context basic_sensors_def[] = { temperature_sensors_def, pressure_sensors_def,
						     acidity_sensors_def };

static int read_value(anjay_iid_t iid, void *_ctx, double *out_value)
{
	const struct basic_sensor_driver *driver =
		&(((const struct sensor_context *)_ctx)->drivers[iid]);

	if (driver->read(out_value)) {
		return -1;
	}

	return 0;
}

void basic_sensor_objects_install(anjay_t *anjay)
{
	for (int i = 0; i < AVS_ARRAY_SIZE(basic_sensors_def); i++) {
		struct sensor_context *ctx = &basic_sensors_def[i];

		if (ctx->instances_count == 0) {
			continue;
		}

		if (anjay_ipso_basic_sensor_install(anjay, ctx->oid, ctx->instances_count)) {
			continue;
		}

		for (int j = 0; j < ctx->instances_count; j++) {
			struct basic_sensor_driver *driver = &ctx->drivers[j];

			if (driver->init()) {
				driver->installed = false;
				continue;
			}

			driver->installed = !anjay_ipso_basic_sensor_instance_add(
				anjay, ctx->oid, j,
				(anjay_ipso_basic_sensor_impl_t){ .unit = ctx->unit,
								  .user_context = ctx,
								  .min_range_value = NAN,
								  .max_range_value = NAN,
								  .get_value = read_value });
		}
	}
}

void basic_sensor_objects_update(anjay_t *anjay)
{
	for (int i = 0; i < AVS_ARRAY_SIZE(basic_sensors_def); i++) {
		struct sensor_context *ctx = &basic_sensors_def[i];

		for (int j = 0; j < ctx->instances_count; j++) {
			if (ctx->drivers[j].installed) {
				anjay_ipso_basic_sensor_update(anjay, ctx->oid, j);
			}
		}
	}
}
