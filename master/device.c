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
   EtherCAT device methods.
*/

/*****************************************************************************/

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>

#include "device.h"
#include "master.h"

/*****************************************************************************/

/**
   Device constructor.
   \return 0 in case of success, else < 0
*/

int ec_device_init(ec_device_t *device, /**< EtherCAT device */
        ec_master_t *master /**< master owning the device */
        )
{
    device->master = master;

#ifdef EC_DEBUG_IF
    if (ec_debug_init(&device->dbg)) {
        EC_ERR("Failed to init debug device!\n");
        goto out_return;
    }
#endif

    if (!(device->tx_skb = dev_alloc_skb(ETH_FRAME_LEN))) {
        EC_ERR("Error allocating device socket buffer!\n");
#ifdef EC_DEBUG_IF
        goto out_debug;
#else
        goto out_return;
#endif
    }

    // add Ethernet-II-header
    skb_reserve(device->tx_skb, ETH_HLEN);
    device->eth = (struct ethhdr *) skb_push(device->tx_skb, ETH_HLEN);
    device->eth->h_proto = htons(0x88A4);
    memset(device->eth->h_dest, 0xFF, ETH_ALEN);

    ec_device_detach(device); // resets remaining fields
    return 0;

#ifdef EC_DEBUG_IF
 out_debug:
    ec_debug_clear(&device->dbg);
#endif
 out_return:
    return -1;
}

/*****************************************************************************/

/**
   EtherCAT device destuctor.
*/

void ec_device_clear(ec_device_t *device /**< EtherCAT device */)
{
    if (device->open) ec_device_close(device);
    dev_kfree_skb(device->tx_skb);
#ifdef EC_DEBUG_IF
    ec_debug_clear(&device->dbg);
#endif
}

/*****************************************************************************/

/**
   Associate with net_device.
*/

void ec_device_attach(ec_device_t *device, /**< EtherCAT device */
        struct net_device *net_dev, /**< net_device structure */
        ec_pollfunc_t poll, /**< pointer to device's poll function */
        struct module *module /**< the device's module */
        )
{
    ec_device_detach(device); // resets fields

    device->dev = net_dev;
    device->poll = poll;
    device->module = module;

    device->tx_skb->dev = net_dev;
    memcpy(device->eth->h_source, net_dev->dev_addr, ETH_ALEN);
}

/*****************************************************************************/

/**
   Disconnect from net_device.
*/

void ec_device_detach(ec_device_t *device /**< EtherCAT device */)
{
    device->dev = NULL;
    device->poll = NULL;
    device->module = NULL;
    device->open = 0;
    device->link_state = 0; // down
    device->tx_count = 0;
    device->rx_count = 0;
    device->tx_skb->dev = NULL;
}

/*****************************************************************************/

/**
   Opens the EtherCAT device.
   \return 0 in case of success, else < 0
*/

int ec_device_open(ec_device_t *device /**< EtherCAT device */)
{
    if (!device->dev) {
        EC_ERR("No net_device to open!\n");
        return -1;
    }

    if (device->open) {
        EC_WARN("Device already opened!\n");
        return 0;
    }

    device->link_state = 0;
    device->tx_count = 0;
    device->rx_count = 0;

    if (device->dev->open(device->dev) == 0) device->open = 1;

    return device->open ? 0 : -1;
}

/*****************************************************************************/

/**
   Stops the EtherCAT device.
   \return 0 in case of success, else < 0
*/

int ec_device_close(ec_device_t *device /**< EtherCAT device */)
{
    if (!device->dev) {
        EC_ERR("No device to close!\n");
        return -1;
    }

    if (!device->open) {
        EC_WARN("Device already closed!\n");
        return 0;
    }

    if (device->dev->stop(device->dev) == 0) device->open = 0;

    return !device->open ? 0 : -1;
}

/*****************************************************************************/

/**
   Returns a pointer to the device's transmit memory.
   \return pointer to the TX socket buffer
*/

uint8_t *ec_device_tx_data(ec_device_t *device /**< EtherCAT device */)
{
    return device->tx_skb->data + ETH_HLEN;
}

/*****************************************************************************/

/**
   Sends the content of the transmit socket buffer.
   Cuts the socket buffer content to the (now known) size, and calls the
   start_xmit() function of the assigned net_device.
*/

void ec_device_send(ec_device_t *device, /**< EtherCAT device */
                    size_t size /**< number of bytes to send */
                    )
{
    if (unlikely(!device->link_state)) // Link down
        return;

    // set the right length for the data
    device->tx_skb->len = ETH_HLEN + size;

    if (unlikely(device->master->debug_level > 1)) {
        EC_DBG("sending frame:\n");
        ec_print_data(device->tx_skb->data + ETH_HLEN, size);
    }

#ifdef EC_DEBUG_IF
    ec_debug_send(&device->dbg, device->tx_skb->data, ETH_HLEN + size);
#endif

    // start sending
    device->dev->hard_start_xmit(device->tx_skb, device->dev);
    device->tx_count++;
}

/*****************************************************************************/

/**
   Calls the poll function of the assigned net_device.
   The master itself works without using interrupts. Therefore the processing
   of received data and status changes of the network device has to be
   done by the master calling the ISR "manually".
*/

void ec_device_poll(ec_device_t *device /**< EtherCAT device */)
{
    device->cycles_poll = get_cycles();
    device->jiffies_poll = jiffies;
    device->poll(device->dev);
}

/******************************************************************************
 *  Device interface
 *****************************************************************************/

/**
   Accepts a received frame.
   Forwards the received data to the master. The master will analyze the frame
   and dispatch the received commands to the sending instances.
   \ingroup DeviceInterface
*/

void ecdev_receive(ec_device_t *device, /**< EtherCAT device */
                   const void *data, /**< pointer to received data */
                   size_t size /**< number of bytes received */
                   )
{
    device->rx_count++;

    if (unlikely(device->master->debug_level > 1)) {
        EC_DBG("Received frame:\n");
        ec_print_data_diff(device->tx_skb->data + ETH_HLEN,
                           data + ETH_HLEN, size - ETH_HLEN);
    }

#ifdef EC_DEBUG_IF
    ec_debug_send(&device->dbg, data, size);
#endif

    ec_master_receive_datagrams(device->master,
                                data + ETH_HLEN,
                                size - ETH_HLEN);
}

/*****************************************************************************/

/**
   Sets a new link state.
   If the device notifies the master about the link being down, the master
   will not try to send frames using this device.
   \ingroup DeviceInterface
*/

void ecdev_set_link(ec_device_t *device, /**< EtherCAT device */
        uint8_t state /**< new link state */
        )
{
    if (unlikely(!device)) {
        EC_WARN("ecdev_link_state: no device!\n");
        return;
    }

    if (likely(state != device->link_state)) {
        device->link_state = state;
        EC_INFO("Link state changed to %s.\n", (state ? "UP" : "DOWN"));
    }
}

/*****************************************************************************/

/**
   Reads the link state.
   \ingroup DeviceInterface
*/

uint8_t ecdev_get_link(ec_device_t *device /**< EtherCAT device */)
{
    if (unlikely(!device)) {
        EC_WARN("ecdev_link_state: no device!\n");
        return 0;
    }

    return device->link_state;
}

/*****************************************************************************/

/** \cond */

EXPORT_SYMBOL(ecdev_receive);
EXPORT_SYMBOL(ecdev_get_link);
EXPORT_SYMBOL(ecdev_set_link);

/** \endcond */

/*****************************************************************************/
