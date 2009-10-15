/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2008  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version 2, as
 *  published by the Free Software Foundation.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  ---
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 *****************************************************************************/

/** \file
 * EtherCAT generic Ethernet device module.
 */

/*****************************************************************************/

#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/version.h>
#include <linux/if_arp.h> /* ARPHRD_ETHER */
#include <linux/etherdevice.h>

#include "../globals.h"
#include "ecdev.h"

#define PFX "ec_generic: "

#define ETH_P_ETHERCAT 0x88A4

/*****************************************************************************/

int __init ec_gen_init_module(void);
void __exit ec_gen_cleanup_module(void);

/*****************************************************************************/

/** \cond */

MODULE_AUTHOR("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION("EtherCAT master generic Ethernet device module");
MODULE_LICENSE("GPL");
MODULE_VERSION(EC_MASTER_VERSION);

/** \endcond */

struct list_head generic_devices;

typedef struct {
    struct list_head list;
    struct net_device *netdev;
    struct socket *socket;
    ec_device_t *ecdev;
} ec_gen_device_t;

int ec_gen_device_open(ec_gen_device_t *);
int ec_gen_device_stop(ec_gen_device_t *);
int ec_gen_device_start_xmit(ec_gen_device_t *, struct sk_buff *);

/*****************************************************************************/

static int ec_gen_netdev_open(struct net_device *dev)
{
    ec_gen_device_t *gendev = *((ec_gen_device_t **) netdev_priv(dev));
    return ec_gen_device_open(gendev);
}

/*****************************************************************************/

static int ec_gen_netdev_stop(struct net_device *dev)
{
    ec_gen_device_t *gendev = *((ec_gen_device_t **) netdev_priv(dev));
    return ec_gen_device_stop(gendev);
}

/*****************************************************************************/

static int ec_gen_netdev_start_xmit(
        struct sk_buff *skb,
        struct net_device *dev
        )
{
    ec_gen_device_t *gendev = *((ec_gen_device_t **) netdev_priv(dev));
    return ec_gen_device_start_xmit(gendev, skb);
}

/*****************************************************************************/

void ec_gen_poll(struct net_device *dev)
{
}

/*****************************************************************************/

static const struct net_device_ops ec_gen_netdev_ops = {
	.ndo_open		= ec_gen_netdev_open,
	.ndo_stop		= ec_gen_netdev_stop,
	.ndo_start_xmit	= ec_gen_netdev_start_xmit,
};

/*****************************************************************************/

/** Init generic device.
 */
int ec_gen_device_init(
        ec_gen_device_t *dev,
        struct net_device *real_netdev
        )
{
    ec_gen_device_t **priv;
    char null = 0x00;

    dev->ecdev = NULL;
    dev->socket = NULL;

	dev->netdev = alloc_netdev(sizeof(ec_gen_device_t *), &null, ether_setup);
	if (!dev->netdev) {
		return -ENOMEM;
	}
    memcpy(dev->netdev->dev_addr, real_netdev->dev_addr, ETH_ALEN);
    dev->netdev->netdev_ops = &ec_gen_netdev_ops;
    priv = netdev_priv(dev->netdev);
    *priv = dev;

    return 0;
}

/*****************************************************************************/

/** Clear generic device.
 */
void ec_gen_device_clear(
        ec_gen_device_t *dev
        )
{
    if (dev->ecdev) {
        ecdev_close(dev->ecdev);
        ecdev_withdraw(dev->ecdev);
    }
    if (dev->socket) {
        sock_release(dev->socket);
    }
    free_netdev(dev->netdev);
}

/*****************************************************************************/

/** Creates a network socket.
 */
int ec_gen_device_create_socket(
        ec_gen_device_t *dev,
        struct net_device *real_netdev
        )
{
    int ret;
    struct sockaddr_ll sa;

    ret = sock_create_kern(PF_PACKET, SOCK_RAW, htons(ETH_P_ETHERCAT), &dev->socket);
    if (ret) {
        printk(KERN_ERR PFX "Failed to create socket.\n");
        return ret;
    }

    printk(KERN_ERR PFX "Binding socket to interface %i (%s).\n",
            real_netdev->ifindex, real_netdev->name);

    memset(&sa, 0x00, sizeof(sa));
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(ETH_P_ETHERCAT);
    sa.sll_ifindex = real_netdev->ifindex;
    ret = kernel_bind(dev->socket, (struct sockaddr *) &sa, sizeof(sa));
    if (ret) {
        printk(KERN_ERR PFX "Failed to bind() socket to interface.\n");
        sock_release(dev->socket);
        dev->socket = NULL;
        return ret;
    }

    return 0;
}

/*****************************************************************************/

/** Offer generic device to master.
 */
int ec_gen_device_offer(
        ec_gen_device_t *dev,
        struct net_device *real_netdev
        )
{
    int ret = 0;

	dev->ecdev = ecdev_offer(dev->netdev, ec_gen_poll, THIS_MODULE);
    if (dev->ecdev) {
        if (ec_gen_device_create_socket(dev, real_netdev)) {
            ecdev_withdraw(dev->ecdev);
            dev->ecdev = NULL;
        } else if (ecdev_open(dev->ecdev)) {
            ecdev_withdraw(dev->ecdev);
            dev->ecdev = NULL;
        } else {
            ecdev_set_link(dev->ecdev, 1); // FIXME
            ret = 1;
        }
    }

    return ret;
}

/*****************************************************************************/

/** Open the device.
 */
int ec_gen_device_open(
        ec_gen_device_t *dev
        )
{
    return 0;
}

/*****************************************************************************/

/** Stop the device.
 */
int ec_gen_device_stop(
        ec_gen_device_t *dev
        )
{
    return 0;
}

/*****************************************************************************/

int ec_gen_device_start_xmit(
        ec_gen_device_t *dev,
        struct sk_buff *skb
        )
{
    struct msghdr msg;
    struct kvec iov;
    size_t len = skb->len;
    int ret;

    iov.iov_base = skb->data;
    iov.iov_len = len;
    memset(&msg, 0, sizeof(msg));

    ret = kernel_sendmsg(dev->socket, &msg, &iov, 1, len);

    return ret == len ? NETDEV_TX_OK : NETDEV_TX_BUSY;
}

/*****************************************************************************/

/** Offer device.
 */
int offer_device(
        struct net_device *netdev
        )
{
    ec_gen_device_t *gendev;
    int ret = 0;

    gendev = kmalloc(sizeof(ec_gen_device_t), GFP_KERNEL);
    if (!gendev) {
        return -ENOMEM;
    }

    ret = ec_gen_device_init(gendev, netdev);
    if (ret) {
        kfree(gendev);
        return ret;
    }

    if (ec_gen_device_offer(gendev, netdev)) {
        list_add_tail(&gendev->list, &generic_devices);
    } else {
        ec_gen_device_clear(gendev);
        kfree(gendev);
    }

    return ret;
}

/*****************************************************************************/

/** Clear devices.
 */
void clear_devices(void)
{
    ec_gen_device_t *gendev, *next;

    list_for_each_entry_safe(gendev, next, &generic_devices, list) {
        list_del(&gendev->list);
        ec_gen_device_clear(gendev);
        kfree(gendev);
    }
}

/*****************************************************************************/

/** Module initialization.
 *
 * Initializes \a master_count masters.
 * \return 0 on success, else < 0
 */
int __init ec_gen_init_module(void)
{
    int ret = 0;
    struct net_device *netdev;

    printk(KERN_INFO PFX "EtherCAT master generic Ethernet device module %s\n",
            EC_MASTER_VERSION);

    INIT_LIST_HEAD(&generic_devices);

    read_lock(&dev_base_lock);
    for_each_netdev(&init_net, netdev) {
        if (netdev->type != ARPHRD_ETHER)
            continue;
        ret = offer_device(netdev);
        if (ret) {
            read_unlock(&dev_base_lock);
            goto out_err;
        }
    }
    read_unlock(&dev_base_lock);
    return ret;

out_err:
    clear_devices();
    return ret;
}

/*****************************************************************************/

/** Module cleanup.
 *
 * Clears all master instances.
 */
void __exit ec_gen_cleanup_module(void)
{
    clear_devices();
    printk(KERN_INFO PFX "Unloading.\n");
}

/*****************************************************************************/

/** \cond */

module_init(ec_gen_init_module);
module_exit(ec_gen_cleanup_module);

/** \endcond */

/*****************************************************************************/
