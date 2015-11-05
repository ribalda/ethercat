/**
    Network Driver for Beckhoff CCAT communication controller
    Copyright (C) 2014-2015  Beckhoff Automation GmbH
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

#include <linux/etherdevice.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include "module.h"

MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_AUTHOR("Patrick Bruenn <p.bruenn@beckhoff.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,12,27))
/*
 * Set both the DMA mask and the coherent DMA mask to the same thing.
 * Note that we don't check the return value from dma_set_coherent_mask()
 * as the DMA API guarantees that the coherent DMA mask can be set to
 * the same or smaller than the streaming DMA mask.
 */
static inline int dma_set_mask_and_coherent(struct device *dev, u64 mask)
{
	int rc = dma_set_mask(dev, mask);
	if (rc == 0)
		dma_set_coherent_mask(dev, mask);
	return rc;
}
#endif

/**
 * configure the drivers capabilities here
 */
static const struct ccat_driver *const drivers[] = {
#ifdef CONFIG_PCI
	&eth_dma_driver,	/* load Ethernet MAC/EtherCAT Master driver with DMA support from netdev.c */
#endif
	&eth_eim_driver,	/* load Ethernet MAC/EtherCAT Master driver without DMA support from */
	&gpio_driver,		/* load GPIO driver from gpio.c */
	&sram_driver,		/* load SRAM driver from sram.c */
	&update_driver,		/* load Update driver from update.c */
};

static int __init ccat_class_init(struct ccat_class *base)
{
	if (1 == atomic_inc_return(&base->instances)) {
		if (alloc_chrdev_region
		    (&base->dev, 0, base->count, KBUILD_MODNAME)) {
			pr_warn("alloc_chrdev_region() for '%s' failed\n",
				base->name);
			return -1;
		}

		base->class = class_create(THIS_MODULE, base->name);
		if (!base->class) {
			pr_warn("Create device class '%s' failed\n",
				base->name);
			unregister_chrdev_region(base->dev, base->count);
			return -1;
		}
	}
	return 0;
}

static void ccat_class_exit(struct ccat_class *base)
{
	if (!atomic_dec_return(&base->instances)) {
		class_destroy(base->class);
		unregister_chrdev_region(base->dev, base->count);
	}
}

static void free_ccat_cdev(struct ccat_cdev *ccdev)
{
	ccat_class_exit(ccdev->class);
	ccdev->dev = 0;
}

static struct ccat_cdev *alloc_ccat_cdev(struct ccat_class *base)
{
	int i = 0;

	ccat_class_init(base);
	for (i = 0; i < base->count; ++i) {
		if (base->devices[i].dev == 0) {
			base->devices[i].dev = MKDEV(MAJOR(base->dev), i);
			return &base->devices[i];
		}
	}
	pr_warn("exceeding max. number of '%s' devices (%d)\n",
		base->class->name, base->count);
	atomic_dec_return(&base->instances);
	return NULL;
}

static int ccat_cdev_init(struct cdev *cdev, dev_t dev, struct class *class,
			  struct file_operations *fops)
{
	if (!device_create
	    (class, NULL, dev, NULL, "%s%d", class->name, MINOR(dev))) {
		pr_warn("device_create() failed\n");
		return -1;
	}

	cdev_init(cdev, fops);
	cdev->owner = fops->owner;
	if (cdev_add(cdev, dev, 1)) {
		pr_warn("add update device failed\n");
		device_destroy(class, dev);
		return -1;
	}

	pr_info("registered %s%d.\n", class->name, MINOR(dev));
	return 0;
}

int ccat_cdev_open(struct inode *const i, struct file *const f)
{
	struct ccat_cdev *ccdev =
	    container_of(i->i_cdev, struct ccat_cdev, cdev);
	struct cdev_buffer *buf;

	if (!atomic_dec_and_test(&ccdev->in_use)) {
		atomic_inc(&ccdev->in_use);
		return -EBUSY;
	}

	buf = kzalloc(sizeof(*buf) + ccdev->iosize, GFP_KERNEL);
	if (!buf) {
		atomic_inc(&ccdev->in_use);
		return -ENOMEM;
	}

	buf->ccdev = ccdev;
	f->private_data = buf;
	return 0;
}

int ccat_cdev_probe(struct ccat_function *func, struct ccat_class *cdev_class,
		    size_t iosize)
{
	struct ccat_cdev *const ccdev = alloc_ccat_cdev(cdev_class);
	if (!ccdev) {
		return -ENOMEM;
	}

	ccdev->ioaddr = func->ccat->bar_0 + func->info.addr;
	ccdev->iosize = iosize;
	atomic_set(&ccdev->in_use, 1);

	if (ccat_cdev_init
	    (&ccdev->cdev, ccdev->dev, cdev_class->class, &cdev_class->fops)) {
		pr_warn("ccat_cdev_probe() failed\n");
		free_ccat_cdev(ccdev);
		return -1;
	}
	ccdev->class = cdev_class;
	func->private_data = ccdev;
	return 0;
}

int ccat_cdev_release(struct inode *const i, struct file *const f)
{
	const struct cdev_buffer *const buf = f->private_data;
	struct ccat_cdev *const ccdev = buf->ccdev;

	kfree(f->private_data);
	atomic_inc(&ccdev->in_use);
	return 0;
}

void ccat_cdev_remove(struct ccat_function *func)
{
	struct ccat_cdev *const ccdev = func->private_data;

	cdev_del(&ccdev->cdev);
	device_destroy(ccdev->class->class, ccdev->dev);
	free_ccat_cdev(ccdev);
}

static const struct ccat_driver *ccat_function_connect(struct ccat_function
						       *const func)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(drivers); ++i) {
		if (func->info.type == drivers[i]->type) {
			return drivers[i]->probe(func) ? NULL : drivers[i];
		}
	}
	return NULL;
}

/**
 * Initialize all available CCAT functions.
 *
 * Return: count of failed functions
 */
static int ccat_functions_init(struct ccat_device *const ccatdev)
{
	static const size_t block_size = sizeof(struct ccat_info_block);
	struct ccat_function *next = kzalloc(sizeof(*next), GFP_KERNEL);
	void __iomem *addr = ccatdev->bar_0; /** first block is the CCAT information block entry */
	const u8 num_func = ioread8(addr + 4); /** number of CCAT function blocks is at offset 0x4 */
	const void __iomem *end = addr + (block_size * num_func);

	INIT_LIST_HEAD(&ccatdev->functions);
	for (; addr < end && next; addr += block_size) {
		memcpy_fromio(&next->info, addr, sizeof(next->info));
		if (CCATINFO_NOTUSED != next->info.type) {
			next->ccat = ccatdev;
			next->drv = ccat_function_connect(next);
			if (next->drv) {
				list_add(&next->list, &ccatdev->functions);
				next = kzalloc(sizeof(*next), GFP_KERNEL);
			}
		}
	}
	kfree(next);
	return list_empty(&ccatdev->functions);
}

/**
 * Destroy all previously initialized CCAT functions
 */
static void ccat_functions_remove(struct ccat_device *const dev)
{
	struct ccat_function *func;
	struct ccat_function *tmp;
	list_for_each_entry_safe(func, tmp, &dev->functions, list) {
		if (func->drv) {
			func->drv->remove(func);
			func->drv = NULL;
		}
		list_del(&func->list);
		kfree(func);
	}
}

#ifdef CONFIG_PCI
static int ccat_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct ccat_device *ccatdev;
	u8 rev;
	int status;

	ccatdev = devm_kzalloc(&pdev->dev, sizeof(*ccatdev), GFP_KERNEL);
	if (!ccatdev) {
		pr_err("%s() out of memory.\n", __FUNCTION__);
		return -ENOMEM;
	}
	ccatdev->pdev = pdev;
	pci_set_drvdata(pdev, ccatdev);

	status = pci_enable_device_mem(pdev);
	if (status) {
		pr_err("enable %s failed: %d\n", pdev->dev.kobj.name, status);
		return status;
	}

	status = pci_read_config_byte(pdev, PCI_REVISION_ID, &rev);
	if (status) {
		pr_err("read CCAT pci revision failed with %d\n", status);
		goto disable_device;
	}

	status = pci_request_regions(pdev, KBUILD_MODNAME);
	if (status) {
		pr_err("allocate mem_regions failed.\n");
		goto disable_device;
	}

	status = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (status) {
		status =
		    dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
		if (status) {
			pr_err("No suitable DMA available, pci rev: %u\n", rev);
			goto release_regions;
		}
		pr_debug("32 bit DMA supported, pci rev: %u\n", rev);
	} else {
		pr_debug("64 bit DMA supported, pci rev: %u\n", rev);
	}

	ccatdev->bar_0 = pci_iomap(pdev, 0, 0);
	if (!ccatdev->bar_0) {
		pr_err("initialization of bar0 failed.\n");
		status = -EIO;
		goto release_regions;
	}

	ccatdev->bar_2 = pci_iomap(pdev, 2, 0);
	if (!ccatdev->bar_2) {
		pr_warn("initialization of optional bar2 failed.\n");
	}

	pci_set_master(pdev);
	if (ccat_functions_init(ccatdev)) {
		pr_warn("some functions couldn't be initialized\n");
	}
	return 0;

release_regions:
	pci_release_regions(pdev);
disable_device:
	pci_disable_device(pdev);
	return status;
}

static void ccat_pci_remove(struct pci_dev *pdev)
{
	struct ccat_device *ccatdev = pci_get_drvdata(pdev);

	if (ccatdev) {
		ccat_functions_remove(ccatdev);
		if (ccatdev->bar_2)
			pci_iounmap(pdev, ccatdev->bar_2);
		pci_iounmap(pdev, ccatdev->bar_0);
		pci_release_regions(pdev);
		pci_disable_device(pdev);
	}
}

#define PCI_DEVICE_ID_BECKHOFF_CCAT 0x5000
#define PCI_VENDOR_ID_BECKHOFF 0x15EC

static const struct pci_device_id pci_ids[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_BECKHOFF, PCI_DEVICE_ID_BECKHOFF_CCAT)},
	{0,},
};

MODULE_DEVICE_TABLE(pci, pci_ids);

static struct pci_driver ccat_pci_driver = {
	.name = KBUILD_MODNAME,
	.id_table = pci_ids,
	.probe = ccat_pci_probe,
	.remove = ccat_pci_remove,
};

module_pci_driver(ccat_pci_driver);

#else /* #ifdef CONFIG_PCI */

static int ccat_eim_probe(struct platform_device *pdev)
{
	struct ccat_device *ccatdev;

	ccatdev = devm_kzalloc(&pdev->dev, sizeof(*ccatdev), GFP_KERNEL);
	if (!ccatdev) {
		pr_err("%s() out of memory.\n", __FUNCTION__);
		return -ENOMEM;
	}
	ccatdev->pdev = pdev;
	platform_set_drvdata(pdev, ccatdev);

	if (!request_mem_region(0xf0000000, 0x02000000, pdev->name)) {
		pr_warn("request mem region failed.\n");
		return -EIO;
	}

	if (!(ccatdev->bar_0 = ioremap(0xf0000000, 0x02000000))) {
		pr_warn("initialization of bar0 failed.\n");
		return -EIO;
	}

	ccatdev->bar_2 = NULL;

	if (ccat_functions_init(ccatdev)) {
		pr_warn("some functions couldn't be initialized\n");
	}
	return 0;
}

static int ccat_eim_remove(struct platform_device *pdev)
{
	struct ccat_device *ccatdev = platform_get_drvdata(pdev);

	if (ccatdev) {
		ccat_functions_remove(ccatdev);
		iounmap(ccatdev->bar_0);
		release_mem_region(0xf0000000, 0x02000000);
	}
	return 0;
}

static const struct of_device_id bhf_eim_ccat_ids[] = {
	{.compatible = "bhf,emi-ccat",},
	{}
};

MODULE_DEVICE_TABLE(of, bhf_eim_ccat_ids);

static struct platform_driver ccat_eim_driver = {
	.driver = {
		   .name = KBUILD_MODNAME,
		   .of_match_table = bhf_eim_ccat_ids,
		   },
	.probe = ccat_eim_probe,
	.remove = ccat_eim_remove,
};

module_platform_driver(ccat_eim_driver);
#endif /* #ifdef CONFIG_PCI */
