/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; version 2 of the License.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be
 *  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/

/**
   \file
   Ethernet-over-EtherCAT (EoE).
*/

/*****************************************************************************/

#include <linux/etherdevice.h>

#include "../include/ecrt.h"
#include "globals.h"
#include "master.h"
#include "slave.h"
#include "mailbox.h"
#include "ethernet.h"

#define EOE_DEBUG_LEVEL 1

/*****************************************************************************/

/**
   Queued frame structure.
*/

typedef struct
{
    struct list_head queue; /**< list item */
    struct sk_buff *skb; /**< socket buffer */
}
ec_eoe_frame_t;

/*****************************************************************************/

void ec_eoe_flush(ec_eoe_t *);
void ec_eoedev_init(struct net_device *);
int ec_eoedev_open(struct net_device *);
int ec_eoedev_stop(struct net_device *);
int ec_eoedev_tx(struct sk_buff *, struct net_device *);
struct net_device_stats *ec_eoedev_stats(struct net_device *);
void ec_eoedev_rx(struct net_device *, const uint8_t *, size_t);

/*****************************************************************************/

/**
   EoE constructor.
*/

int ec_eoe_init(ec_eoe_t *eoe, ec_slave_t *slave)
{
    ec_eoe_t **priv;
    int result;

    eoe->slave = slave;
    eoe->state = EC_EOE_RX_START;
    eoe->opened = 0;
    eoe->skb = NULL;
    eoe->expected_fragment = 0;
    INIT_LIST_HEAD(&eoe->tx_queue);
    eoe->tx_queue_active = 0;
    eoe->queued_frames = 0;
    eoe->tx_queue_lock = SPIN_LOCK_UNLOCKED;
    eoe->tx_frame_number = 0xFF;
    memset(&eoe->stats, 0, sizeof(struct net_device_stats));

    if (!(eoe->dev =
          alloc_netdev(sizeof(ec_eoe_t *), "eoe%d", ec_eoedev_init))) {
        EC_ERR("Unable to allocate net_device for EoE object!\n");
        goto out_return;
    }

    // set EoE object reference
    priv = netdev_priv(eoe->dev);
    *priv = eoe;

    eoe->dev->mtu = slave->sii_rx_mailbox_size - ETH_HLEN - 10;

    // connect the net_device to the kernel
    if ((result = register_netdev(eoe->dev))) {
        EC_ERR("Unable to register net_device: error %i\n", result);
        goto out_free;
    }

    return 0;

 out_free:
    free_netdev(eoe->dev);
    eoe->dev = NULL;
 out_return:
    return -1;
}

/*****************************************************************************/

/**
   EoE destructor.
*/

void ec_eoe_clear(ec_eoe_t *eoe)
{
    if (eoe->dev) {
        unregister_netdev(eoe->dev);
        free_netdev(eoe->dev);
    }

    // empty transmit queue
    ec_eoe_flush(eoe);
}

/*****************************************************************************/

/**
   Empties the transmit queue.
*/

void ec_eoe_flush(ec_eoe_t *eoe)
{
    ec_eoe_frame_t *frame, *next;

    spin_lock_bh(&eoe->tx_queue_lock);

    list_for_each_entry_safe(frame, next, &eoe->tx_queue, queue) {
        list_del(&frame->queue);
        dev_kfree_skb(frame->skb);
        kfree(frame);
    }
    eoe->queued_frames = 0;

    spin_unlock_bh(&eoe->tx_queue_lock);
}

/*****************************************************************************/

/**
   Runs the EoE state machine.
*/

void ec_eoe_run(ec_eoe_t *eoe)
{
    uint8_t *data;
    ec_master_t *master;
    size_t rec_size, data_size;
    off_t offset;
    uint8_t fragment_number, frame_number, last_fragment, time_appended;
    uint8_t fragment_offset, frame_type;
    ec_eoe_frame_t *frame;
    unsigned int wakeup = 0;
#if EOE_DEBUG_LEVEL > 1
    unsigned int i;
#endif

    if (!eoe->opened) return;

    master = eoe->slave->master;

    switch (eoe->state) {
        case EC_EOE_RX_START:
            ec_slave_mbox_prepare_check(eoe->slave);
            ec_master_queue_command(master, &eoe->slave->mbox_command);
            eoe->state = EC_EOE_RX_CHECK;
            break;

        case EC_EOE_RX_CHECK:
            if (eoe->slave->mbox_command.state != EC_CMD_RECEIVED) {
                eoe->stats.rx_errors++;
                eoe->state = EC_EOE_TX_START;
                break;
            }
            if (!ec_slave_mbox_check(eoe->slave)) {
                eoe->state = EC_EOE_TX_START;
                break;
            }
            ec_slave_mbox_prepare_fetch(eoe->slave);
            ec_master_queue_command(master, &eoe->slave->mbox_command);
            eoe->state = EC_EOE_RX_FETCH;
            break;

        case EC_EOE_RX_FETCH:
            if (eoe->slave->mbox_command.state != EC_CMD_RECEIVED) {
                eoe->stats.rx_errors++;
                eoe->state = EC_EOE_TX_START;
                break;
            }
            if (!(data = ec_slave_mbox_fetch(eoe->slave, 0x02, &rec_size))) {
                eoe->stats.rx_errors++;
                eoe->state = EC_EOE_TX_START;
                break;
            }

            frame_type = EC_READ_U16(data) & 0x000F;

            if (frame_type == 0x00) { // EoE Fragment Request
                last_fragment = (EC_READ_U16(data) >> 8) & 0x0001;
                time_appended = (EC_READ_U16(data) >> 9) & 0x0001;
                fragment_number = EC_READ_U16(data + 2) & 0x003F;
                fragment_offset = (EC_READ_U16(data + 2) >> 6) & 0x003F;
                frame_number = (EC_READ_U16(data + 2) >> 12) & 0x000F;

#if EOE_DEBUG_LEVEL > 0
                EC_DBG("EoE RX fragment %i, offset %i, frame %i%s%s,"
                       " %i octets\n", fragment_number, fragment_offset,
                       frame_number,
                       last_fragment ? ", last fragment" : "",
                       time_appended ? ", + timestamp" : "",
                       time_appended ? rec_size - 8 : rec_size - 4);

#if EOE_DEBUG_LEVEL > 1
                EC_DBG("");
                for (i = 0; i < rec_size - 4; i++) {
                    printk("%02X ", data[i + 4]);
                    if ((i + 1) % 16 == 0) {
                        printk("\n");
                        EC_DBG("");
                    }
                }
                printk("\n");
#endif
#endif

                data_size = time_appended ? rec_size - 8 : rec_size - 4;

                if (!fragment_number) {
                    if (eoe->skb) {
                        EC_WARN("EoE RX freeing old socket buffer...\n");
                        dev_kfree_skb(eoe->skb);
                    }

                    // new socket buffer
                    if (!(eoe->skb = dev_alloc_skb(fragment_offset * 32))) {
                        if (printk_ratelimit())
                            EC_WARN("EoE RX low on mem. frame dropped.\n");
                        eoe->stats.rx_dropped++;
                        eoe->state = EC_EOE_TX_START;
                        break;
                    }
                    eoe->skb_offset = 0;
                    eoe->skb_size = fragment_offset * 32;
                    eoe->expected_fragment = 0;
                }
                else {
                    if (!eoe->skb) {
                        eoe->stats.rx_dropped++;
                        eoe->state = EC_EOE_TX_START;
                        break;
                    }

                    offset = fragment_offset * 32;
                    if (offset != eoe->skb_offset ||
                        offset + data_size > eoe->skb_size ||
                        fragment_number != eoe->expected_fragment) {
                        eoe->stats.rx_errors++;
                        eoe->state = EC_EOE_TX_START;
                        dev_kfree_skb(eoe->skb);
                        eoe->skb = NULL;
                        break;
                    }
                }

                // copy fragment into socket buffer
                memcpy(skb_put(eoe->skb, data_size), data + 4, data_size);
                eoe->skb_offset += data_size;

                if (last_fragment) {
                    // update statistics
                    eoe->stats.rx_packets++;
                    eoe->stats.rx_bytes += eoe->skb->len;

#if EOE_DEBUG_LEVEL > 0
                    EC_DBG("EoE RX frame completed with %u octets.\n",
                           eoe->skb->len);
#endif

                    // pass socket buffer to network stack
                    eoe->skb->dev = eoe->dev;
                    eoe->skb->protocol = eth_type_trans(eoe->skb, eoe->dev);
                    eoe->skb->ip_summed = CHECKSUM_UNNECESSARY;
                    eoe->skb->pkt_type = PACKET_HOST;
                    if (netif_rx(eoe->skb)) {
                        EC_WARN("EoE RX netif_rx failed.\n");
                    }
                    eoe->skb = NULL;

                    eoe->state = EC_EOE_TX_START;
                }
                else {
                    eoe->expected_fragment++;
#if EOE_DEBUG_LEVEL > 0
                    EC_DBG("EoE RX expecting fragment %i\n",
                           eoe->expected_fragment);
#endif
                    eoe->state = EC_EOE_RX_START;
                }
            }
            else {
#if EOE_DEBUG_LEVEL > 0
                EC_DBG("other frame received.\n");
#endif
                eoe->stats.rx_dropped++;
                eoe->state = EC_EOE_TX_START;
            }
            break;

        case EC_EOE_TX_START:
            spin_lock_bh(&eoe->tx_queue_lock);

            if (!eoe->queued_frames || list_empty(&eoe->tx_queue)) {
                spin_unlock_bh(&eoe->tx_queue_lock);
                eoe->state = EC_EOE_RX_START;
                break;
            }

            // take the first frame out of the queue
            frame = list_entry(eoe->tx_queue.next, ec_eoe_frame_t, queue);
            list_del(&frame->queue);
            if (!eoe->tx_queue_active &&
                eoe->queued_frames == EC_EOE_TX_QUEUE_SIZE / 2) {
                netif_wake_queue(eoe->dev);
                eoe->tx_queue_active = 1;
                wakeup = 1;
            }
            eoe->queued_frames--;
            spin_unlock_bh(&eoe->tx_queue_lock);

#if EOE_DEBUG_LEVEL > 0
            EC_DBG("EoE TX Sending frame with %i octets."
                   " (%i frames queued).\n",
                   frame->skb->len, eoe->queued_frames);

#if EOE_DEBUG_LEVEL > 1
            EC_DBG("");
            for (i = 0; i < frame->skb->len; i++) {
                printk("%02X ", frame->skb->data[i]);
                if ((i + 1) % 16 == 0) {
                    printk("\n");
                    EC_DBG("");
                }
            }
            printk("\n");
#endif

            if (wakeup) EC_DBG("waking up TX queue...\n");
#endif

            if (!(data = ec_slave_mbox_prepare_send(eoe->slave, 0x02,
                                                    frame->skb->len + 4))) {
                dev_kfree_skb(frame->skb);
                kfree(frame);
                eoe->stats.tx_errors++;
                eoe->state = EC_EOE_RX_START;
                break;
            }

            eoe->tx_frame_number++;
            eoe->tx_frame_number %= 16;

            EC_WRITE_U16(data, 0x0100); // eoe fragment req.
            EC_WRITE_U16(data + 2, (eoe->tx_frame_number & 0x0F) << 12);

            memcpy(data + 4, frame->skb->data, frame->skb->len);
            ec_master_queue_command(master, &eoe->slave->mbox_command);

            eoe->last_tx_bytes = frame->skb->len;
            dev_kfree_skb(frame->skb);
            kfree(frame);

            eoe->state = EC_EOE_TX_SENT;
            break;

        case EC_EOE_TX_SENT:
            if (eoe->slave->mbox_command.state != EC_CMD_RECEIVED) {
                eoe->stats.tx_errors++;
                eoe->state = EC_EOE_RX_START;
                break;
            }
            if (eoe->slave->mbox_command.working_counter != 1) {
                eoe->stats.tx_errors++;
                eoe->state = EC_EOE_RX_START;
                break;
            }

            eoe->stats.tx_packets++;
            eoe->stats.tx_bytes += eoe->last_tx_bytes;
            eoe->state = EC_EOE_RX_START;
            break;

        default:
            break;
    }
}

/*****************************************************************************/

/**
   Prints EoE object information.
*/

void ec_eoe_print(const ec_eoe_t *eoe)
{
    EC_INFO("  EoE slave %i\n", eoe->slave->ring_position);
    EC_INFO("    State %i\n", eoe->state);
    EC_INFO("    Assigned device: %s (%s)\n", eoe->dev->name,
            eoe->opened ? "opened" : "closed");
}

/*****************************************************************************/

/**
   Initializes a net_device structure for an EoE object.
*/

void ec_eoedev_init(struct net_device *dev /**< pointer to the net_device */)
{
    ec_eoe_t *priv;
    unsigned int i;

    // initialize net_device
    ether_setup(dev);
    dev->open = ec_eoedev_open;
    dev->stop = ec_eoedev_stop;
    dev->hard_start_xmit = ec_eoedev_tx;
    dev->get_stats = ec_eoedev_stats;

    for (i = 0; i < 6; i++) dev->dev_addr[i] = (i + 1) | (i + 1) << 4;

    // initialize private data
    priv = netdev_priv(dev);
    memset(priv, 0, sizeof(ec_eoe_t *));
}

/*****************************************************************************/

/**
   Opens the virtual network device.
*/

int ec_eoedev_open(struct net_device *dev /**< EoE net_device */)
{
    ec_eoe_t *eoe = *((ec_eoe_t **) netdev_priv(dev));
    ec_eoe_flush(eoe);
    eoe->opened = 1;
    netif_start_queue(dev);
    eoe->tx_queue_active = 1;
    EC_INFO("%s (slave %i) opened.\n", dev->name, eoe->slave->ring_position);
    return 0;
}

/*****************************************************************************/

/**
   Stops the virtual network device.
*/

int ec_eoedev_stop(struct net_device *dev /**< EoE net_device */)
{
    ec_eoe_t *eoe = *((ec_eoe_t **) netdev_priv(dev));
    netif_stop_queue(dev);
    eoe->tx_queue_active = 0;
    eoe->opened = 0;
    ec_eoe_flush(eoe);
    EC_INFO("%s (slave %i) stopped.\n", dev->name, eoe->slave->ring_position);
    return 0;
}

/*****************************************************************************/

/**
   Transmits data via the virtual network device.
*/

int ec_eoedev_tx(struct sk_buff *skb, /**< transmit socket buffer */
                 struct net_device *dev /**< EoE net_device */
                )
{
    ec_eoe_t *eoe = *((ec_eoe_t **) netdev_priv(dev));
    ec_eoe_frame_t *frame;

    if (skb->len + 10 > eoe->slave->sii_tx_mailbox_size) {
        EC_WARN("EoE TX frame (%i octets) exceeds MTU. dropping.\n", skb->len);
        dev_kfree_skb(skb);
        eoe->stats.tx_dropped++;
        return 0;
    }

    if (!(frame =
          (ec_eoe_frame_t *) kmalloc(sizeof(ec_eoe_frame_t), GFP_ATOMIC))) {
        if (printk_ratelimit())
            EC_WARN("EoE TX: low on mem. frame dropped.\n");
        return 1;
    }

    frame->skb = skb;

    spin_lock_bh(&eoe->tx_queue_lock);
    list_add_tail(&frame->queue, &eoe->tx_queue);
    eoe->queued_frames++;
    if (eoe->queued_frames == EC_EOE_TX_QUEUE_SIZE) {
        netif_stop_queue(dev);
        eoe->tx_queue_active = 0;
    }
    spin_unlock_bh(&eoe->tx_queue_lock);

#if EOE_DEBUG_LEVEL > 0
    EC_DBG("EoE TX queued frame with %i octets (%i frames queued).\n",
           skb->len, eoe->queued_frames);
    if (!eoe->tx_queue_active)
        EC_WARN("EoE TX queue is now full.\n");
#endif

    return 0;
}

/*****************************************************************************/

/**
   Gets statistics about the virtual network device.
*/

struct net_device_stats *ec_eoedev_stats(struct net_device *dev
                                         /**< EoE net_device */)
{
    ec_eoe_t *eoe = *((ec_eoe_t **) netdev_priv(dev));
    return &eoe->stats;
}

/*****************************************************************************/
