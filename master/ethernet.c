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

/*****************************************************************************/

/**
   Contains the private data of an EoE net_device.
*/

typedef struct
{
    struct net_device_stats stats; /**< device statistics */
    ec_eoe_t *eoe; /**< pointer to parent eoe object */
}
ec_eoedev_priv_t;

/*****************************************************************************/

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
    ec_eoedev_priv_t *priv;
    int result;

    eoe->slave = slave;
    eoe->rx_state = EC_EOE_IDLE;
    eoe->opened = 0;

    if (!(eoe->dev =
          alloc_netdev(sizeof(ec_eoedev_priv_t), "eoe%d", ec_eoedev_init))) {
        EC_ERR("Unable to allocate net_device for EoE object!\n");
        goto out_return;
    }

    // set EoE object reference
    priv = netdev_priv(eoe->dev);
    priv->eoe = eoe;

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
}

/*****************************************************************************/

/**
   Runs the EoE state machine.
*/

void ec_eoe_run(ec_eoe_t *eoe)
{
    uint8_t *data;
    ec_master_t *master;
    size_t rec_size;
    uint8_t fragment_number, frame_number, last_fragment, time_appended;
    uint8_t fragment_offset, frame_type;

    if (!eoe->opened) return;

    master = eoe->slave->master;

    if (eoe->rx_state == EC_EOE_IDLE) {
        ec_slave_mbox_prepare_check(eoe->slave);
        ec_master_queue_command(master, &eoe->slave->mbox_command);
        eoe->rx_state = EC_EOE_CHECKING;
        return;
    }

    if (eoe->rx_state == EC_EOE_CHECKING) {
        if (eoe->slave->mbox_command.state != EC_CMD_RECEIVED) {
            master->stats.eoe_errors++;
            eoe->rx_state = EC_EOE_IDLE;
            return;
        }
        if (!ec_slave_mbox_check(eoe->slave)) {
            eoe->rx_state = EC_EOE_IDLE;
            return;
        }
        ec_slave_mbox_prepare_fetch(eoe->slave);
        ec_master_queue_command(master, &eoe->slave->mbox_command);
        eoe->rx_state = EC_EOE_FETCHING;
        return;
    }

    if (eoe->rx_state == EC_EOE_FETCHING) {
        if (eoe->slave->mbox_command.state != EC_CMD_RECEIVED) {
            master->stats.eoe_errors++;
            eoe->rx_state = EC_EOE_IDLE;
            return;
        }
        if (!(data = ec_slave_mbox_fetch(eoe->slave, 0x02, &rec_size))) {
            master->stats.eoe_errors++;
            eoe->rx_state = EC_EOE_IDLE;
            return;
        }

        frame_type = EC_READ_U16(data) & 0x000F;

        if (frame_type == 0x00) { // EoE Fragment Request
            last_fragment = (EC_READ_U16(data) >> 8) & 0x0001;
            time_appended = (EC_READ_U16(data) >> 9) & 0x0001;
            fragment_number = EC_READ_U16(data + 2) & 0x003F;
            fragment_offset = (EC_READ_U16(data + 2) >> 6) & 0x003F;
            frame_number = (EC_READ_U16(data + 2) >> 12) & 0x000F;

#if 0
            EC_DBG("EOE fragment req received, fragment: %i, offset: %i,"
                   " frame %i%s%s, %i bytes\n", fragment_number,
                   fragment_offset, frame_number,
                   last_fragment ? ", last fragment" : "",
                   time_appended ? ", + timestamp" : "",
                   time_appended ? rec_size - 8 : rec_size - 4);

#if 0
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

            ec_eoedev_rx(eoe->dev, data + 4,
                         time_appended ? rec_size - 8 : rec_size - 4);
        }
        else {
#if 1
            EC_DBG("other frame received.\n");
#endif
        }

        eoe->rx_state = EC_EOE_IDLE;
        return;
    }
}

/*****************************************************************************/

/**
   Prints EoE object information.
*/

void ec_eoe_print(const ec_eoe_t *eoe)
{
    EC_INFO("  EoE slave %i\n", eoe->slave->ring_position);
    EC_INFO("    RX State %i\n", eoe->rx_state);
    EC_INFO("    Assigned device: %s (%s)\n", eoe->dev->name,
            eoe->opened ? "opened" : "closed");
}

/*****************************************************************************/

/**
   Initializes a net_device structure for an EoE object.
*/

void ec_eoedev_init(struct net_device *dev /**< pointer to the net_device */)
{
    ec_eoedev_priv_t *priv;
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
    memset(priv, 0, sizeof(ec_eoedev_priv_t));
}

/*****************************************************************************/

/**
   Opens the virtual network device.
*/

int ec_eoedev_open(struct net_device *dev /**< EoE net_device */)
{
    ec_eoedev_priv_t *priv = netdev_priv(dev);
    priv->eoe->opened = 1;
    netif_start_queue(dev);
    EC_INFO("%s (slave %i) opened.\n", dev->name,
            priv->eoe->slave->ring_position);
    return 0;
}

/*****************************************************************************/

/**
   Stops the virtual network device.
*/

int ec_eoedev_stop(struct net_device *dev /**< EoE net_device */)
{
    ec_eoedev_priv_t *priv = netdev_priv(dev);
    netif_stop_queue(dev);
    priv->eoe->opened = 0;
    EC_INFO("%s (slave %i) stopped.\n", dev->name,
            priv->eoe->slave->ring_position);
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
    ec_eoedev_priv_t *priv = netdev_priv(dev);
    priv->stats.tx_packets++;
    dev_kfree_skb(skb);
    EC_INFO("EoE device sent %i octets.\n", skb->len);
    return 0;
}

/*****************************************************************************/

/**
   Receives data from the bus and passes it to the network stack.
*/

void ec_eoedev_rx(struct net_device *dev, /**< EoE net_device */
                 const uint8_t *data, /**< pointer to the data */
                 size_t size /**< size of the received data */
                 )
{
    ec_eoedev_priv_t *priv = netdev_priv(dev);
    struct sk_buff *skb;

    // allocate socket buffer
    if (!(skb = dev_alloc_skb(size + 2))) {
        if (printk_ratelimit())
            EC_WARN("EoE RX: low on mem. frame dropped.\n");
        priv->stats.rx_dropped++;
        return;
    }

    // copy received data to socket buffer
    memcpy(skb_put(skb, size), data, size);

    // set socket buffer fields
    skb->dev = dev;
    skb->protocol = eth_type_trans(skb, dev);
    skb->ip_summed = CHECKSUM_UNNECESSARY;

    // update statistics
    priv->stats.rx_packets++;
    priv->stats.rx_bytes += size;

    // pass socket buffer to network stack
    netif_rx(skb);
}

/*****************************************************************************/

/**
   Gets statistics about the virtual network device.
*/

struct net_device_stats *ec_eoedev_stats(struct net_device *dev
                                         /**< EoE net_device */)
{
    ec_eoedev_priv_t *priv = netdev_priv(dev);
    return &priv->stats;
}

/*****************************************************************************/
