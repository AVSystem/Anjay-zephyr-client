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

#include <assert.h>
#include <stdbool.h>

#include <anjay/anjay.h>
#include <anjay/ipso_objects.h>
#include <avsystem/commons/avs_defs.h>
#include <avsystem/commons/avs_list.h>
#include <avsystem/commons/avs_memory.h>

#include <devicetree.h>
#include <drivers/gpio.h>
#include <logging/log.h>

#include "objects.h"

LOG_MODULE_REGISTER(push_button);

#if PUSH_BUTTON_AVAILABLE_ANY
struct push_button_instance_glue {
	anjay_t *anjay;
	const struct device *dev;
	int gpio_pin;
	int gpio_flags;
	struct gpio_callback push_button_callback;
};

#define PUSH_BUTTON_GLUE_ITEM(num)                                                                 \
	{                                                                                          \
		.dev = DEVICE_DT_GET(DT_GPIO_CTLR(PUSH_BUTTON_NODE(num), gpios)),                  \
		.gpio_pin = DT_GPIO_PIN(PUSH_BUTTON_NODE(num), gpios),                             \
		.gpio_flags = (GPIO_INPUT | DT_GPIO_FLAGS(PUSH_BUTTON_NODE(num), gpios))           \
	}

static struct push_button_instance_glue button_glue[] = {
#if PUSH_BUTTON_AVAILABLE(0)
	PUSH_BUTTON_GLUE_ITEM(0),
#endif // PUSH_BUTTON_AVAILABLE(0)
#if PUSH_BUTTON_AVAILABLE(1)
	PUSH_BUTTON_GLUE_ITEM(1),
#endif // PUSH_BUTTON_AVAILABLE(1)
#if PUSH_BUTTON_AVAILABLE(2)
	PUSH_BUTTON_GLUE_ITEM(2),
#endif // PUSH_BUTTON_AVAILABLE(2)
};

#define BUTTON_CHANGE_WORKS_NUM 256

struct change_button_state_work {
	bool reserved;
	struct k_work work;
	anjay_t *anjay;
	anjay_iid_t iid;
	bool state;
};

static struct change_button_state_work button_change_works[BUTTON_CHANGE_WORKS_NUM];
static size_t last_work_slot;

static void button_change_state_handler(struct k_work *_work)
{
	struct change_button_state_work *work =
		CONTAINER_OF(_work, struct change_button_state_work, work);
	anjay_ipso_button_update(work->anjay, work->iid, work->state);
	work->reserved = false;
}

static void button_state_changed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	(void)pins;
	struct push_button_instance_glue *glue =
		AVS_CONTAINER_OF(cb, struct push_button_instance_glue, push_button_callback);
	struct change_button_state_work *work = NULL;

	for (int i = 0; i < BUTTON_CHANGE_WORKS_NUM; i++) {
		int slot_num = (last_work_slot + i + 1) % BUTTON_CHANGE_WORKS_NUM;

		if (!button_change_works[slot_num].reserved) {
			last_work_slot = slot_num;

			work = &button_change_works[slot_num];
			work->reserved = true;
			work->anjay = glue->anjay;
			work->state = (bool)gpio_pin_get(dev, glue->gpio_pin);
			work->iid = (anjay_iid_t)(((size_t)(glue - button_glue)) /
						  sizeof(struct push_button_instance_glue));

			k_work_init(&work->work, button_change_state_handler);

			if (k_work_submit(&work->work) == 1) {
				return;
			}

			break;
		}
	}

	LOG_ERR("Could not schedule the work");
}

static int configure_push_button(anjay_t *anjay, const struct device *dev, int gpio_pin,
				 int gpio_flags, anjay_iid_t iid,
				 struct push_button_instance_glue *glue)
{
	if (!device_is_ready(dev) || gpio_pin_configure(dev, gpio_pin, gpio_flags) ||
	    gpio_pin_interrupt_configure(dev, gpio_pin, GPIO_INT_EDGE_BOTH)) {
		return -1;
	}

	char application_type[40];

	sprintf(application_type, "Button %d", iid);
	if (anjay_ipso_button_instance_add(anjay, iid, application_type)) {
		return -1;
	}

	(void)anjay_ipso_button_update(anjay, iid, gpio_pin_get(dev, gpio_pin));

	gpio_init_callback(&glue->push_button_callback, button_state_changed, BIT(gpio_pin));
	glue->anjay = anjay;

	if (gpio_add_callback(dev, &glue->push_button_callback)) {
		gpio_pin_interrupt_configure(dev, gpio_pin, GPIO_INT_DISABLE);
		anjay_ipso_button_instance_remove(anjay, iid);
		return -1;
	}
	return 0;
}

int push_button_object_install(anjay_t *anjay)
{
	if (anjay_ipso_button_install(anjay, AVS_ARRAY_SIZE(button_glue))) {
		return -1;
	}

	for (anjay_iid_t iid = 0; iid < AVS_ARRAY_SIZE(button_glue); iid++) {
		configure_push_button(anjay, button_glue[iid].dev, button_glue[iid].gpio_pin,
				      button_glue[iid].gpio_flags, iid, &button_glue[iid]);
	}

	return 0;
}

#else // PUSH_BUTTON_AVAILABLE_ANY
int push_button_object_install(anjay_t *anjay)
{
	return 0;
}

void push_button_object_update(anjay_t *anjay)
{
}
#endif // PUSH_BUTTON_AVAILABLE_ANY
