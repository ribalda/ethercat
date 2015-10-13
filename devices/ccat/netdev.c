/**
    Network Driver for Beckhoff CCAT communication controller
    Copyright (C) 2014 - 2015  Beckhoff Automation GmbH
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>

#ifdef CONFIG_PCI
#include <asm/dma.h>
#else
#define free_dma(X)
#define request_dma(X, Y) ((int)(-EINVAL))
#endif

#include "module.h"

/**
 * EtherCAT frame to enable forwarding on EtherCAT Terminals
 */
static const u8 frameForwardEthernetFrames[] = {
	0x01, 0x01, 0x05, 0x01, 0x00, 0x00,
	0x00, 0x1b, 0x21, 0x36, 0x1b, 0xce,
	0x88, 0xa4, 0x0e, 0x10,
	0x08,
	0x00,
	0x00, 0x00,
	0x00, 0x01,
	0x02, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00
};

#define FIFO_LENGTH 64
#define POLL_TIME ktime_set(0, 50 * NSEC_PER_USEC)

struct ccat_dma_frame_hdr {
	__le32 reserved1;
	__le32 rx_flags;
#define CCAT_FRAME_RECEIVED 0x1
	__le16 length;
	__le16 reserved3;
	__le32 tx_flags;
#define CCAT_FRAME_SENT 0x1
	__le64 timestamp;
};

struct ccat_eim_frame_hdr {
	__le16 length;
	__le16 reserved3;
	__le32 tx_flags;
	__le64 timestamp;
};

struct ccat_eth_frame {
	u8 placeholder[0x800];
};

struct ccat_dma_frame {
	struct ccat_dma_frame_hdr hdr;
	u8 data[sizeof(struct ccat_eth_frame) -
		sizeof(struct ccat_dma_frame_hdr)];
};

struct ccat_eim_frame {
	struct ccat_eim_frame_hdr hdr;
	u8 data[sizeof(struct ccat_eth_frame) -
		sizeof(struct ccat_eim_frame_hdr)];
};

#define MAX_PAYLOAD_SIZE \
	(sizeof(struct ccat_eth_frame) - max(sizeof(struct ccat_dma_frame_hdr), sizeof(struct ccat_eim_frame_hdr)))

/**
 * struct ccat_eth_register - CCAT register addresses in the PCI BAR
 * @mii: address of the CCAT management interface register
 * @tx_fifo: address of the CCAT TX DMA fifo register
 * @rx_fifo: address of the CCAT RX DMA fifo register
 * @mac: address of the CCAT media access control register
 * @rx_mem: address of the CCAT register holding the RX DMA address
 * @tx_mem: address of the CCAT register holding the TX DMA address
 * @misc: address of a CCAT register holding miscellaneous information
 */
struct ccat_eth_register {
	void __iomem *mii;
	void __iomem *tx_fifo;
	void __iomem *rx_fifo;
	void __iomem *mac;
	void __iomem *rx_mem;
	void __iomem *tx_mem;
	void __iomem *misc;
};

/**
 * struct ccat_dma - CCAT DMA channel configuration
 * @phys: device-viewed address(physical) of the associated DMA memory
 * @start: CPU-viewed address(virtual) of the associated DMA memory
 * @size: number of bytes in the associated DMA memory
 * @channel: CCAT DMA channel number
 * @dev: valid struct device pointer
 */
struct ccat_dma {
	struct ccat_dma_frame *next;
	void *start;
	size_t size;
	dma_addr_t phys;
	size_t channel;
	struct device *dev;
};

struct ccat_eim {
	struct ccat_eim_frame __iomem *next;
	void __iomem *start;
	size_t size;
};

struct ccat_mem {
	struct ccat_eth_frame *next;
	void *start;
};

/**
 * struct ccat_eth_fifo - CCAT RX or TX DMA fifo
 * @add: callback used to add a frame to this fifo
 * @reg: PCI register address of this DMA fifo
 * @dma: information about the associated DMA memory
 */
struct ccat_eth_fifo {
	void (*add) (struct ccat_eth_fifo *);
	void (*copy_to_skb) (struct ccat_eth_fifo *, struct sk_buff *, size_t);
	void (*queue_skb) (struct ccat_eth_fifo * const, struct sk_buff *);
	void __iomem *reg;
	const struct ccat_eth_frame *end;
	union {
		struct ccat_mem mem;
		struct ccat_dma dma;
		struct ccat_eim eim;
	};
};

/**
 * same as: typedef struct _CCatInfoBlockOffs from CCatDefinitions.h
 */
struct ccat_mac_infoblock {
	u32 reserved;
	u32 mii;
	u32 tx_fifo;
	u32 mac;
	u32 rx_mem;
	u32 tx_mem;
	u32 misc;
};

/**
 * struct ccat_eth_priv - CCAT Ethernet/EtherCAT Master function (netdev)
 * @func: pointer to the parent struct ccat_function
 * @netdev: the net_device structure used by the kernel networking stack
 * @info: holds a copy of the CCAT Ethernet/EtherCAT Master function information block (read from PCI config space)
 * @reg: register addresses in PCI config space of the Ethernet/EtherCAT Master function
 * @rx_fifo: DMA fifo used for RX DMA descriptors
 * @tx_fifo: DMA fifo used for TX DMA descriptors
 * @poll_timer: interval timer used to poll CCAT for events like link changed, rx done, tx done
 * @rx_bytes: number of bytes received -> reported with ndo_get_stats64()
 * @rx_dropped: number of received frames, which were dropped -> reported with ndo_get_stats64()
 * @tx_bytes: number of bytes send -> reported with ndo_get_stats64()
 * @tx_dropped: number of frames requested to send, which were dropped -> reported with ndo_get_stats64()
 */
struct ccat_eth_priv {
	void (*free) (struct ccat_eth_priv *);
	 bool(*tx_ready) (const struct ccat_eth_priv *);
	 size_t(*rx_ready) (struct ccat_eth_fifo *);
	struct ccat_function *func;
	struct net_device *netdev;
	struct ccat_eth_register reg;
	struct ccat_eth_fifo rx_fifo;
	struct ccat_eth_fifo tx_fifo;
	struct hrtimer poll_timer;
	atomic64_t rx_bytes;
	atomic64_t rx_dropped;
	atomic64_t tx_bytes;
	atomic64_t tx_dropped;
	ec_device_t *ecdev;
	void (*carrier_off) (struct net_device * netdev);
	 bool(*carrier_ok) (const struct net_device * netdev);
	void (*carrier_on) (struct net_device * netdev);
	void (*kfree_skb_any) (struct sk_buff * skb);
	void (*receive) (struct ccat_eth_priv *, size_t);
	void (*start_queue) (struct net_device * netdev);
	void (*stop_queue) (struct net_device * netdev);
	void (*unregister) (struct net_device * netdev);
};

struct ccat_mac_register {
	/** MAC error register     @+0x0 */
	u8 frame_len_err;
	u8 rx_err;
	u8 crc_err;
	u8 link_lost_err;
	u32 reserved1;
	/** Buffer overflow errors @+0x8 */
	u8 rx_mem_full;
	u8 reserved2[7];
	/** MAC frame counter      @+0x10 */
	u32 tx_frames;
	u32 rx_frames;
	u64 reserved3;
	/** MAC fifo level         @+0x20 */
	u8 tx_fifo_level:7;
	u8 reserved4:1;
	u8 reserved5[7];
	/** TX memory full error   @+0x28 */
	u8 tx_mem_full;
	u8 reserved6[7];
	u64 reserved8[9];
	/** Connection             @+0x78 */
	u8 mii_connected;
};

static void ccat_dma_free(struct ccat_dma *const dma)
{
	const struct ccat_dma tmp = *dma;

	free_dma(dma->channel);
	memset(dma, 0, sizeof(*dma));
	dma_free_coherent(tmp.dev, tmp.size, tmp.start, tmp.phys);
}

/**
 * ccat_dma_init() - Initialize CCAT and host memory for DMA transfer
 * @dma object for management data which will be initialized
 * @channel number of the DMA channel
 * @ioaddr of the pci bar2 configspace used to calculate the address of the pci dma configuration
 * @dev which should be configured for DMA
 */
static int ccat_dma_init(struct ccat_dma *const dma, size_t channel,
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
	dma->start =
	    dma_zalloc_coherent(dev, dma->size, &dma->phys, GFP_KERNEL);
	if (!dma->start || !dma->phys) {
		pr_info("init DMA%llu memory failed.\n", (u64) channel);
		return -ENOMEM;
	}

	if (request_dma(channel, KBUILD_MODNAME)) {
		pr_info("request dma channel %llu failed\n", (u64) channel);
		ccat_dma_free(dma);
		return -EINVAL;
	}

	translateAddr = (dma->phys + memSize - PAGE_SIZE) & memTranslate;
	addr = translateAddr;
	memcpy_toio(ioaddr + offset, &addr, sizeof(addr));
	frame = dma->start + translateAddr - dma->phys;
	pr_debug
	    ("DMA%llu mem initialized\n start:         0x%p\n phys:         0x%llx\n translated:   0x%llx\n pci addr:     0x%08x%x\n memTranslate: 0x%x\n size:         %llu bytes.\n",
	     (u64) channel, dma->start, (u64) (dma->phys), addr,
	     ioread32(ioaddr + offset + 4), ioread32(ioaddr + offset),
	     memTranslate, (u64) dma->size);
	return 0;
}

static void ecdev_kfree_skb_any(struct sk_buff *skb)
{
	/* never release a skb in EtherCAT mode */
}

static bool ecdev_carrier_ok(const struct net_device *const netdev)
{
	struct ccat_eth_priv *const priv = netdev_priv(netdev);
	return ecdev_get_link(priv->ecdev);
}

static void ecdev_carrier_on(struct net_device *const netdev)
{
	struct ccat_eth_priv *const priv = netdev_priv(netdev);
	ecdev_set_link(priv->ecdev, 1);
}

static void ecdev_carrier_off(struct net_device *const netdev)
{
	struct ccat_eth_priv *const priv = netdev_priv(netdev);
	ecdev_set_link(priv->ecdev, 0);
}

static void ecdev_nop(struct net_device *const netdev)
{
	/* dummy called if nothing has to be done in EtherCAT operation mode */
}

static void ecdev_receive_dma(struct ccat_eth_priv *const priv, size_t len)
{
	ecdev_receive(priv->ecdev, priv->rx_fifo.dma.next->data, len);
}

static void ecdev_receive_eim(struct ccat_eth_priv *const priv, size_t len)
{
	ecdev_receive(priv->ecdev, priv->rx_fifo.eim.next->data, len);
}

static void unregister_ecdev(struct net_device *const netdev)
{
	struct ccat_eth_priv *const priv = netdev_priv(netdev);
	ecdev_close(priv->ecdev);
	ecdev_withdraw(priv->ecdev);
}

static inline bool fifo_eim_tx_ready(const struct ccat_eth_priv *const priv)
{
	static const size_t TX_FIFO_LEVEL_OFFSET = 0x20;
	static const u8 TX_FIFO_LEVEL_MASK = 0x3F;
	void __iomem *addr = priv->reg.mac + TX_FIFO_LEVEL_OFFSET;

	return !(ioread8(addr) & TX_FIFO_LEVEL_MASK);
}

static inline size_t fifo_eim_rx_ready(struct ccat_eth_fifo *const fifo)
{
	static const size_t OVERHEAD = sizeof(struct ccat_eim_frame_hdr);
	const size_t len = ioread16(&fifo->eim.next->hdr.length);

	return (len < OVERHEAD) ? 0 : len - OVERHEAD;
}

static void ccat_eth_fifo_inc(struct ccat_eth_fifo *fifo)
{
	if (++fifo->mem.next > fifo->end)
		fifo->mem.next = fifo->mem.start;
}

static void fifo_eim_rx_add(struct ccat_eth_fifo *const fifo)
{
	struct ccat_eim_frame __iomem *frame = fifo->eim.next;
	iowrite16(0, frame);
	wmb();
}

static void fifo_eim_tx_add(struct ccat_eth_fifo *const fifo)
{
}

#define memcpy_from_ccat(DEST, SRC, LEN) memcpy(DEST,(__force void*)(SRC), LEN)
#define memcpy_to_ccat(DEST, SRC, LEN) memcpy((__force void*)(DEST),SRC, LEN)
static void fifo_eim_copy_to_linear_skb(struct ccat_eth_fifo *const fifo,
					struct sk_buff *skb, const size_t len)
{
	memcpy_from_ccat(skb->data, fifo->eim.next->data, len);
}

static void fifo_eim_queue_skb(struct ccat_eth_fifo *const fifo,
			       struct sk_buff *skb)
{
	struct ccat_eim_frame __iomem *frame = fifo->eim.next;
	const u32 addr_and_length =
	    (void __iomem *)frame - (void __iomem *)fifo->eim.start;

	const __le16 length = cpu_to_le16(skb->len);
	memcpy_to_ccat(&frame->hdr.length, &length, sizeof(length));
	memcpy_to_ccat(frame->data, skb->data, skb->len);
	iowrite32(addr_and_length, fifo->reg);
}

static void ccat_eth_priv_free_eim(struct ccat_eth_priv *priv)
{
	/* reset hw fifo's */
	iowrite32(0, priv->tx_fifo.reg + 0x8);
	wmb();
}

static void ccat_eth_fifo_reset(struct ccat_eth_fifo *const fifo)
{
	/* reset hw fifo */
	if (fifo->reg) {
		iowrite32(0, fifo->reg + 0x8);
		wmb();
	}

	if (fifo->add) {
		fifo->mem.next = fifo->mem.start;
		do {
			fifo->add(fifo);
			ccat_eth_fifo_inc(fifo);
		} while (fifo->mem.next != fifo->mem.start);
	}
}

static inline bool fifo_dma_tx_ready(const struct ccat_eth_priv *const priv)
{
	const struct ccat_dma_frame *frame = priv->tx_fifo.dma.next;
	return le32_to_cpu(frame->hdr.tx_flags) & CCAT_FRAME_SENT;
}

static inline size_t fifo_dma_rx_ready(struct ccat_eth_fifo *const fifo)
{
	static const size_t OVERHEAD =
	    offsetof(struct ccat_dma_frame_hdr, rx_flags);
	const struct ccat_dma_frame *const frame = fifo->dma.next;

	if (le32_to_cpu(frame->hdr.rx_flags) & CCAT_FRAME_RECEIVED) {
		const size_t len = le16_to_cpu(frame->hdr.length);
		return (len < OVERHEAD) ? 0 : len - OVERHEAD;
	}
	return 0;
}

static void ccat_eth_rx_fifo_dma_add(struct ccat_eth_fifo *const fifo)
{
	struct ccat_dma_frame *const frame = fifo->dma.next;
	const size_t offset = (void *)frame - fifo->dma.start;
	const u32 addr_and_length = (1 << 31) | offset;

	frame->hdr.rx_flags = cpu_to_le32(0);
	iowrite32(addr_and_length, fifo->reg);
}

static void ccat_eth_tx_fifo_dma_add_free(struct ccat_eth_fifo *const fifo)
{
	/* mark frame as ready to use for tx */
	fifo->dma.next->hdr.tx_flags = cpu_to_le32(CCAT_FRAME_SENT);
}

static void fifo_dma_copy_to_linear_skb(struct ccat_eth_fifo *const fifo,
					struct sk_buff *skb, const size_t len)
{
	skb_copy_to_linear_data(skb, fifo->dma.next->data, len);
}

static void fifo_dma_queue_skb(struct ccat_eth_fifo *const fifo,
			       struct sk_buff *skb)
{
	struct ccat_dma_frame *frame = fifo->dma.next;
	u32 addr_and_length;

	frame->hdr.tx_flags = cpu_to_le32(0);
	frame->hdr.length = cpu_to_le16(skb->len);

	memcpy(frame->data, skb->data, skb->len);

	/* Queue frame into CCAT TX-FIFO, CCAT ignores the first 8 bytes of the tx descriptor */
	addr_and_length = offsetof(struct ccat_dma_frame_hdr, length);
	addr_and_length += ((void *)frame - fifo->dma.start);
	addr_and_length +=
	    ((skb->len + sizeof(struct ccat_dma_frame_hdr)) / 8) << 24;
	iowrite32(addr_and_length, fifo->reg);
}

static void ccat_eth_priv_free_dma(struct ccat_eth_priv *priv)
{
	/* reset hw fifo's */
	iowrite32(0, priv->rx_fifo.reg + 0x8);
	iowrite32(0, priv->tx_fifo.reg + 0x8);
	wmb();

	/* release dma */
	ccat_dma_free(&priv->rx_fifo.dma);
	ccat_dma_free(&priv->tx_fifo.dma);
}

/**
 * Initalizes both (Rx/Tx) DMA fifo's and related management structures
 */
static int ccat_eth_priv_init_dma(struct ccat_eth_priv *priv)
{
	struct ccat_function *const func = priv->func;
	struct pci_dev *pdev = func->ccat->pdev;
	int status = 0;
	priv->rx_ready = fifo_dma_rx_ready;
	priv->tx_ready = fifo_dma_tx_ready;
	priv->free = ccat_eth_priv_free_dma;

	status =
	    ccat_dma_init(&priv->rx_fifo.dma, func->info.rx_dma_chan,
			  func->ccat->bar_2, &pdev->dev);
	if (status) {
		pr_info("init RX DMA memory failed.\n");
		return status;
	}

	status =
	    ccat_dma_init(&priv->tx_fifo.dma, func->info.tx_dma_chan,
			  func->ccat->bar_2, &pdev->dev);
	if (status) {
		pr_info("init TX DMA memory failed.\n");
		ccat_dma_free(&priv->rx_fifo.dma);
		return status;
	}

	priv->rx_fifo.add = ccat_eth_rx_fifo_dma_add;
	priv->rx_fifo.copy_to_skb = fifo_dma_copy_to_linear_skb;
	priv->rx_fifo.queue_skb = NULL;
	priv->rx_fifo.end =
	    ((struct ccat_eth_frame *)priv->rx_fifo.dma.start) + FIFO_LENGTH -
	    1;
	priv->rx_fifo.reg = priv->reg.rx_fifo;
	ccat_eth_fifo_reset(&priv->rx_fifo);

	priv->tx_fifo.add = ccat_eth_tx_fifo_dma_add_free;
	priv->tx_fifo.copy_to_skb = NULL;
	priv->tx_fifo.queue_skb = fifo_dma_queue_skb;
	priv->tx_fifo.end =
	    ((struct ccat_eth_frame *)priv->tx_fifo.dma.start) + FIFO_LENGTH -
	    1;
	priv->tx_fifo.reg = priv->reg.tx_fifo;
	ccat_eth_fifo_reset(&priv->tx_fifo);

	/* disable MAC filter */
	iowrite8(0, priv->reg.mii + 0x8 + 6);
	wmb();
	return 0;
}

static int ccat_eth_priv_init_eim(struct ccat_eth_priv *priv)
{
	priv->rx_ready = fifo_eim_rx_ready;
	priv->tx_ready = fifo_eim_tx_ready;
	priv->free = ccat_eth_priv_free_eim;

	priv->rx_fifo.eim.start = priv->reg.rx_mem;
	priv->rx_fifo.eim.size = priv->func->info.rx_size;

	priv->rx_fifo.add = fifo_eim_rx_add;
	priv->rx_fifo.copy_to_skb = fifo_eim_copy_to_linear_skb;
	priv->rx_fifo.queue_skb = NULL;
	priv->rx_fifo.end = priv->rx_fifo.dma.start;
	priv->rx_fifo.reg = NULL;
	ccat_eth_fifo_reset(&priv->rx_fifo);

	priv->tx_fifo.eim.start = priv->reg.tx_mem;
	priv->tx_fifo.eim.size = priv->func->info.tx_size;

	priv->tx_fifo.add = fifo_eim_tx_add;
	priv->tx_fifo.copy_to_skb = NULL;
	priv->tx_fifo.queue_skb = fifo_eim_queue_skb;
	priv->tx_fifo.end =
	    priv->tx_fifo.dma.start + priv->tx_fifo.dma.size -
	    sizeof(struct ccat_eth_frame);
	priv->tx_fifo.reg = priv->reg.tx_fifo;
	ccat_eth_fifo_reset(&priv->tx_fifo);

	/* disable MAC filter */
	iowrite8(0, priv->reg.mii + 0x8 + 6);
	wmb();
	return 0;
}

/**
 * Initializes a struct ccat_eth_register with data from a corresponding
 * CCAT function.
 */
static void ccat_eth_priv_init_reg(struct ccat_eth_register *const reg,
				   const struct ccat_function *const func)
{
	struct ccat_mac_infoblock offsets;
	void __iomem *const func_base = func->ccat->bar_0 + func->info.addr;

	/* struct ccat_eth_fifo contains a union of ccat_dma, ccat_eim and ccat_mem
	 * the members next, start and size have to overlay the exact same memory,
	 * to support 'polymorphic' usage of them */
	BUILD_BUG_ON(offsetof(struct ccat_dma, next) !=
		     offsetof(struct ccat_mem, next));
	BUILD_BUG_ON(offsetof(struct ccat_dma, start) !=
		     offsetof(struct ccat_mem, start));
	BUILD_BUG_ON(offsetof(struct ccat_dma, next) !=
		     offsetof(struct ccat_eim, next));
	BUILD_BUG_ON(offsetof(struct ccat_dma, start) !=
		     offsetof(struct ccat_eim, start));
	BUILD_BUG_ON(offsetof(struct ccat_dma, size) !=
		     offsetof(struct ccat_eim, size));

	memcpy_fromio(&offsets, func_base, sizeof(offsets));
	reg->mii = func_base + offsets.mii;
	reg->tx_fifo = func_base + offsets.tx_fifo;
	reg->rx_fifo = func_base + offsets.tx_fifo + 0x10;
	reg->mac = func_base + offsets.mac;
	reg->rx_mem = func_base + offsets.rx_mem;
	reg->tx_mem = func_base + offsets.tx_mem;
	reg->misc = func_base + offsets.misc;
}

static netdev_tx_t ccat_eth_start_xmit(struct sk_buff *skb,
				       struct net_device *dev)
{
	struct ccat_eth_priv *const priv = netdev_priv(dev);
	struct ccat_eth_fifo *const fifo = &priv->tx_fifo;

	if (skb_is_nonlinear(skb)) {
		pr_warn("Non linear skb not supported -> drop frame.\n");
		atomic64_inc(&priv->tx_dropped);
		priv->kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if (skb->len > MAX_PAYLOAD_SIZE) {
		pr_warn("skb.len %llu exceeds dma buffer %llu -> drop frame.\n",
			(u64) skb->len, (u64) MAX_PAYLOAD_SIZE);
		atomic64_inc(&priv->tx_dropped);
		priv->kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if (!priv->tx_ready(priv)) {
		netdev_err(dev, "BUG! Tx Ring full when queue awake!\n");
		priv->stop_queue(priv->netdev);
		return NETDEV_TX_BUSY;
	}

	/* prepare frame in DMA memory */
	fifo->queue_skb(fifo, skb);

	/* update stats */
	atomic64_add(skb->len, &priv->tx_bytes);

	priv->kfree_skb_any(skb);

	ccat_eth_fifo_inc(fifo);
	/* stop queue if tx ring is full */

	if (!priv->tx_ready(priv)) {
		priv->stop_queue(priv->netdev);
	}
	return NETDEV_TX_OK;
}

/**
 * Function to transmit a raw buffer to the network (f.e. frameForwardEthernetFrames)
 * @dev a valid net_device
 * @data pointer to your raw buffer
 * @len number of bytes in the raw buffer to transmit
 */
static void ccat_eth_xmit_raw(struct net_device *dev, const char *const data,
			      size_t len)
{
	struct sk_buff *skb = dev_alloc_skb(len);

	skb->dev = dev;
	skb_copy_to_linear_data(skb, data, len);
	skb_put(skb, len);
	ccat_eth_start_xmit(skb, dev);
}

static void ccat_eth_receive(struct ccat_eth_priv *const priv, const size_t len)
{
	struct sk_buff *const skb = dev_alloc_skb(len + NET_IP_ALIGN);
	struct net_device *const dev = priv->netdev;

	if (!skb) {
		pr_info("%s() out of memory :-(\n", __FUNCTION__);
		atomic64_inc(&priv->rx_dropped);
		return;
	}
	skb->dev = dev;
	skb_reserve(skb, NET_IP_ALIGN);
	priv->rx_fifo.copy_to_skb(&priv->rx_fifo, skb, len);
	skb_put(skb, len);
	skb->protocol = eth_type_trans(skb, dev);
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	atomic64_add(len, &priv->rx_bytes);
	netif_rx(skb);
}

static void ccat_eth_link_down(struct net_device *const dev)
{
	struct ccat_eth_priv *const priv = netdev_priv(dev);

	priv->stop_queue(dev);
	priv->carrier_off(dev);
	netdev_info(dev, "NIC Link is Down\n");
}

static void ccat_eth_link_up(struct net_device *const dev)
{
	struct ccat_eth_priv *const priv = netdev_priv(dev);

	netdev_info(dev, "NIC Link is Up\n");
	/* TODO netdev_info(dev, "NIC Link is Up %u Mbps %s Duplex\n",
	   speed == SPEED_100 ? 100 : 10,
	   cmd.duplex == DUPLEX_FULL ? "Full" : "Half"); */

	ccat_eth_fifo_reset(&priv->rx_fifo);
	ccat_eth_fifo_reset(&priv->tx_fifo);

	/* TODO reset CCAT MAC register */

	ccat_eth_xmit_raw(dev, frameForwardEthernetFrames,
			  sizeof(frameForwardEthernetFrames));
	priv->carrier_on(dev);
	priv->start_queue(dev);
}

/**
 * Read link state from CCAT hardware
 * @return 1 if link is up, 0 if not
 */
inline static size_t ccat_eth_priv_read_link_state(const struct ccat_eth_priv
						   *const priv)
{
	return (1 << 24) == (ioread32(priv->reg.mii + 0x8 + 4) & (1 << 24));
}

/**
 * Poll for link state changes
 */
static void poll_link(struct ccat_eth_priv *const priv)
{
	const size_t link = ccat_eth_priv_read_link_state(priv);

	if (link != priv->carrier_ok(priv->netdev)) {
		if (link)
			ccat_eth_link_up(priv->netdev);
		else
			ccat_eth_link_down(priv->netdev);
	}
}

/**
 * Poll for available rx dma descriptors in ethernet operating mode
 */
static void poll_rx(struct ccat_eth_priv *const priv)
{
	struct ccat_eth_fifo *const fifo = &priv->rx_fifo;
	size_t rx_per_poll = FIFO_LENGTH / 2;
	size_t len = priv->rx_ready(fifo);

	while (len && --rx_per_poll) {
		priv->receive(priv, len);
		fifo->add(fifo);
		ccat_eth_fifo_inc(fifo);
		len = priv->rx_ready(fifo);
	}
}

static void ec_poll_rx(struct net_device *dev)
{
	struct ccat_eth_priv *const priv = netdev_priv(dev);
	poll_rx(priv);
}

/**
 * Poll for available tx dma descriptors in ethernet operating mode
 */
static void poll_tx(struct ccat_eth_priv *const priv)
{
	if (priv->tx_ready(priv)) {
		netif_wake_queue(priv->netdev);
	}
}

/**
 * Since CCAT doesn't support interrupts until now, we have to poll
 * some status bits to recognize things like link change etc.
 */
static enum hrtimer_restart poll_timer_callback(struct hrtimer *timer)
{
	struct ccat_eth_priv *const priv =
	    container_of(timer, struct ccat_eth_priv, poll_timer);

	poll_link(priv);
	if (!priv->ecdev) {
		poll_rx(priv);
		poll_tx(priv);
	}
	hrtimer_forward_now(timer, POLL_TIME);
	return HRTIMER_RESTART;
}

static struct rtnl_link_stats64 *ccat_eth_get_stats64(struct net_device *dev, struct rtnl_link_stats64
						      *storage)
{
	struct ccat_eth_priv *const priv = netdev_priv(dev);
	struct ccat_mac_register mac;
	memcpy_fromio(&mac, priv->reg.mac, sizeof(mac));
	storage->rx_packets = mac.rx_frames;	/* total packets received       */
	storage->tx_packets = mac.tx_frames;	/* total packets transmitted    */
	storage->rx_bytes = atomic64_read(&priv->rx_bytes);	/* total bytes received         */
	storage->tx_bytes = atomic64_read(&priv->tx_bytes);	/* total bytes transmitted      */
	storage->rx_errors = mac.frame_len_err + mac.rx_mem_full + mac.crc_err + mac.rx_err;	/* bad packets received         */
	storage->tx_errors = mac.tx_mem_full;	/* packet transmit problems     */
	storage->rx_dropped = atomic64_read(&priv->rx_dropped);	/* no space in linux buffers    */
	storage->tx_dropped = atomic64_read(&priv->tx_dropped);	/* no space available in linux  */
	//TODO __u64    multicast;              /* multicast packets received   */
	//TODO __u64    collisions;

	/* detailed rx_errors: */
	storage->rx_length_errors = mac.frame_len_err;
	storage->rx_over_errors = mac.rx_mem_full;	/* receiver ring buff overflow  */
	storage->rx_crc_errors = mac.crc_err;	/* recved pkt with crc error    */
	storage->rx_frame_errors = mac.rx_err;	/* recv'd frame alignment error */
	storage->rx_fifo_errors = mac.rx_mem_full;	/* recv'r fifo overrun          */
	//TODO __u64    rx_missed_errors;       /* receiver missed packet       */

	/* detailed tx_errors */
	//TODO __u64    tx_aborted_errors;
	//TODO __u64    tx_carrier_errors;
	//TODO __u64    tx_fifo_errors;
	//TODO __u64    tx_heartbeat_errors;
	//TODO __u64    tx_window_errors;

	/* for cslip etc */
	//TODO __u64    rx_compressed;
	//TODO __u64    tx_compressed;
	return storage;
}

static int ccat_eth_open(struct net_device *dev)
{
	struct ccat_eth_priv *const priv = netdev_priv(dev);

	hrtimer_init(&priv->poll_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	priv->poll_timer.function = poll_timer_callback;
	hrtimer_start(&priv->poll_timer, POLL_TIME, HRTIMER_MODE_REL);
	return 0;
}

static int ccat_eth_stop(struct net_device *dev)
{
	struct ccat_eth_priv *const priv = netdev_priv(dev);

	priv->stop_queue(dev);
	hrtimer_cancel(&priv->poll_timer);
	return 0;
}

static const struct net_device_ops ccat_eth_netdev_ops = {
	.ndo_get_stats64 = ccat_eth_get_stats64,
	.ndo_open = ccat_eth_open,
	.ndo_start_xmit = ccat_eth_start_xmit,
	.ndo_stop = ccat_eth_stop,
};

static struct ccat_eth_priv *ccat_eth_alloc_netdev(struct ccat_function *func)
{
	struct ccat_eth_priv *priv = NULL;
	struct net_device *const netdev = alloc_etherdev(sizeof(*priv));

	if (netdev) {
		priv = netdev_priv(netdev);
		memset(priv, 0, sizeof(*priv));
		priv->netdev = netdev;
		priv->func = func;
		ccat_eth_priv_init_reg(&priv->reg, func);
	}
	return priv;
}

static int ccat_eth_init_netdev(struct ccat_eth_priv *priv)
{
	int status;

	/* init netdev with MAC and stack callbacks */
	memcpy_fromio(priv->netdev->dev_addr, priv->reg.mii + 8,
		      priv->netdev->addr_len);
	priv->netdev->netdev_ops = &ccat_eth_netdev_ops;

	/* use as EtherCAT device? */
	priv->carrier_off = ecdev_carrier_off;
	priv->carrier_ok = ecdev_carrier_ok;
	priv->carrier_on = ecdev_carrier_on;
	priv->kfree_skb_any = ecdev_kfree_skb_any;

	/* It would be more intuitive to check for:
	 * if (priv->func->drv->type == CCATINFO_ETHERCAT_MASTER_DMA) {
	 * unfortunately priv->func->drv is not initialized until probe() returns.
	 * So we check if there is a rx dma fifo registered to determine dma/io mode */
	if (priv->rx_fifo.reg) {
		priv->receive = ecdev_receive_dma;
	} else {
		priv->receive = ecdev_receive_eim;
	}
	priv->start_queue = ecdev_nop;
	priv->stop_queue = ecdev_nop;
	priv->unregister = unregister_ecdev;
	priv->ecdev = ecdev_offer(priv->netdev, ec_poll_rx, THIS_MODULE);
	if (priv->ecdev) {
		priv->carrier_off(priv->netdev);
		if (ecdev_open(priv->ecdev)) {
			pr_info("unable to register network device.\n");
			ecdev_withdraw(priv->ecdev);
			priv->free(priv);
			free_netdev(priv->netdev);
			return -1;	// TODO return better error code
		}
		priv->func->private_data = priv;
		return 0;
	}

	/* EtherCAT disabled -> prepare normal ethernet mode */
	priv->carrier_off = netif_carrier_off;
	priv->carrier_ok = netif_carrier_ok;
	priv->carrier_on = netif_carrier_on;
	priv->kfree_skb_any = dev_kfree_skb_any;
	priv->receive = ccat_eth_receive;
	priv->start_queue = netif_start_queue;
	priv->stop_queue = netif_stop_queue;
	priv->unregister = unregister_netdev;
	priv->carrier_off(priv->netdev);

	status = register_netdev(priv->netdev);
	if (status) {
		pr_info("unable to register network device.\n");
		priv->free(priv);
		free_netdev(priv->netdev);
		return status;
	}
	pr_info("registered %s as network device.\n", priv->netdev->name);
	priv->func->private_data = priv;
	return 0;
}

static int ccat_eth_dma_probe(struct ccat_function *func)
{
	struct ccat_eth_priv *priv = ccat_eth_alloc_netdev(func);
	int status;

	if (!priv)
		return -ENOMEM;

	status = ccat_eth_priv_init_dma(priv);
	if (status) {
		pr_warn("%s(): DMA initialization failed.\n", __FUNCTION__);
		free_netdev(priv->netdev);
		return status;
	}
	return ccat_eth_init_netdev(priv);
}

static void ccat_eth_dma_remove(struct ccat_function *func)
{
	struct ccat_eth_priv *const eth = func->private_data;
	eth->unregister(eth->netdev);
	eth->free(eth);
	free_netdev(eth->netdev);
}

struct ccat_driver eth_dma_driver = {
	.type = CCATINFO_ETHERCAT_MASTER_DMA,
	.probe = ccat_eth_dma_probe,
	.remove = ccat_eth_dma_remove,
};

static int ccat_eth_eim_probe(struct ccat_function *func)
{
	struct ccat_eth_priv *priv = ccat_eth_alloc_netdev(func);
	int status;

	if (!priv)
		return -ENOMEM;

	status = ccat_eth_priv_init_eim(priv);
	if (status) {
		pr_warn("%s(): memory initialization failed.\n", __FUNCTION__);
		free_netdev(priv->netdev);
		return status;
	}
	return ccat_eth_init_netdev(priv);
}

static void ccat_eth_eim_remove(struct ccat_function *func)
{
	struct ccat_eth_priv *const eth = func->private_data;
	eth->unregister(eth->netdev);
	eth->free(eth);
	free_netdev(eth->netdev);
}

struct ccat_driver eth_eim_driver = {
	.type = CCATINFO_ETHERCAT_NODMA,
	.probe = ccat_eth_eim_probe,
	.remove = ccat_eth_eim_remove,
};
