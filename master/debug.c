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
   Ethernet interface for debugging purposes.
*/

/*****************************************************************************/

#include <linux/etherdevice.h>

#include "globals.h"
#include "debug.h"

/*****************************************************************************/

// net_device functions
int ec_dbgdev_open(struct net_device *);
int ec_dbgdev_stop(struct net_device *);
struct net_device_stats *ec_dbgdev_stats(struct net_device *);

/*****************************************************************************/

/**
   Debug constructor.
   Initializes the debug object, creates a net_device and registeres it.
*/

int ec_debug_init(ec_debug_t *dbg /**< debug object */)
{
    int result;

    dbg->opened = 0;
    memset(&dbg->stats, 0, sizeof(struct net_device_stats));

    if (!(dbg->dev =
          alloc_netdev(sizeof(ec_debug_t *), "ec%d", ether_setup))) {
        EC_ERR("Unable to allocate net_device for debug object!\n");
        goto out_return;
    }

    // initialize net_device
    dbg->dev->open = ec_dbgdev_open;
    dbg->dev->stop = ec_dbgdev_stop;
    dbg->dev->get_stats = ec_dbgdev_stats;

    // initialize private data
    *((ec_debug_t **) netdev_priv(dbg->dev)) = dbg;

    // connect the net_device to the kernel
    if ((result = register_netdev(dbg->dev))) {
        EC_ERR("Unable to register net_device: error %i\n", result);
        goto out_free;
    }

    return 0;

 out_free:
    free_netdev(dbg->dev);
    dbg->dev = NULL;
 out_return:
    return -1;
}

/*****************************************************************************/

/**
   Debug destructor.
   Unregisteres the net_device and frees allocated memory.
*/

void ec_debug_clear(ec_debug_t *dbg /**< debug object */)
{
    if (dbg->dev) {
        unregister_netdev(dbg->dev);
        free_netdev(dbg->dev);
    }
}

/*****************************************************************************/

/**
   Sends frame data to the interface.
*/

void ec_debug_send(ec_debug_t *dbg, /**< debug object */
                   const uint8_t *data, /**< frame data */
                   size_t size /**< size of the frame data */
                   )
{
    struct sk_buff *skb;

    if (!dbg->opened) return;

    // allocate socket buffer
    if (!(skb = dev_alloc_skb(size))) {
        dbg->stats.rx_dropped++;
        return;
    }

    // copy frame contents into socket buffer
    memcpy(skb_put(skb, size), data, size);

    // update device statistics
    dbg->stats.rx_packets++;
    dbg->stats.rx_bytes += size;

    // pass socket buffer to network stack
    skb->dev = dbg->dev;
    skb->protocol = eth_type_trans(skb, dbg->dev);
    skb->ip_summed = CHECKSUM_UNNECESSARY;
    netif_rx(skb);
}

/******************************************************************************
 *  NET_DEVICE functions
 *****************************************************************************/

/**
   Opens the virtual network device.
*/

int ec_dbgdev_open(struct net_device *dev /**< debug net_device */)
{
    ec_debug_t *dbg = *((ec_debug_t **) netdev_priv(dev));
    dbg->opened = 1;
    EC_INFO("debug interface %s opened.\n", dev->name);
    return 0;
}

/*****************************************************************************/

/**
   Stops the virtual network device.
*/

int ec_dbgdev_stop(struct net_device *dev /**< debug net_device */)
{
    ec_debug_t *dbg = *((ec_debug_t **) netdev_priv(dev));
    dbg->opened = 0;
    EC_INFO("debug interface %s stopped.\n", dev->name);
    return 0;
}

/*****************************************************************************/

/**
   Gets statistics about the virtual network device.
*/

struct net_device_stats *ec_dbgdev_stats(struct net_device *dev
                                         /**< debug net_device */)
{
    ec_debug_t *dbg = *((ec_debug_t **) netdev_priv(dev));
    return &dbg->stats;
}

/*****************************************************************************/
