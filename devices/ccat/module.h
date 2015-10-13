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

#ifndef _CCAT_H_
#define _CCAT_H_

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/hrtimer.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include "../ecdev.h"

#define DRV_EXTRAVERSION "-ec"
#define DRV_VERSION      "0.13" DRV_EXTRAVERSION
#define DRV_DESCRIPTION  "Beckhoff CCAT Ethernet/EtherCAT Network Driver"

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

extern struct ccat_driver eth_eim_driver;
extern struct ccat_driver eth_dma_driver;
extern struct ccat_driver gpio_driver;
extern struct ccat_driver sram_driver;
extern struct ccat_driver update_driver;

/**
 * CCAT function type identifiers (u16)
 */
enum ccat_info_t {
	CCATINFO_NOTUSED = 0,
	CCATINFO_ETHERCAT_NODMA = 0x3,
	CCATINFO_GPIO = 0xd,
	CCATINFO_EPCS_PROM = 0xf,
	CCATINFO_ETHERCAT_MASTER_DMA = 0x14,
	CCATINFO_SRAM = 0x16,
};

struct ccat_cdev {
	atomic_t in_use;
	void __iomem *ioaddr;
	size_t iosize;
	dev_t dev;
	struct cdev cdev;
	struct ccat_class *class;
};

/**
 * struct cdev_buffer
 * @ccdev: referenced character device
 * @data: buffer used for write operations
 * @size: number of bytes written to the data buffer
 */
struct cdev_buffer {
	struct ccat_cdev *ccdev;
	size_t size;
	char data[];
};

extern int ccat_cdev_open(struct inode *const i, struct file *const f);
extern int ccat_cdev_release(struct inode *const i, struct file *const f);

/**
 * struct ccat_device - CCAT device representation
 * @pdev: pointer to the pci object allocated by the kernel
 * @bar_0: holding information about PCI BAR 0
 * @bar_2: holding information about PCI BAR 2 (optional)
 * @functions: list of available (driver loaded) FPGA functions
 *
 * One instance of a ccat_device should represent a physical CCAT. Since
 * a CCAT is implemented as FPGA the available functions can vary.
 */
struct ccat_device {
	void *pdev;
	void __iomem *bar_0;
	void __iomem *bar_2;
	struct list_head functions;
};

struct ccat_info_block {
	u16 type;
	u16 rev;
	union {
		u32 config;
		u8 num_gpios;
		struct {
			u16 tx_size;
			u16 rx_size;
		};
		struct {
			u8 tx_dma_chan;
			u8 rx_dma_chan;
		};
		struct {
			u8 sram_width;
			u8 sram_size;
			u16 reserved;
		};
	};
	u32 addr;
	u32 size;
};

struct ccat_function {
	const struct ccat_driver *drv;
	struct ccat_device *ccat;
	struct ccat_info_block info;
	struct list_head list;
	void *private_data;
};

struct ccat_class {
	dev_t dev;
	struct class *class;
	atomic_t instances;
	const unsigned count;
	struct ccat_cdev *devices;
	const char *name;
	struct file_operations fops;
};

extern void ccat_cdev_remove(struct ccat_function *func);
extern int ccat_cdev_probe(struct ccat_function *func,
			   struct ccat_class *cdev_class, size_t iosize);

/**
 * struct ccat_driver - CCAT FPGA function
 * @probe: add device instance
 * @remove: remove device instance
 * @type: type of the FPGA function supported by this driver
 * @cdev_class: if not NULL that driver supports ccat_class_init()/_exit()
 */
struct ccat_driver {
	int (*probe) (struct ccat_function * func);
	void (*remove) (struct ccat_function * drv);
	enum ccat_info_t type;
	struct ccat_class *cdev_class;
};

#endif /* #ifndef _CCAT_H_ */
