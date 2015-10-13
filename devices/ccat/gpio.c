/**
    Network Driver for Beckhoff CCAT communication controller
    Copyright (C) 2014  Beckhoff Automation GmbH
    Author: Patrick Bruenn <p.bruenn@beckhoff.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/version.h>
#include "module.h"

/**
 * struct ccat_gpio - CCAT GPIO function
 * @ioaddr: PCI base address of the CCAT Update function
 * @info: holds a copy of the CCAT Update function information block (read from PCI config space)
 */
struct ccat_gpio {
	struct gpio_chip chip;
	void __iomem *ioaddr;
	struct mutex lock;
};

/** TODO implement in LED driver
	#define TC_RED 0x01
	#define TC_GREEN 0x02
	#define TC_BLUE 0x04
	#define FB1_RED 0x08
	#define FB1_GREEN 0x10
	#define FB1_BLUE 0x20
	#define FB2_RED 0x40
	#define FB2_GREEN 0x80
	#define FB2_BLUE 0x100
 */

static int set_bit_in_register(struct mutex *lock, void __iomem * ioaddr,
			       unsigned nr, int val)
{
	volatile unsigned long old;

	mutex_lock(lock);
	old = ioread32(ioaddr);
	val ? set_bit(nr, &old) : clear_bit(nr, &old);
	if (val)
		set_bit(nr, &old);
	else
		clear_bit(nr, &old);
	iowrite32(old, ioaddr);
	mutex_unlock(lock);
	return 0;
}

static int ccat_gpio_get_direction(struct gpio_chip *chip, unsigned nr)
{
	struct ccat_gpio *gdev = container_of(chip, struct ccat_gpio, chip);
	const size_t byte_offset = 4 * (nr / 32) + 0x8;
	const u32 mask = 1 << (nr % 32);

	return !(mask & ioread32(gdev->ioaddr + byte_offset));
}

static int ccat_gpio_direction_input(struct gpio_chip *chip, unsigned nr)
{
	struct ccat_gpio *gdev = container_of(chip, struct ccat_gpio, chip);

	return set_bit_in_register(&gdev->lock, gdev->ioaddr + 0x8, nr, 0);
}

static int ccat_gpio_direction_output(struct gpio_chip *chip, unsigned nr,
				      int val)
{
	struct ccat_gpio *gdev = container_of(chip, struct ccat_gpio, chip);

	return set_bit_in_register(&gdev->lock, gdev->ioaddr + 0x8, nr, 1);
}

static int ccat_gpio_get(struct gpio_chip *chip, unsigned nr)
{
	struct ccat_gpio *gdev = container_of(chip, struct ccat_gpio, chip);
	const size_t byte_off = 4 * (nr / 32);
	const int mask = 1 << (nr % 32);
	int dir_off;
	int value;

	/** omit direction changes before value was read */
	mutex_lock(&gdev->lock);
	dir_off = 0x10 * ccat_gpio_get_direction(chip, nr);
	value = !(mask & ioread32(gdev->ioaddr + byte_off + dir_off));
	mutex_unlock(&gdev->lock);
	return value;
}

static void ccat_gpio_set(struct gpio_chip *chip, unsigned nr, int val)
{
	struct ccat_gpio *gdev = container_of(chip, struct ccat_gpio, chip);

	set_bit_in_register(&gdev->lock, gdev->ioaddr, nr, val);
}

static const struct gpio_chip ccat_gpio_chip = {
	.label = KBUILD_MODNAME,
	.owner = THIS_MODULE,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,12,0))
	.get_direction = ccat_gpio_get_direction,
#endif
	.direction_input = ccat_gpio_direction_input,
	.get = ccat_gpio_get,
	.direction_output = ccat_gpio_direction_output,
	.set = ccat_gpio_set,
	.dbg_show = NULL,
	.base = -1,
	.can_sleep = false
};

static int ccat_gpio_probe(struct ccat_function *func)
{
	struct ccat_gpio *const gpio = kzalloc(sizeof(*gpio), GFP_KERNEL);
	int ret;

	if (!gpio)
		return -ENOMEM;

	gpio->ioaddr = func->ccat->bar_0 + func->info.addr;
	memcpy(&gpio->chip, &ccat_gpio_chip, sizeof(gpio->chip));
	gpio->chip.ngpio = func->info.num_gpios;
	mutex_init(&gpio->lock);

	ret = gpiochip_add(&gpio->chip);
	if (ret) {
		kfree(gpio);
		return ret;
	}
	pr_info("registered %s as gpio chip with #%d GPIOs.\n",
		gpio->chip.label, gpio->chip.ngpio);
	func->private_data = gpio;
	return 0;
}

static void ccat_gpio_remove(struct ccat_function *func)
{
	struct ccat_gpio *const gpio = func->private_data;

	gpiochip_remove(&gpio->chip);
};

struct ccat_driver gpio_driver = {
	.type = CCATINFO_GPIO,
	.probe = ccat_gpio_probe,
	.remove = ccat_gpio_remove,
};
