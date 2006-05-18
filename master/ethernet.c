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
 *  as published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
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
 *  The right to use EtherCAT Technology is granted and comes free of
 *  charge under condition of compatibility of product made by
 *  Licensee. People intending to distribute/sell products based on the
 *  code, have to sign an agreement to guarantee that products using
 *  software based on IgH EtherCAT master stay compatible with the actual
 *  EtherCAT specification (which are released themselves as an open
 *  standard) as the (only) precondition to have the right to use EtherCAT
 *  Technology, IP and trade marks.
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

#define EOE_DEBUG_LEVEL 0

/*****************************************************************************/

void ec_eoe_flush(ec_eoe_t *);

// state functions
void ec_eoe_state_rx_start(ec_eoe_t *);
void ec_eoe_state_rx_check(ec_eoe_t *);
void ec_eoe_state_rx_fetch(ec_eoe_t *);
void ec_eoe_state_tx_start(ec_eoe_t *);
void ec_eoe_state_tx_sent(ec_eoe_t *);

// net_device functions
int ec_eoedev_open(struct net_device *);
int ec_eoedev_stop(struct net_device *);
int ec_eoedev_tx(struct sk_buff *, struct net_device *);
struct net_device_stats *ec_eoedev_stats(struct net_device *);

/*****************************************************************************/

/**
   EoE constructor.
   Initializes the EoE object, creates a net_device and registeres it.
*/

int ec_eoe_init(ec_eoe_t *eoe, /**< EoE object */
                ec_slave_t *slave /**< assigned slave */
                )
{
    ec_eoe_t **priv;
    int result, i;

    eoe->slave = slave;
    eoe->state = ec_eoe_state_rx_start;
    eoe->opened = 0;
    eoe->rx_skb = NULL;
    eoe->rx_expected_fragment = 0;
    INIT_LIST_HEAD(&eoe->tx_queue);
    eoe->tx_frame = NULL;
    eoe->tx_queue_active = 0;
    eoe->tx_queued_frames = 0;
    eoe->tx_queue_lock = SPIN_LOCK_UNLOCKED;
    eoe->tx_frame_number = 0xFF;
    memset(&eoe->stats, 0, sizeof(struct net_device_stats));

    if (!(eoe->dev =
          alloc_netdev(sizeof(ec_eoe_t *), "eoe%d", ether_setup))) {
        EC_ERR("Unable to allocate net_device for EoE object!\n");
        goto out_return;
    }

    // initialize net_device
    eoe->dev->open = ec_eoedev_open;
    eoe->dev->stop = ec_eoedev_stop;
    eoe->dev->hard_start_xmit = ec_eoedev_tx;
    eoe->dev->get_stats = ec_eoedev_stats;

    for (i = 0; i < ETH_ALEN; i++)
        eoe->dev->dev_addr[i] = i | (i << 4);

    // initialize private data
    priv = netdev_priv(eoe->dev);
    *priv = eoe;

    // Usually setting the MTU appropriately makes the upper layers
    // do the frame fragmenting. In some cases this doesn't work
    // so the MTU is left on the Ethernet standard value and fragmenting
    // is done "manually".
#if 0
    eoe->dev->mtu = slave->sii_rx_mailbox_size - ETH_HLEN - 10;
#endif

    // connect the net_device to the kernel
    if ((result = register_netdev(eoe->dev))) {
        EC_ERR("Unable to register net_device: error %i\n", result);
        goto out_free;
    }

    // make the last address octet unique
    eoe->dev->dev_addr[ETH_ALEN - 1] = (uint8_t) eoe->dev->ifindex;

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
   Unregisteres the net_device and frees allocated memory.
*/

void ec_eoe_clear(ec_eoe_t *eoe /**< EoE object */)
{
    if (eoe->dev) {
        unregister_netdev(eoe->dev);
        free_netdev(eoe->dev);
    }

    // empty transmit queue
    ec_eoe_flush(eoe);

    if (eoe->tx_frame) {
        dev_kfree_skb(eoe->tx_frame->skb);
        kfree(eoe->tx_frame);
    }

    if (eoe->rx_skb) dev_kfree_skb(eoe->rx_skb);
}

/*****************************************************************************/

/**
   Empties the transmit queue.
*/

void ec_eoe_flush(ec_eoe_t *eoe /**< EoE object */)
{
    ec_eoe_frame_t *frame, *next;

    spin_lock_bh(&eoe->tx_queue_lock);

    list_for_each_entry_safe(frame, next, &eoe->tx_queue, queue) {
        list_del(&frame->queue);
        dev_kfree_skb(frame->skb);
        kfree(frame);
    }
    eoe->tx_queued_frames = 0;

    spin_unlock_bh(&eoe->tx_queue_lock);
}

/*****************************************************************************/

/**
   Sends a frame or the next fragment.
*/

int ec_eoe_send(ec_eoe_t *eoe /**< EoE object */)
{
    size_t remaining_size, current_size, complete_offset;
    unsigned int last_fragment;
    uint8_t *data;
#if EOE_DEBUG_LEVEL > 1
    unsigned int i;
#endif

    remaining_size = eoe->tx_frame->skb->len - eoe->tx_offset;

    if (remaining_size <= eoe->slave->sii_tx_mailbox_size - 10) {
        current_size = remaining_size;
        last_fragment = 1;
    }
    else {
        current_size = ((eoe->slave->sii_tx_mailbox_size - 10) / 32) * 32;
        last_fragment = 0;
    }

    if (eoe->tx_fragment_number) {
        complete_offset = eoe->tx_offset / 32;
    }
    else {
        complete_offset = remaining_size / 32 + 1;
    }

#if EOE_DEBUG_LEVEL > 0
    EC_DBG("EoE TX sending %sfragment %i with %i octets (%i)."
           " %i frames queued.\n", last_fragment ? "last " : "",
           eoe->tx_fragment_number, current_size, complete_offset,
           eoe->tx_queued_frames);
#endif

#if EOE_DEBUG_LEVEL > 1
    EC_DBG("");
    for (i = 0; i < current_size; i++) {
        printk("%02X ", frame->skb->data[eoe->tx_offset + i]);
        if ((i + 1) % 16 == 0) {
            printk("\n");
            EC_DBG("");
        }
    }
    printk("\n");
#endif

    if (!(data = ec_slave_mbox_prepare_send(eoe->slave, 0x02,
                                            current_size + 4)))
        return -1;

    EC_WRITE_U8 (data,     0x00); // eoe fragment req.
    EC_WRITE_U8 (data + 1, last_fragment);
    EC_WRITE_U16(data + 2, ((eoe->tx_fragment_number & 0x3F) |
                            (complete_offset & 0x3F) << 6 |
                            (eoe->tx_frame_number & 0x0F) << 12));

    memcpy(data + 4, eoe->tx_frame->skb->data + eoe->tx_offset, current_size);
    ec_master_queue_command(eoe->slave->master, &eoe->slave->mbox_command);

    eoe->tx_offset += current_size;
    eoe->tx_fragment_number++;

    return 0;
}

/*****************************************************************************/

/**
   Runs the EoE state machine.
*/

void ec_eoe_run(ec_eoe_t *eoe /**< EoE object */)
{
    if (!eoe->opened) return;

    // call state function
    eoe->state(eoe);
}

/*****************************************************************************/

/**
   Returns the state of the device.
   \return 1 if the device is "up", 0 if it is "down"
*/

unsigned int ec_eoe_active(const ec_eoe_t *eoe /**< EoE object */)
{
    return eoe->opened;
}

/*****************************************************************************/

/**
   Prints EoE object information.
*/

void ec_eoe_print(const ec_eoe_t *eoe /**< EoE object */)
{
    EC_INFO("  EoE slave %i\n", eoe->slave->ring_position);
    EC_INFO("    Assigned device: %s (%s)\n", eoe->dev->name,
            eoe->opened ? "opened" : "closed");
}

/******************************************************************************
 *  STATE PROCESSING FUNCTIONS
 *****************************************************************************/

/**
   State: RX_START.
   Starts a new receiving sequence by queuing a command that checks the
   slave's mailbox for a new command.
*/

void ec_eoe_state_rx_start(ec_eoe_t *eoe /**< EoE object */)
{
    ec_slave_mbox_prepare_check(eoe->slave);
    ec_master_queue_command(eoe->slave->master, &eoe->slave->mbox_command);
    eoe->state = ec_eoe_state_rx_check;
}

/*****************************************************************************/

/**
   State: RX_CHECK.
   Processes the checking command sent in RX_START and issues a receive
   command, if new data is available.
*/

void ec_eoe_state_rx_check(ec_eoe_t *eoe /**< EoE object */)
{
    if (eoe->slave->mbox_command.state != EC_CMD_RECEIVED) {
        eoe->stats.rx_errors++;
        eoe->state = ec_eoe_state_tx_start;
        return;
    }

    if (!ec_slave_mbox_check(eoe->slave)) {
        eoe->state = ec_eoe_state_tx_start;
        return;
    }

    ec_slave_mbox_prepare_fetch(eoe->slave);
    ec_master_queue_command(eoe->slave->master, &eoe->slave->mbox_command);
    eoe->state = ec_eoe_state_rx_fetch;
}

/*****************************************************************************/

/**
   State: RX_FETCH.
   Checks if the requested data of RX_CHECK was received and processes the
   EoE command.
*/

void ec_eoe_state_rx_fetch(ec_eoe_t *eoe /**< EoE object */)
{
    size_t rec_size, data_size;
    uint8_t *data, frame_type, last_fragment, time_appended;
    uint8_t frame_number, fragment_offset, fragment_number;
    off_t offset;

    if (eoe->slave->mbox_command.state != EC_CMD_RECEIVED) {
        eoe->stats.rx_errors++;
        eoe->state = ec_eoe_state_tx_start;
        return;
    }

    if (!(data = ec_slave_mbox_fetch(eoe->slave, 0x02, &rec_size))) {
        eoe->stats.rx_errors++;
        eoe->state = ec_eoe_state_tx_start;
        return;
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
#endif

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

        data_size = time_appended ? rec_size - 8 : rec_size - 4;

        if (!fragment_number) {
            if (eoe->rx_skb) {
                EC_WARN("EoE RX freeing old socket buffer...\n");
                dev_kfree_skb(eoe->rx_skb);
            }

            // new socket buffer
            if (!(eoe->rx_skb = dev_alloc_skb(fragment_offset * 32))) {
                if (printk_ratelimit())
                    EC_WARN("EoE RX low on mem. frame dropped.\n");
                eoe->stats.rx_dropped++;
                eoe->state = ec_eoe_state_tx_start;
                return;
            }

            eoe->rx_skb_offset = 0;
            eoe->rx_skb_size = fragment_offset * 32;
            eoe->rx_expected_fragment = 0;
        }
        else {
            if (!eoe->rx_skb) {
                eoe->stats.rx_dropped++;
                eoe->state = ec_eoe_state_tx_start;
                return;
            }

            offset = fragment_offset * 32;
            if (offset != eoe->rx_skb_offset ||
                offset + data_size > eoe->rx_skb_size ||
                fragment_number != eoe->rx_expected_fragment) {
                eoe->stats.rx_errors++;
                eoe->state = ec_eoe_state_tx_start;
                dev_kfree_skb(eoe->rx_skb);
                eoe->rx_skb = NULL;
                return;
            }
        }

        // copy fragment into socket buffer
        memcpy(skb_put(eoe->rx_skb, data_size), data + 4, data_size);
        eoe->rx_skb_offset += data_size;

        if (last_fragment) {
            // update statistics
            eoe->stats.rx_packets++;
            eoe->stats.rx_bytes += eoe->rx_skb->len;

#if EOE_DEBUG_LEVEL > 0
            EC_DBG("EoE RX frame completed with %u octets.\n",
                   eoe->rx_skb->len);
#endif

            // pass socket buffer to network stack
            eoe->rx_skb->dev = eoe->dev;
            eoe->rx_skb->protocol = eth_type_trans(eoe->rx_skb, eoe->dev);
            eoe->rx_skb->ip_summed = CHECKSUM_UNNECESSARY;
            if (netif_rx(eoe->rx_skb)) {
                EC_WARN("EoE RX netif_rx failed.\n");
            }
            eoe->rx_skb = NULL;

            eoe->state = ec_eoe_state_tx_start;
        }
        else {
            eoe->rx_expected_fragment++;
#if EOE_DEBUG_LEVEL > 0
            EC_DBG("EoE RX expecting fragment %i\n",
                   eoe->rx_expected_fragment);
#endif
            eoe->state = ec_eoe_state_rx_start;
        }
    }
    else {
#if EOE_DEBUG_LEVEL > 0
        EC_DBG("other frame received.\n");
#endif
        eoe->stats.rx_dropped++;
        eoe->state = ec_eoe_state_tx_start;
    }
}

/*****************************************************************************/

/**
   State: TX START.
   Starts a new transmit sequence. If no data is available, a new receive
   sequence is started instead.
*/

void ec_eoe_state_tx_start(ec_eoe_t *eoe /**< EoE object */)
{
#if EOE_DEBUG_LEVEL > 0
    unsigned int wakeup;
#endif

    spin_lock_bh(&eoe->tx_queue_lock);

    if (!eoe->tx_queued_frames || list_empty(&eoe->tx_queue)) {
        spin_unlock_bh(&eoe->tx_queue_lock);
        // no data available.
        // start a new receive immediately.
        ec_eoe_state_rx_start(eoe);
        return;
    }

    // take the first frame out of the queue
    eoe->tx_frame = list_entry(eoe->tx_queue.next, ec_eoe_frame_t, queue);
    list_del(&eoe->tx_frame->queue);
    if (!eoe->tx_queue_active &&
        eoe->tx_queued_frames == EC_EOE_TX_QUEUE_SIZE / 2) {
        netif_wake_queue(eoe->dev);
        eoe->tx_queue_active = 1;
#if EOE_DEBUG_LEVEL > 0
        wakeup = 1;
#endif
    }

    eoe->tx_queued_frames--;
    spin_unlock_bh(&eoe->tx_queue_lock);

    eoe->tx_frame_number++;
    eoe->tx_frame_number %= 16;
    eoe->tx_fragment_number = 0;
    eoe->tx_offset = 0;

    if (ec_eoe_send(eoe)) {
        dev_kfree_skb(eoe->tx_frame->skb);
        kfree(eoe->tx_frame);
        eoe->tx_frame = NULL;
        eoe->stats.tx_errors++;
        eoe->state = ec_eoe_state_rx_start;
        return;
    }

#if EOE_DEBUG_LEVEL > 0
    if (wakeup) EC_DBG("waking up TX queue...\n");
#endif

    eoe->state = ec_eoe_state_tx_sent;
}

/*****************************************************************************/

/**
   State: TX SENT.
   Checks is the previous transmit command succeded and sends the next
   fragment, if necessary.
*/

void ec_eoe_state_tx_sent(ec_eoe_t *eoe /**< EoE object */)
{
    if (eoe->slave->mbox_command.state != EC_CMD_RECEIVED) {
        eoe->stats.tx_errors++;
        eoe->state = ec_eoe_state_rx_start;
        return;
    }

    if (eoe->slave->mbox_command.working_counter != 1) {
        eoe->stats.tx_errors++;
        eoe->state = ec_eoe_state_rx_start;
        return;
    }

    // frame completely sent
    if (eoe->tx_offset >= eoe->tx_frame->skb->len) {
        eoe->stats.tx_packets++;
        eoe->stats.tx_bytes += eoe->tx_frame->skb->len;
        dev_kfree_skb(eoe->tx_frame->skb);
        kfree(eoe->tx_frame);
        eoe->tx_frame = NULL;
        eoe->state = ec_eoe_state_rx_start;
    }
    else { // send next fragment
        if (ec_eoe_send(eoe)) {
            dev_kfree_skb(eoe->tx_frame->skb);
            kfree(eoe->tx_frame);
            eoe->tx_frame = NULL;
            eoe->stats.tx_errors++;
            eoe->state = ec_eoe_state_rx_start;
        }
    }
}

/******************************************************************************
 *  NET_DEVICE functions
 *****************************************************************************/

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

#if 0
    if (skb->len > eoe->slave->sii_tx_mailbox_size - 10) {
        EC_WARN("EoE TX frame (%i octets) exceeds MTU. dropping.\n", skb->len);
        dev_kfree_skb(skb);
        eoe->stats.tx_dropped++;
        return 0;
    }
#endif

    if (!(frame =
          (ec_eoe_frame_t *) kmalloc(sizeof(ec_eoe_frame_t), GFP_ATOMIC))) {
        if (printk_ratelimit())
            EC_WARN("EoE TX: low on mem. frame dropped.\n");
        return 1;
    }

    frame->skb = skb;

    spin_lock_bh(&eoe->tx_queue_lock);
    list_add_tail(&frame->queue, &eoe->tx_queue);
    eoe->tx_queued_frames++;
    if (eoe->tx_queued_frames == EC_EOE_TX_QUEUE_SIZE) {
        netif_stop_queue(dev);
        eoe->tx_queue_active = 0;
    }
    spin_unlock_bh(&eoe->tx_queue_lock);

#if EOE_DEBUG_LEVEL > 0
    EC_DBG("EoE TX queued frame with %i octets (%i frames queued).\n",
           skb->len, eoe->tx_queued_frames);
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
