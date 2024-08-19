/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Modifications Copyright 2023 NXP
 */

#ifndef __LINUX_GPIO_CONSUMER_H
#define __LINUX_GPIO_CONSUMER_H

#include <linux/compiler_types.h>
#include "os/windows/src/comm.h"

struct device;

struct gpio_desc {
	unsigned long		flags;
/* flag symbols are bit numbers */
#define FLAG_ACTIVE_LOW	BIT(6)	/* value has active low */

	struct iotarget_handles io_target;
};

#define devm_gpiod_get_optional devm_gpiod_get
struct gpio_desc* __must_check devm_gpiod_get(struct device* dev,
	const char* con_id,
	unsigned long flags);

void devm_gpiod_put(struct device* dev, struct gpio_desc* desc);

#define gpiod_set_value_cansleep gpiod_set_value
void gpiod_set_value(struct gpio_desc *desc, int value);

#endif
