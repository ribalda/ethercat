/**
    Network Driver for Beckhoff CCAT communication controller
    Copyright (C) 2015  Beckhoff Automation GmbH & Co. KG
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

#include "module.h"
#include <asm/io.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#define CCAT_SRAM_DEVICES_MAX 4

static ssize_t __sram_read(struct cdev_buffer *buffer, char __user * buf,
			   size_t len, loff_t * off)
{
	memcpy_fromio(buffer->data, buffer->ccdev->ioaddr + *off, len);
	if (copy_to_user(buf, buffer->data, len))
		return -EFAULT;

	*off += len;
	return len;
}

static ssize_t ccat_sram_read(struct file *const f, char __user * buf,
			      size_t len, loff_t * off)
{
	struct cdev_buffer *buffer = f->private_data;
	const size_t iosize = buffer->ccdev->iosize;

	if (*off >= iosize) {
		return 0;
	}

	len = min(len, (size_t) (iosize - *off));

	return __sram_read(buffer, buf, len, off);
}

static ssize_t ccat_sram_write(struct file *const f, const char __user * buf,
			       size_t len, loff_t * off)
{
	struct cdev_buffer *const buffer = f->private_data;

	if (*off + len > buffer->ccdev->iosize) {
		return 0;
	}

	if (copy_from_user(buffer->data, buf, len)) {
		return -EFAULT;
	}

	memcpy_toio(buffer->ccdev->ioaddr + *off, buffer->data, len);

	*off += len;
	return len;
}

static struct ccat_cdev dev_table[CCAT_SRAM_DEVICES_MAX];
static struct ccat_class cdev_class = {
	.instances = {0},
	.count = CCAT_SRAM_DEVICES_MAX,
	.devices = dev_table,
	.name = "ccat_sram",
	.fops = {
		 .owner = THIS_MODULE,
		 .open = ccat_cdev_open,
		 .release = ccat_cdev_release,
		 .read = ccat_sram_read,
		 .write = ccat_sram_write,
		 },
};

static int ccat_sram_probe(struct ccat_function *func)
{
	static const u8 NO_SRAM_CONNECTED = 0;
	const u8 type = func->info.sram_width & 0x3;
	const size_t iosize = (1 << func->info.sram_size);

	pr_info("%s: 0x%04x rev: 0x%04x\n", __FUNCTION__, func->info.type,
		func->info.rev);
	if (type == NO_SRAM_CONNECTED) {
		return -ENODEV;
	}
	return ccat_cdev_probe(func, &cdev_class, iosize);
}

const struct ccat_driver sram_driver = {
	.type = CCATINFO_SRAM,
	.probe = ccat_sram_probe,
	.remove = ccat_cdev_remove,
	.cdev_class = &cdev_class,
};
