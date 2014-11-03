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

#include <asm/dma.h>
#include <linux/etherdevice.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/version.h>
#include "module.h"
#include "netdev.h"
#include "update.h"

MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_AUTHOR("Patrick Bruenn <p.bruenn@beckhoff.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

static void ccat_bar_free(struct ccat_bar *bar)
{
	if (bar->ioaddr) {
		const struct ccat_bar tmp = *bar;
		memset(bar, 0, sizeof(*bar));
		iounmap(tmp.ioaddr);
		release_mem_region(tmp.start, tmp.len);
	} else {
		pr_warn("%s(): %p was already done.\n", __FUNCTION__, bar);
	}
}

/**
 * ccat_bar_init() - Initialize a CCAT pci bar
 * @bar object which should be initialized
 * @index 0 and 2 are valid for CCAT, meaning pci bar0 or pci bar2
 * @pdev the pci device as which the CCAT was recognized before
 *
 * Reading PCI config space; request and map memory region.
 */
static int ccat_bar_init(struct ccat_bar *bar, size_t index,
			 struct pci_dev *pdev)
{
	struct resource *res;

	bar->start = pci_resource_start(pdev, index);
	bar->end = pci_resource_end(pdev, index);
	bar->len = pci_resource_len(pdev, index);
	bar->flags = pci_resource_flags(pdev, index);
	if (!(IORESOURCE_MEM & bar->flags)) {
		pr_info("bar%llu is no mem_region -> abort.\n", (u64) index);
		return -EIO;
	}

	res = request_mem_region(bar->start, bar->len, KBUILD_MODNAME);
	if (!res) {
		pr_info("allocate mem_region failed.\n");
		return -EIO;
	}
	pr_debug("bar%llu at [%lx,%lx] len=%lu res: %p.\n", (u64) index,
		 bar->start, bar->end, bar->len, res);

	bar->ioaddr = ioremap(bar->start, bar->len);
	if (!bar->ioaddr) {
		pr_info("bar%llu ioremap failed.\n", (u64) index);
		release_mem_region(bar->start, bar->len);
		return -EIO;
	}
	pr_debug("bar%llu I/O mem mapped to %p.\n", (u64) index, bar->ioaddr);
	return 0;
}

void ccat_dma_free(struct ccat_dma *const dma)
{
	const struct ccat_dma tmp = *dma;

	free_dma(dma->channel);
	memset(dma, 0, sizeof(*dma));
	dma_free_coherent(tmp.dev, tmp.size, tmp.virt, tmp.phys);
}

/**
 * ccat_dma_init() - Initialize CCAT and host memory for DMA transfer
 * @dma object for management data which will be initialized
 * @channel number of the DMA channel
 * @ioaddr of the pci bar2 configspace used to calculate the address of the pci dma configuration
 * @dev which should be configured for DMA
 */
int ccat_dma_init(struct ccat_dma *const dma, size_t channel,
		  void __iomem * const ioaddr, struct device *const dev)
{
	void *frame;
	u64 addr;
	u32 translateAddr;
	u32 memTranslate;
	u32 memSize;
	u32 data = 0xffffffff;
	u32 offset = (sizeof(u64) * channel) + 0x1000;

	dma->channel = channel;
	dma->dev = dev;

	/* calculate size and alignments */
	iowrite32(data, ioaddr + offset);
	wmb();
	data = ioread32(ioaddr + offset);
	memTranslate = data & 0xfffffffc;
	memSize = (~memTranslate) + 1;
	dma->size = 2 * memSize - PAGE_SIZE;
	dma->virt = dma_zalloc_coherent(dev, dma->size, &dma->phys, GFP_KERNEL);
	if (!dma->virt || !dma->phys) {
		pr_info("init DMA%llu memory failed.\n", (u64) channel);
		return -1;
	}

	if (request_dma(channel, KBUILD_MODNAME)) {
		pr_info("request dma channel %llu failed\n", (u64) channel);
		ccat_dma_free(dma);
		return -1;
	}

	translateAddr = (dma->phys + memSize - PAGE_SIZE) & memTranslate;
	addr = translateAddr;
	memcpy_toio(ioaddr + offset, &addr, sizeof(addr));
	frame = dma->virt + translateAddr - dma->phys;
	pr_debug
	    ("DMA%llu mem initialized\n virt:         0x%p\n phys:         0x%llx\n translated:   0x%llx\n pci addr:     0x%08x%x\n memTranslate: 0x%x\n size:         %llu bytes.\n",
	     (u64) channel, dma->virt, (u64) (dma->phys), addr,
	     ioread32(ioaddr + offset + 4), ioread32(ioaddr + offset),
	     memTranslate, (u64) dma->size);
	return 0;
}

/**
 * Initialize all available CCAT functions.
 *
 * Return: count of failed functions
 */
static int ccat_functions_init(struct ccat_device *const ccatdev)
{
	static const size_t block_size = sizeof(struct ccat_info_block);
	void __iomem *addr = ccatdev->bar[0].ioaddr; /** first block is the CCAT information block entry */
	const u8 num_func = ioread8(addr + 4); /** number of CCAT function blocks is at offset 0x4 */
	const void __iomem *end = addr + (block_size * num_func);
	int status = 0;	/** count init function failures */

	while (addr < end) {
		const u8 type = ioread16(addr);
		switch (type) {
		case CCATINFO_NOTUSED:
			break;
		case CCATINFO_EPCS_PROM:
			pr_info("Found: CCAT update(EPCS_PROM) -> init()\n");
			ccatdev->update = ccat_update_init(ccatdev, addr);
			status += (NULL == ccatdev->update);
			break;
		case CCATINFO_ETHERCAT_MASTER_DMA:
			pr_info("Found: ETHERCAT_MASTER_DMA -> init()\n");
			ccatdev->ethdev = ccat_eth_init(ccatdev, addr);
			status += (NULL == ccatdev->ethdev);
			break;
		default:
			pr_info("Found: 0x%04x not supported\n", type);
			break;
		}
		addr += block_size;
	}
	return status;
}

/**
 * Destroy all previously initialized CCAT functions
 */
static void ccat_functions_remove(struct ccat_device *const ccatdev)
{
	if (!ccatdev->ethdev) {
		pr_warn("%s(): 'ethdev' was not initialized.\n", __FUNCTION__);
	} else {
		struct ccat_eth_priv *const ethdev = ccatdev->ethdev;
		ccatdev->ethdev = NULL;
		ccat_eth_remove(ethdev);
	}
	if (!ccatdev->update) {
		pr_warn("%s(): 'update' was not initialized.\n", __FUNCTION__);
	} else {
		struct ccat_update *const update = ccatdev->update;
		ccatdev->update = NULL;
		ccat_update_remove(update);
	}
}

static int ccat_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int status;
	u8 revision;
	struct ccat_device *ccatdev = kmalloc(sizeof(*ccatdev), GFP_KERNEL);

	if (!ccatdev) {
		pr_err("%s() out of memory.\n", __FUNCTION__);
		return -ENOMEM;
	}
	memset(ccatdev, 0, sizeof(*ccatdev));
	ccatdev->pdev = pdev;
	pci_set_drvdata(pdev, ccatdev);

	status = pci_enable_device_mem(pdev);
	if (status) {
		pr_info("enable %s failed: %d\n", pdev->dev.kobj.name, status);
		return status;
	}

	status = pci_read_config_byte(pdev, PCI_REVISION_ID, &revision);
	if (status) {
		pr_warn("read CCAT pci revision failed with %d\n", status);
		return status;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	if (!dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64))) {
		pr_debug("64 bit DMA supported, pci rev: %u\n", revision);
	} else if (!dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32))) {
		pr_debug("32 bit DMA supported, pci rev: %u\n", revision);
	} else {
		pr_warn("No suitable DMA available, pci rev: %u\n", revision);
	}
#else
	if (!dma_set_mask(&pdev->dev, DMA_BIT_MASK(64))) {
		pr_debug("64 bit DMA supported, pci rev: %u\n", revision);
	} else if (!dma_set_mask(&pdev->dev, DMA_BIT_MASK(32))) {
		pr_debug("32 bit DMA supported, pci rev: %u\n", revision);
	} else {
		pr_warn("No suitable DMA available, pci rev: %u\n", revision);
	}
#endif

	if (ccat_bar_init(&ccatdev->bar[0], 0, pdev)) {
		pr_warn("initialization of bar0 failed.\n");
		return -EIO;
	}

	if (ccat_bar_init(&ccatdev->bar[2], 2, pdev)) {
		pr_warn("initialization of bar2 failed.\n");
		return -EIO;
	}

	pci_set_master(pdev);
	if (ccat_functions_init(ccatdev)) {
		pr_warn("some functions couldn't be initialized\n");
	}
	return 0;
}

static void ccat_remove(struct pci_dev *pdev)
{
	struct ccat_device *ccatdev = pci_get_drvdata(pdev);

	if (ccatdev) {
		ccat_functions_remove(ccatdev);
		ccat_bar_free(&ccatdev->bar[2]);
		ccat_bar_free(&ccatdev->bar[0]);
		pci_disable_device(pdev);
		pci_set_drvdata(pdev, NULL);
		kfree(ccatdev);
	}
	pr_debug("%s() done.\n", __FUNCTION__);
}

#define PCI_DEVICE_ID_BECKHOFF_CCAT 0x5000
#define PCI_VENDOR_ID_BECKHOFF 0x15EC

static const struct pci_device_id pci_ids[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_BECKHOFF, PCI_DEVICE_ID_BECKHOFF_CCAT)},
	{0,},
};

#if 0 /* prevent auto-loading */
MODULE_DEVICE_TABLE(pci, pci_ids);
#endif

static struct pci_driver pci_driver = {
	.name = KBUILD_MODNAME,
	.id_table = pci_ids,
	.probe = ccat_probe,
	.remove = ccat_remove,
};

static void __exit ccat_exit_module(void)
{
	pci_unregister_driver(&pci_driver);
}

static int __init ccat_init_module(void)
{
	pr_info("%s, %s\n", DRV_DESCRIPTION, DRV_VERSION);
	return pci_register_driver(&pci_driver);
}

module_exit(ccat_exit_module);
module_init(ccat_init_module);
