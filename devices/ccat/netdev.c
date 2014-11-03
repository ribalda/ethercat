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

#include <linux/etherdevice.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>

#include "module.h"
#include "netdev.h"

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
#define POLL_TIME ktime_set(0, 100 * NSEC_PER_USEC)

/**
 * Helper to check if frame in tx dma memory was already marked as sent by CCAT
 */
static inline bool ccat_eth_frame_sent(const struct ccat_eth_frame *const frame)
{
	return le32_to_cpu(frame->tx_flags) & CCAT_FRAME_SENT;
}

/**
 * Helper to check if frame in tx dma memory was already marked as sent by CCAT
 */
static inline bool ccat_eth_frame_received(const struct ccat_eth_frame *const
					   frame)
{
	return le32_to_cpu(frame->rx_flags) & CCAT_FRAME_RECEIVED;
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

static void unregister_ecdev(struct net_device *const netdev)
{
	struct ccat_eth_priv *const priv = netdev_priv(netdev);
	ecdev_close(priv->ecdev);
	ecdev_withdraw(priv->ecdev);
}

static void ccat_eth_fifo_inc(struct ccat_eth_dma_fifo *fifo)
{
	if (++fifo->next >= fifo->end)
		fifo->next = fifo->dma.virt;
}

typedef void (*fifo_add_function) (struct ccat_eth_dma_fifo *,
				   struct ccat_eth_frame *);

static void ccat_eth_rx_fifo_add(struct ccat_eth_dma_fifo *fifo,
				 struct ccat_eth_frame *frame)
{
	const size_t offset = ((void *)(frame) - fifo->dma.virt);
	const u32 addr_and_length = (1 << 31) | offset;

	frame->rx_flags = cpu_to_le32(0);
	iowrite32(addr_and_length, fifo->reg);
}

static void ccat_eth_tx_fifo_add_free(struct ccat_eth_dma_fifo *fifo,
				      struct ccat_eth_frame *frame)
{
	/* mark frame as ready to use for tx */
	frame->tx_flags = cpu_to_le32(CCAT_FRAME_SENT);
}

static void ccat_eth_dma_fifo_reset(struct ccat_eth_dma_fifo *fifo)
{
	/* reset hw fifo */
	iowrite32(0, fifo->reg + 0x8);
	wmb();

	if (fifo->add) {
		fifo->next = fifo->dma.virt;
		do {
			fifo->add(fifo, fifo->next);
			ccat_eth_fifo_inc(fifo);
		} while (fifo->next != fifo->dma.virt);
	}
}

static int ccat_eth_dma_fifo_init(struct ccat_eth_dma_fifo *fifo,
				  void __iomem * const fifo_reg,
				  fifo_add_function add, size_t channel,
				  struct ccat_eth_priv *const priv)
{
	if (0 !=
	    ccat_dma_init(&fifo->dma, channel, priv->ccatdev->bar[2].ioaddr,
			  &priv->ccatdev->pdev->dev)) {
		pr_info("init DMA%llu memory failed.\n", (u64) channel);
		return -1;
	}
	fifo->add = add;
	fifo->end = ((struct ccat_eth_frame *)fifo->dma.virt) + FIFO_LENGTH;
	fifo->reg = fifo_reg;
	return 0;
}

/**
 * Stop both (Rx/Tx) DMA fifo's and free related management structures
 */
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
	if (ccat_eth_dma_fifo_init
	    (&priv->rx_fifo, priv->reg.rx_fifo, ccat_eth_rx_fifo_add,
	     priv->info.rx_dma_chan, priv)) {
		pr_warn("init Rx DMA fifo failed.\n");
		return -1;
	}

	if (ccat_eth_dma_fifo_init
	    (&priv->tx_fifo, priv->reg.tx_fifo, ccat_eth_tx_fifo_add_free,
	     priv->info.tx_dma_chan, priv)) {
		pr_warn("init Tx DMA fifo failed.\n");
		ccat_dma_free(&priv->rx_fifo.dma);
		return -1;
	}

	/* disable MAC filter */
	iowrite8(0, priv->reg.mii + 0x8 + 6);
	wmb();
	return 0;
}

/**
 * Initializes the CCat... members of the ccat_eth_priv structure.
 * Call this function only if info and ioaddr are already initialized!
 */
static void ccat_eth_priv_init_mappings(struct ccat_eth_priv *priv)
{
	struct ccat_mac_infoblock offsets;
	void __iomem *const func_base =
	    priv->ccatdev->bar[0].ioaddr + priv->info.addr;

	memcpy_fromio(&offsets, func_base, sizeof(offsets));
	priv->reg.mii = func_base + offsets.mii;
	priv->reg.tx_fifo = func_base + offsets.tx_fifo;
	priv->reg.rx_fifo = func_base + offsets.tx_fifo + 0x10;
	priv->reg.mac = func_base + offsets.mac;
	priv->reg.rx_mem = func_base + offsets.rx_mem;
	priv->reg.tx_mem = func_base + offsets.tx_mem;
	priv->reg.misc = func_base + offsets.misc;
}

static netdev_tx_t ccat_eth_start_xmit(struct sk_buff *skb,
				       struct net_device *dev)
{
	struct ccat_eth_priv *const priv = netdev_priv(dev);
	struct ccat_eth_dma_fifo *const fifo = &priv->tx_fifo;
	u32 addr_and_length;

	if (skb_is_nonlinear(skb)) {
		pr_warn("Non linear skb not supported -> drop frame.\n");
		atomic64_inc(&priv->tx_dropped);
		priv->kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if (skb->len > sizeof(fifo->next->data)) {
		pr_warn("skb.len %llu exceeds dma buffer %llu -> drop frame.\n",
			(u64) skb->len, (u64) sizeof(fifo->next->data));
		atomic64_inc(&priv->tx_dropped);
		priv->kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if (!ccat_eth_frame_sent(fifo->next)) {
		netdev_err(dev, "BUG! Tx Ring full when queue awake!\n");
		priv->stop_queue(priv->netdev);
		return NETDEV_TX_BUSY;
	}

	/* prepare frame in DMA memory */
	fifo->next->tx_flags = cpu_to_le32(0);
	fifo->next->length = cpu_to_le16(skb->len);
	memcpy(fifo->next->data, skb->data, skb->len);

	/* Queue frame into CCAT TX-FIFO, CCAT ignores the first 8 bytes of the tx descriptor */
	addr_and_length = offsetof(struct ccat_eth_frame, length);
	addr_and_length += ((void *)fifo->next - fifo->dma.virt);
	addr_and_length += ((skb->len + CCAT_ETH_FRAME_HEAD_LEN) / 8) << 24;
	iowrite32(addr_and_length, priv->reg.tx_fifo);

	/* update stats */
	atomic64_add(skb->len, &priv->tx_bytes);

	priv->kfree_skb_any(skb);

	ccat_eth_fifo_inc(fifo);
	/* stop queue if tx ring is full */
	if (!ccat_eth_frame_sent(fifo->next)) {
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

static void ccat_eth_receive(struct net_device *const dev,
			     const void *const data, const size_t len)
{
	struct sk_buff *const skb = dev_alloc_skb(len + NET_IP_ALIGN);
	struct ccat_eth_priv *const priv = netdev_priv(dev);

	if (!skb) {
		pr_info("%s() out of memory :-(\n", __FUNCTION__);
		atomic64_inc(&priv->rx_dropped);
		return;
	}
	skb->dev = dev;
	skb_reserve(skb, NET_IP_ALIGN);
	skb_copy_to_linear_data(skb, data, len);
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

	ccat_eth_dma_fifo_reset(&priv->rx_fifo);
	ccat_eth_dma_fifo_reset(&priv->tx_fifo);

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
	static const size_t overhead = CCAT_ETH_FRAME_HEAD_LEN - 4;
	struct ccat_eth_dma_fifo *const fifo = &priv->rx_fifo;

	/* TODO omit possible deadlock in situations with heavy traffic */
	while (ccat_eth_frame_received(fifo->next)) {
		const size_t len = le16_to_cpu(fifo->next->length) - overhead;
		if (priv->ecdev) {
			ecdev_receive(priv->ecdev, fifo->next->data, len);
		} else {
			ccat_eth_receive(priv->netdev, fifo->next->data, len);
		}
		ccat_eth_rx_fifo_add(fifo, fifo->next);
		ccat_eth_fifo_inc(fifo);
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
	if (ccat_eth_frame_sent(priv->tx_fifo.next)) {
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
	if(!priv->ecdev) {
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

struct ccat_eth_priv *ccat_eth_init(const struct ccat_device *const ccatdev,
				    const void __iomem * const addr)
{
	struct ccat_eth_priv *priv;
	struct net_device *const netdev = alloc_etherdev(sizeof(*priv));

	priv = netdev_priv(netdev);
	priv->netdev = netdev;
	priv->ccatdev = ccatdev;

	/* ccat register mappings */
	memcpy_fromio(&priv->info, addr, sizeof(priv->info));
	ccat_eth_priv_init_mappings(priv);

	if (ccat_eth_priv_init_dma(priv)) {
		pr_warn("%s(): DMA initialization failed.\n", __FUNCTION__);
		free_netdev(netdev);
		return NULL;
	}

	/* init netdev with MAC and stack callbacks */
	memcpy_fromio(netdev->dev_addr, priv->reg.mii + 8, netdev->addr_len);
	netdev->netdev_ops = &ccat_eth_netdev_ops;

	/* use as EtherCAT device? */
	priv->ecdev = ecdev_offer(netdev, ec_poll_rx, THIS_MODULE);
	if (priv->ecdev) {
		priv->carrier_off = ecdev_carrier_off;
		priv->carrier_ok = ecdev_carrier_ok;
		priv->carrier_on = ecdev_carrier_on;
		priv->kfree_skb_any = ecdev_kfree_skb_any;
		priv->start_queue = ecdev_nop;
		priv->stop_queue = ecdev_nop;
		priv->unregister = unregister_ecdev;

		priv->carrier_off(netdev);
		if (ecdev_open(priv->ecdev)) {
			pr_info("unable to register network device.\n");
			ecdev_withdraw(priv->ecdev);
			ccat_eth_priv_free_dma(priv);
			free_netdev(netdev);
			return NULL;
		}
		return priv;
	}

	/* EtherCAT disabled -> prepare normal ethernet mode */
	priv->carrier_off = netif_carrier_off;
	priv->carrier_ok = netif_carrier_ok;
	priv->carrier_on = netif_carrier_on;
	priv->kfree_skb_any = dev_kfree_skb_any;
	priv->start_queue = netif_start_queue;
	priv->stop_queue = netif_stop_queue;
	priv->unregister = unregister_netdev;

	priv->carrier_off(netdev);
	if (register_netdev(netdev)) {
		pr_info("unable to register network device.\n");
		ccat_eth_priv_free_dma(priv);
		free_netdev(netdev);
		return NULL;
	}
	pr_info("registered %s as network device.\n", netdev->name);
	return priv;
}

void ccat_eth_remove(struct ccat_eth_priv *const priv)
{
	priv->unregister(priv->netdev);
	ccat_eth_priv_free_dma(priv);
	free_netdev(priv->netdev);
}
