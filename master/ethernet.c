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

/**
   \file
   Ethernet over EtherCAT (EoE).
*/

/*****************************************************************************/

#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#include "globals.h"
#include "master.h"
#include "slave.h"
#include "mailbox.h"
#include "ethernet.h"

/*****************************************************************************/

/** Defines the debug level of EoE processing.
 *
 * 0 = No debug messages.
 * 1 = Output warnings.
 * 2 = Output actions.
 * 3 = Output actions and frame data.
 */
#define EOE_DEBUG_LEVEL 1

/** Size of the EoE tx ring.
 */
#define EC_EOE_TX_RING_SIZE 100

/** Number of tries.
 */
#define EC_EOE_TRIES 100

/*****************************************************************************/

void ec_eoe_flush(ec_eoe_t *);
static unsigned int eoe_tx_unused_frames(ec_eoe_t *);

// state functions
void ec_eoe_state_rx_start(ec_eoe_t *);
void ec_eoe_state_rx_check(ec_eoe_t *);
void ec_eoe_state_rx_fetch(ec_eoe_t *);
void ec_eoe_state_rx_fetch_data(ec_eoe_t *);
void ec_eoe_state_tx_start(ec_eoe_t *);
void ec_eoe_state_tx_sent(ec_eoe_t *);

// net_device functions
int ec_eoedev_open(struct net_device *);
int ec_eoedev_stop(struct net_device *);
int ec_eoedev_tx(struct sk_buff *, struct net_device *);
struct net_device_stats *ec_eoedev_stats(struct net_device *);
static int ec_eoedev_set_mac(struct net_device *netdev, void *p);

/*****************************************************************************/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
/** Device operations for EoE interfaces.
 */
static const struct net_device_ops ec_eoedev_ops = {
    .ndo_open = ec_eoedev_open,
    .ndo_stop = ec_eoedev_stop,
    .ndo_start_xmit = ec_eoedev_tx,
    .ndo_get_stats = ec_eoedev_stats,
    .ndo_set_mac_address = ec_eoedev_set_mac,
};
#endif

/*****************************************************************************/

/**
 * ec_eoedev_set_mac - Change the Ethernet Address of the NIC
 * @netdev: network interface device structure
 * @p: pointer to an address structure
 *
 * Returns 0 on success, negative on failure
 **/
static int
ec_eoedev_set_mac(struct net_device *netdev, void *p)
{
   struct sockaddr *addr = p;

   if (!is_valid_ether_addr(addr->sa_data)) {
      return -EADDRNOTAVAIL;
   }

   memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);

   return 0;
}

/*****************************************************************************/

/** Parse an eoe interface from a string.
 *
 * The eoe interface must match the regular expression
 * "eoe([0-9]*)([as])([0-9]*)".
 *
 * \return 0 on success, else < 0
 */
int ec_eoe_parse(const char *eoe, int *master_idx, 
        uint16_t *alias, uint16_t *posn)
{
    unsigned int value;
    const char *orig = eoe;
    char *rem;

    if (!strlen(eoe)) {
        EC_ERR("EOE interface may not be empty.\n");
        return -EINVAL;
    }
    
    // must start with "eoe"
    if (strncmp(eoe, "eoe", 3) != 0) {
        EC_ERR("Invalid EOE interface \"%s\".\n", orig);
        return -EINVAL;
    }
    eoe += 3;

    // get master index, this does not check if the master index
    // is valid beyond checking that it is not negative
    value = simple_strtoul(eoe, &rem, 10);
    if (value < 0) {
        EC_ERR("Invalid EOE interface \"%s\", master index: %d\n", orig, value);
        return -EINVAL;
    }
    *master_idx = value;
    eoe = rem;
    
    // get alias or position specifier
    if (eoe[0] == 'a') {
        eoe++;
        value = simple_strtoul(eoe, &rem, 10);
        if ((value <= 0) || (value >= 0xFFFF)) {
            EC_ERR("Invalid EOE interface \"%s\", invalid alias: %d\n", 
                    orig, value);
            return -EINVAL;
        }
        *alias = value;
        *posn = 0;
    } else if (eoe[0] == 's') {
        eoe++;
        value = simple_strtoul(eoe, &rem, 10);
        if ((value < 0) || (value >= 0xFFFF)) {
            EC_ERR("Invalid EOE interface \"%s\", invalid ring position: %d\n",
                    orig, value);
            return -EINVAL;
        }
        *alias = 0;
        *posn = value;
    } else {
        EC_ERR("Invalid EOE interface \"%s\", invalid alias/position specifier: %c\n",
                orig, eoe[0]);
        return -EINVAL;
    }
    
    // check no remainder
    if (rem[0] != '\0') {
        EC_ERR("Invalid EOE interface \"%s\", unexpected end characters: %s\n",
                orig, rem);
        return -EINVAL;
    }

    return 0;
}

/*****************************************************************************/

/** EoE explicit init constructor.
 *
 * Initializes the EoE handler before a slave is configured, creates a 
 * net_device and registers it.
 *
 * \return Zero on success, otherwise a negative error code.
 */
int ec_eoe_init(
        ec_master_t *master, /**< EtherCAT master */
        ec_eoe_t *eoe, /**< EoE handler */
        uint16_t alias, /**< EtherCAT slave alias */
        uint16_t ring_position /**< EtherCAT slave ring position */
        )
{
    ec_eoe_t **priv;
    int ret = 0;
    char name[EC_DATAGRAM_NAME_SIZE];

    struct net_device *dev;
    unsigned char lo_mac[ETH_ALEN] = {0};
    unsigned int use_master_mac = 0;

    eoe->master = master;
    eoe->slave = NULL;
    eoe->have_mbox_lock = 0;
    eoe->auto_created = 0;

    ec_datagram_init(&eoe->datagram);
    eoe->queue_datagram = 0;
    eoe->state = ec_eoe_state_rx_start;
    eoe->opened = 0;
    eoe->rx_skb = NULL;
    eoe->rx_expected_fragment = 0;

    eoe->tx_ring_count = EC_EOE_TX_RING_SIZE;
    eoe->tx_ring_size = sizeof(struct sk_buff *) * eoe->tx_ring_count;
    eoe->tx_ring = kmalloc(eoe->tx_ring_size, GFP_KERNEL);
    memset(eoe->tx_ring, 0, eoe->tx_ring_size);
    eoe->tx_next_to_use = 0;
    eoe->tx_next_to_clean = 0;
    eoe->tx_skb = NULL;
    eoe->tx_queue_active = 0;

    eoe->tx_frame_number = 0xFF;
    memset(&eoe->stats, 0, sizeof(struct net_device_stats));

    eoe->rx_counter = 0;
    eoe->tx_counter = 0;
    eoe->rx_rate = 0;
    eoe->tx_rate = 0;
    eoe->rate_jiffies = 0;
    eoe->rx_idle = 1;
    eoe->tx_idle = 1;

    /* device name eoe<MASTER>[as]<SLAVE>, because networking scripts don't
     * like hyphens etc. in interface names. */
    if (alias) {
        snprintf(name, EC_DATAGRAM_NAME_SIZE, "eoe%ua%u", master->index, alias);
    } else {
        snprintf(name, EC_DATAGRAM_NAME_SIZE, "eoe%us%u", master->index, ring_position);
    }

    snprintf(eoe->datagram.name, EC_DATAGRAM_NAME_SIZE, name);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
    eoe->dev = alloc_netdev(sizeof(ec_eoe_t *), name, NET_NAME_UNKNOWN,
            ether_setup);
#else
    eoe->dev = alloc_netdev(sizeof(ec_eoe_t *), name, ether_setup);
#endif
    if (!eoe->dev) {
        EC_MASTER_ERR(master, "Unable to allocate net_device %s"
                " for EoE handler!\n", name);
        ret = -ENODEV;
        goto out_return;
    }

    // initialize net_device
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
    eoe->dev->netdev_ops = &ec_eoedev_ops;
#else
    eoe->dev->open = ec_eoedev_open;
    eoe->dev->stop = ec_eoedev_stop;
    eoe->dev->hard_start_xmit = ec_eoedev_tx;
    eoe->dev->get_stats = ec_eoedev_stats;
#endif

    // First check if the MAC address assigned to the master is globally
    // unique
    if ((master->devices[EC_DEVICE_MAIN].dev->dev_addr[0] & 0x02) !=
            0x02) {
        // The master MAC is unique and the NIC part can be used for the EoE
        // interface MAC
        use_master_mac = 1;
    }
    else {
        // The master MAC is not unique, so we check for unique MAC in other
        // interfaces
        dev = first_net_device(&init_net);
        while (dev) {
            // Check if globally unique MAC address
            if (dev->addr_len == ETH_ALEN) {
                if (memcmp(dev->dev_addr, lo_mac, ETH_ALEN) != 0) {
                    if ((dev->dev_addr[0] & 0x02) != 0x02) {
                        // The first globally unique MAC address has been
                        // identified
                        break;
                    }
                }
            }
            dev = next_net_device(dev);
        }
        if (eoe->dev->addr_len == ETH_ALEN) {
            if (dev) {
                // A unique MAC were identified in one of the other network
                // interfaces and the NIC part can be used for the EoE
                // interface MAC.
                EC_MASTER_INFO(master, "%s MAC address derived from"
                        " NIC part of %s MAC address\n",
                    eoe->dev->name, dev->name);
                eoe->dev->dev_addr[1] = dev->dev_addr[3];
                eoe->dev->dev_addr[2] = dev->dev_addr[4];
                eoe->dev->dev_addr[3] = dev->dev_addr[5];
            }
            else {
                use_master_mac = 1;
            }
        }
    }
    if (eoe->dev->addr_len == ETH_ALEN) {
        if (use_master_mac) {
            EC_MASTER_INFO(master, "%s MAC address derived"
                    " from NIC part of %s MAC address\n",
                eoe->dev->name,
                master->devices[EC_DEVICE_MAIN].dev->name);
            eoe->dev->dev_addr[1] =
                master->devices[EC_DEVICE_MAIN].dev->dev_addr[3];
            eoe->dev->dev_addr[2] =
                master->devices[EC_DEVICE_MAIN].dev->dev_addr[4];
            eoe->dev->dev_addr[3] =
                master->devices[EC_DEVICE_MAIN].dev->dev_addr[5];
        }
        eoe->dev->dev_addr[0] = 0x02;
        if (alias) {
            eoe->dev->dev_addr[4] = (uint8_t)(alias >> 8);
            eoe->dev->dev_addr[5] = (uint8_t)(alias);
        } else {
            eoe->dev->dev_addr[4] = (uint8_t)(ring_position >> 8);
            eoe->dev->dev_addr[5] = (uint8_t)(ring_position);
        }
    }

    // initialize private data
    priv = netdev_priv(eoe->dev);
    *priv = eoe;

    // connect the net_device to the kernel
    ret = register_netdev(eoe->dev);
    if (ret) {
        EC_MASTER_ERR(master, "Unable to register net_device for %s:"
                " error %i\n", eoe->dev->name, ret);
        goto out_free;
    }

    // set carrier off status BEFORE open */
    EC_MASTER_DBG(eoe->master, 1, "%s: carrier off.\n", eoe->dev->name);
    netif_carrier_off(eoe->dev);

    return 0;

 out_free:
    free_netdev(eoe->dev);
    eoe->dev = NULL;
 out_return:
    return ret;
}

/*****************************************************************************/

/** EoE auto constructor for slave.
 *
 * Initializes the EoE handler, creates a net_device and registers it.
 *
 * \return Zero on success, otherwise a negative error code.
 */
int ec_eoe_auto_init(
        ec_eoe_t *eoe, /**< EoE handler */
        ec_slave_t *slave /**< EtherCAT slave */
        )
{
    int ret = 0;

    if ((ret = ec_eoe_init(slave->master, eoe, slave->effective_alias,
            slave->ring_position)) != 0) {
        return ret;
    }
    
    // set auto created flag
    eoe->auto_created = 1;

    ec_eoe_link_slave(eoe, slave);

    return ret;
}

/*****************************************************************************/

/** EoE link slave.
 *
 * links a slave to a handler after a slave is connected or reconfigured
 * during a rescan.
 */
void ec_eoe_link_slave(
        ec_eoe_t *eoe, /**< EoE handler */
        ec_slave_t *slave /**< EtherCAT slave */
        )
{
    eoe->slave = slave;

    if (eoe->slave) {
        EC_SLAVE_INFO(slave, "Linked to EoE handler %s\n",
                eoe->dev->name);

        // Usually setting the MTU appropriately makes the upper layers
        // do the frame fragmenting. In some cases this doesn't work
        // so the MTU is left on the Ethernet standard value and fragmenting
        // is done "manually".
#if 0
        eoe->dev->mtu = slave->configured_rx_mailbox_size - ETH_HLEN - 10;
#endif

        EC_MASTER_DBG(eoe->master, 1, "%s: carrier on.\n", eoe->dev->name);
        netif_carrier_on(eoe->dev);
    } else {
        EC_MASTER_ERR(eoe->master, "%s : slave not supplied to ec_eoe_link_slave().\n",
                eoe->dev->name);
        EC_MASTER_DBG(eoe->master, 1, "%s: carrier off.\n", eoe->dev->name);
        netif_carrier_off(eoe->dev);
    }
}

/*****************************************************************************/

/** EoE clear slave.
 *
 * delinks slave from the handler so that the EoE interface is kept if a
 * slave get disconnected.
 */
void ec_eoe_clear_slave(ec_eoe_t *eoe /**< EoE handler */)
{
#if EOE_DEBUG_LEVEL >= 1
    ec_slave_t *slave = eoe->slave;
#endif

    EC_MASTER_DBG(eoe->master, 1, "%s: carrier off.\n", eoe->dev->name);
    netif_carrier_off(eoe->dev);

    // empty transmit queue
    ec_eoe_flush(eoe);

    if (eoe->tx_skb) {
        dev_kfree_skb(eoe->tx_skb);
        eoe->tx_skb = NULL;
        eoe->stats.tx_errors++;
    }

    if (eoe->rx_skb) {
        dev_kfree_skb(eoe->rx_skb);
        eoe->rx_skb = NULL;
        eoe->stats.rx_errors++;
    }

    eoe->state = ec_eoe_state_rx_start;
        
    eoe->slave = NULL;

#if EOE_DEBUG_LEVEL >= 1
    if (slave) {
        EC_MASTER_DBG(eoe->master, 0, "%s slave link cleared.\n", eoe->dev->name);
    }
#endif
}

/*****************************************************************************/

/** EoE destructor.
 *
 * Unregisteres the net_device and frees allocated memory.
 */
void ec_eoe_clear(ec_eoe_t *eoe /**< EoE handler */)
{
    unregister_netdev(eoe->dev); // possibly calls close callback

    // empty transmit queue
    ec_eoe_flush(eoe);

    if (eoe->tx_skb)
        dev_kfree_skb(eoe->tx_skb);

    if (eoe->rx_skb)
        dev_kfree_skb(eoe->rx_skb);

    kfree(eoe->tx_ring);

    free_netdev(eoe->dev);

    ec_datagram_clear(&eoe->datagram);
}

/*****************************************************************************/

/** Empties the transmit queue.
 */
void ec_eoe_flush(ec_eoe_t *eoe /**< EoE handler */)
{
    struct sk_buff *skb;

    if (eoe->have_mbox_lock) {
        eoe->have_mbox_lock = 0;
        ec_read_mbox_lock_clear(eoe->slave);
    }

    while (eoe->tx_next_to_clean != eoe->tx_next_to_use) {
        skb = eoe->tx_ring[eoe->tx_next_to_clean];
        dev_kfree_skb(skb);
        eoe->tx_ring[eoe->tx_next_to_clean] = NULL;

        eoe->stats.tx_dropped++;

        if (unlikely(++eoe->tx_next_to_clean == eoe->tx_ring_count)) {
            eoe->tx_next_to_clean = 0;
        }
    }

    eoe->tx_next_to_use = 0;
    eoe->tx_next_to_clean = 0;
}

/*****************************************************************************/

unsigned int ec_eoe_tx_queued_frames(const ec_eoe_t *eoe /**< EoE handler */)
{
    unsigned int next_to_use = eoe->tx_next_to_use;
    unsigned int next_to_clean = eoe->tx_next_to_clean;

    if (next_to_use >= next_to_clean) {
        return next_to_use - next_to_clean;
    } else {
        return next_to_use + eoe->tx_ring_count - next_to_clean;
    }
}

/*****************************************************************************/

static unsigned int eoe_tx_unused_frames(ec_eoe_t *eoe /**< EoE handler */)
{
    unsigned int next_to_use = eoe->tx_next_to_use;
    unsigned int next_to_clean = eoe->tx_next_to_clean;

    // Note: -1 to avoid tail touching head
    if (next_to_clean > next_to_use) {
        return next_to_clean - next_to_use - 1;
    } else {
        return next_to_clean + eoe->tx_ring_count - next_to_use - 1;
    }
}

/*****************************************************************************/

/** Sends a frame or the next fragment.
 *
 * \return Zero on success, otherwise a negative error code.
 */
int ec_eoe_send(ec_eoe_t *eoe /**< EoE handler */)
{
    size_t remaining_size, current_size, complete_offset;
    unsigned int last_fragment;
    uint8_t *data;
#if EOE_DEBUG_LEVEL >= 3
    unsigned int i;
#endif

    if (!eoe->slave) {
        return -ECHILD;
    }

    remaining_size = eoe->tx_skb->len - eoe->tx_offset;

    if (remaining_size <= eoe->slave->configured_tx_mailbox_size - 10) {
        current_size = remaining_size;
        last_fragment = 1;
    } else {
        current_size =
            ((eoe->slave->configured_tx_mailbox_size - 10) / 32) * 32;
        last_fragment = 0;
    }

    if (eoe->tx_fragment_number) {
        complete_offset = eoe->tx_offset / 32;
    }
    else {
        // complete size in 32 bit blocks, rounded up.
        complete_offset = remaining_size / 32 + 1;
    }

#if EOE_DEBUG_LEVEL >= 2
    EC_SLAVE_DBG(eoe->slave, 0, "EoE %s TX sending fragment %u%s"
            " with %zu octets (%zu). %u frames queued.\n",
            eoe->dev->name, eoe->tx_fragment_number,
            last_fragment ? "" : "+", current_size, complete_offset,
            ec_eoe_tx_queued_frames(eoe));
#endif

#if EOE_DEBUG_LEVEL >= 3
    EC_SLAVE_DBG(eoe->slave, 0, "");
    for (i = 0; i < current_size; i++) {
        printk(KERN_CONT "%02X ", eoe->tx_skb->data[eoe->tx_offset + i]);
        if ((i + 1) % 16 == 0) {
            printk(KERN_CONT "\n");
            EC_SLAVE_DBG(eoe->slave, 0, "");
        }
    }
    printk(KERN_CONT "\n");
#endif

    data = ec_slave_mbox_prepare_send(eoe->slave, &eoe->datagram,
            EC_MBOX_TYPE_EOE, current_size + 4);
    if (IS_ERR(data)) {
        return PTR_ERR(data);
    }

    EC_WRITE_U8 (data, EC_EOE_TYPE_FRAME_FRAG); // Initiate EoE Tx Request
    EC_WRITE_U8 (data + 1, last_fragment);
    EC_WRITE_U16(data + 2, ((eoe->tx_fragment_number & 0x3F) |
                            (complete_offset & 0x3F) << 6 |
                            (eoe->tx_frame_number & 0x0F) << 12));

    memcpy(data + 4, eoe->tx_skb->data + eoe->tx_offset, current_size);
    eoe->queue_datagram = 1;

    eoe->tx_offset += current_size;
    eoe->tx_fragment_number++;
    return 0;
}

/*****************************************************************************/

/** Runs the EoE state machine.
 */
void ec_eoe_run(ec_eoe_t *eoe /**< EoE handler */)
{
    if (!eoe->opened || !eoe->slave || !netif_carrier_ok(eoe->dev)) {
        return;
    }

    // if the datagram was not sent, or is not yet received, skip this cycle
    if (eoe->queue_datagram || eoe->datagram.state == EC_DATAGRAM_SENT) {
        return;
    }

    // call state function
    eoe->state(eoe);

    // update statistics
    if (jiffies - eoe->rate_jiffies > HZ) {
        eoe->rx_rate = eoe->rx_counter;
        eoe->tx_rate = eoe->tx_counter;
        eoe->rx_counter = 0;
        eoe->tx_counter = 0;
        eoe->rate_jiffies = jiffies;
    }

    ec_datagram_output_stats(&eoe->datagram);
}

/*****************************************************************************/

/** Queues the datagram, if necessary.
 */
void ec_eoe_queue(ec_eoe_t *eoe /**< EoE handler */)
{
   if (eoe->queue_datagram && eoe->slave) {
       ec_master_queue_datagram_ext(eoe->slave->master, &eoe->datagram);
       eoe->queue_datagram = 0;
   }
}

/*****************************************************************************/

/** Returns the state of the device.
 *
 * \return 1 if the device is "up", 0 if it is "down"
 */
int ec_eoe_is_open(const ec_eoe_t *eoe /**< EoE handler */)
{
    return eoe->opened;
}

/*****************************************************************************/

/** Returns the idle state.
 *
 * \retval 1 The device is idle.
 * \retval 0 The device is busy.
 */
int ec_eoe_is_idle(const ec_eoe_t *eoe /**< EoE handler */)
{
    return eoe->rx_idle && eoe->tx_idle;
}

/*****************************************************************************/

/** Returns the eoe device name.
 *
 * \retval the device name.
 */
char *ec_eoe_name(const ec_eoe_t *eoe /**< EoE handler */)
{
    return eoe->dev->name;
}

/******************************************************************************
 *  STATE PROCESSING FUNCTIONS
 *****************************************************************************/

/** State: RX_START.
 *
 * Starts a new receiving sequence by queueing a datagram that checks the
 * slave's mailbox for a new EoE datagram.
 *
 * \todo Use both devices.
 */
void ec_eoe_state_rx_start(ec_eoe_t *eoe /**< EoE handler */)
{
    if (!eoe->slave || eoe->slave->error_flag ||
            !eoe->slave->master->devices[EC_DEVICE_MAIN].link_state) {
        eoe->rx_idle = 1;
        eoe->tx_idle = 1;
        return;
    }

    // mailbox read check is skipped if a read request is already ongoing
    if (ec_read_mbox_locked(eoe->slave)) {
        eoe->state = ec_eoe_state_rx_fetch_data;
    } else {
        eoe->have_mbox_lock = 1;
        ec_slave_mbox_prepare_check(eoe->slave, &eoe->datagram);
        eoe->queue_datagram = 1;
        eoe->state = ec_eoe_state_rx_check;
    }
}

/*****************************************************************************/

/** State: RX_CHECK.
 *
 * Processes the checking datagram sent in RX_START and issues a receive
 * datagram, if new data is available.
 */
void ec_eoe_state_rx_check(ec_eoe_t *eoe /**< EoE handler */)
{
    if (eoe->datagram.state != EC_DATAGRAM_RECEIVED) {
#if EOE_DEBUG_LEVEL >= 1
        EC_SLAVE_WARN(eoe->slave, "Failed to receive mbox"
                " check datagram for %s.\n", eoe->dev->name);
        eoe->stats.rx_errors++;
#endif
        eoe->state = ec_eoe_state_tx_start;
        eoe->have_mbox_lock = 0;
        ec_read_mbox_lock_clear(eoe->slave);
        return;
    }

    if (!ec_slave_mbox_check(&eoe->datagram)) {
        eoe->rx_idle = 1;
        eoe->have_mbox_lock = 0;
        ec_read_mbox_lock_clear(eoe->slave);
        // check that data is not already received by another read request
        if (eoe->slave->mbox_eoe_frag_data.payload_size > 0) {
            eoe->state = ec_eoe_state_rx_fetch_data;
            eoe->state(eoe);
        } else {
            eoe->state = ec_eoe_state_tx_start;
        }
        return;
    }

    eoe->rx_idle = 0;
    ec_slave_mbox_prepare_fetch(eoe->slave, &eoe->datagram);
    eoe->queue_datagram = 1;
    eoe->state = ec_eoe_state_rx_fetch;
}

/*****************************************************************************/

/** State: RX_FETCH.
 *
 * Checks if the requested data of RX_CHECK was received and processes the EoE
 * datagram.
 */
void ec_eoe_state_rx_fetch(ec_eoe_t *eoe /**< EoE handler */)
{
    if (eoe->datagram.state != EC_DATAGRAM_RECEIVED) {
        eoe->stats.rx_errors++;
#if EOE_DEBUG_LEVEL >= 1
        EC_SLAVE_WARN(eoe->slave, "Failed to receive mbox"
                " fetch datagram for %s.\n", eoe->dev->name);
#endif
        eoe->state = ec_eoe_state_tx_start;
        eoe->have_mbox_lock = 0;
        ec_read_mbox_lock_clear(eoe->slave);
        return;
    }
    eoe->have_mbox_lock = 0;
    ec_read_mbox_lock_clear(eoe->slave);
    eoe->state = ec_eoe_state_rx_fetch_data;
    eoe->state(eoe);
}



/*****************************************************************************/

/** State: RX_FETCH DATA.
 *
 * Processes the EoE data.
 */
void ec_eoe_state_rx_fetch_data(ec_eoe_t *eoe /**< EoE handler */)
{
    size_t rec_size, data_size;
    uint8_t *data, eoe_type, last_fragment, time_appended, mbox_prot;
    uint8_t fragment_offset, fragment_number;
#if EOE_DEBUG_LEVEL >= 2
    uint8_t frame_number;
#endif
    off_t offset;
#if EOE_DEBUG_LEVEL >= 3
    unsigned int i;
#endif

    if (eoe->slave->mbox_eoe_frag_data.payload_size > 0) {
        eoe->slave->mbox_eoe_frag_data.payload_size = 0;
    } else {
        // initiate a new mailbox read check if required data is not available
        if (!ec_read_mbox_locked(eoe->slave)) {
            eoe->have_mbox_lock = 1;
            ec_slave_mbox_prepare_check(eoe->slave, &eoe->datagram);
            eoe->queue_datagram = 1;
            eoe->state = ec_eoe_state_rx_check;
        }
        return;
    }

    data = ec_slave_mbox_fetch(eoe->slave, &eoe->slave->mbox_eoe_frag_data,
            &mbox_prot, &rec_size);
    if (IS_ERR(data)) {
        eoe->stats.rx_errors++;
#if EOE_DEBUG_LEVEL >= 1
        EC_SLAVE_WARN(eoe->slave, "Invalid mailbox response for %s.\n",
                eoe->dev->name);
#endif
        eoe->state = ec_eoe_state_tx_start;
        return;
    }

    if (mbox_prot != EC_MBOX_TYPE_EOE) { // FIXME mailbox handler necessary
        eoe->stats.rx_errors++;
#if EOE_DEBUG_LEVEL >= 1
        EC_SLAVE_WARN(eoe->slave, "Other mailbox protocol response for %s.\n",
                eoe->dev->name);
#endif
        eoe->state = ec_eoe_state_tx_start;
        return;
    }

    eoe_type = EC_READ_U8(data) & 0x0F;

    if (eoe_type != EC_EOE_TYPE_FRAME_FRAG) {
        EC_SLAVE_ERR(eoe->slave, "%s: EoE iface handler received other EoE type"
                " response (type %x). Dropping.\n", eoe->dev->name, eoe_type);
        eoe->stats.rx_dropped++;
        eoe->state = ec_eoe_state_tx_start;
        return;
    }

    // EoE Fragment Request received

    last_fragment = (EC_READ_U16(data) >> 8) & 0x0001;
    time_appended = (EC_READ_U16(data) >> 9) & 0x0001;
    fragment_number = EC_READ_U16(data + 2) & 0x003F;
    fragment_offset = (EC_READ_U16(data + 2) >> 6) & 0x003F;
#if EOE_DEBUG_LEVEL >= 2
    frame_number = (EC_READ_U16(data + 2) >> 12) & 0x000F;
#endif

#if EOE_DEBUG_LEVEL >= 2
    EC_SLAVE_DBG(eoe->slave, 0, "EoE %s RX fragment %u%s, offset %u,"
            " frame %u%s, %zu octets\n", eoe->dev->name, fragment_number,
           last_fragment ? "" : "+", fragment_offset, frame_number,
           time_appended ? ", + timestamp" : "",
           time_appended ? rec_size - 8 : rec_size - 4);
#endif

#if EOE_DEBUG_LEVEL >= 3
    EC_SLAVE_DBG(eoe->slave, 0, "");
    for (i = 0; i < rec_size - 4; i++) {
        printk(KERN_CONT "%02X ", data[i + 4]);
        if ((i + 1) % 16 == 0) {
            printk(KERN_CONT "\n");
            EC_SLAVE_DBG(eoe->slave, 0, "");
        }
    }
    printk(KERN_CONT "\n");
#endif

    data_size = time_appended ? rec_size - 8 : rec_size - 4;

    if (!fragment_number) {
        if (eoe->rx_skb) {
            EC_SLAVE_WARN(eoe->slave, "EoE RX freeing old socket buffer.\n");
            dev_kfree_skb(eoe->rx_skb);
        }

        // new socket buffer
        if (!(eoe->rx_skb = dev_alloc_skb(fragment_offset * 32))) {
            if (printk_ratelimit())
                EC_SLAVE_WARN(eoe->slave, "EoE RX low on mem,"
                        " frame dropped.\n");
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
            dev_kfree_skb(eoe->rx_skb);
            eoe->rx_skb = NULL;
            eoe->stats.rx_errors++;
#if EOE_DEBUG_LEVEL >= 1
            EC_SLAVE_WARN(eoe->slave, "Fragmenting error at %s.\n",
                    eoe->dev->name);
#endif
            eoe->state = ec_eoe_state_tx_start;
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
        eoe->rx_counter += eoe->rx_skb->len;

#if EOE_DEBUG_LEVEL >= 2
        EC_SLAVE_DBG(eoe->slave, 0, "EoE %s RX frame completed"
                " with %u octets.\n", eoe->dev->name, eoe->rx_skb->len);
#endif

        // pass socket buffer to network stack
        eoe->rx_skb->dev = eoe->dev;
        eoe->rx_skb->protocol = eth_type_trans(eoe->rx_skb, eoe->dev);
        eoe->rx_skb->ip_summed = CHECKSUM_UNNECESSARY;
        if (netif_rx_ni(eoe->rx_skb)) {
            EC_SLAVE_WARN(eoe->slave, "EoE RX netif_rx failed.\n");
        }
        eoe->rx_skb = NULL;

        eoe->state = ec_eoe_state_tx_start;
    }
    else {
        eoe->rx_expected_fragment++;
#if EOE_DEBUG_LEVEL >= 2
        EC_SLAVE_DBG(eoe->slave, 0, "EoE %s RX expecting fragment %u\n",
               eoe->dev->name, eoe->rx_expected_fragment);
#endif
        eoe->state = ec_eoe_state_rx_start;
    }
}

/*****************************************************************************/

/** State: TX START.
 *
 * Starts a new transmit sequence. If no data is available, a new receive
 * sequence is started instead.
 *
 * \todo Use both devices.
 */
void ec_eoe_state_tx_start(ec_eoe_t *eoe /**< EoE handler */)
{
#if EOE_DEBUG_LEVEL >= 2
    unsigned int wakeup = 0;
#endif

    if (!eoe->slave || eoe->slave->error_flag ||
            !eoe->slave->master->devices[EC_DEVICE_MAIN].link_state) {
        eoe->rx_idle = 1;
        eoe->tx_idle = 1;
        return;
    }

    if (eoe->tx_next_to_use == eoe->tx_next_to_clean) {
        // check if the queue needs to be restarted
        if (!eoe->tx_queue_active) {
            eoe->tx_queue_active = 1;
            netif_wake_queue(eoe->dev);
        }

        eoe->tx_idle = 1;
        // no data available.
        // start a new receive immediately.
        ec_eoe_state_rx_start(eoe);
        return;
    }

    // get the frame and take it out of the ring
    eoe->tx_skb = eoe->tx_ring[eoe->tx_next_to_clean];
    eoe->tx_ring[eoe->tx_next_to_clean] = NULL;
    if (unlikely(++eoe->tx_next_to_clean == eoe->tx_ring_count)) {
        eoe->tx_next_to_clean = 0;
    }

    // restart queue?
    if (!eoe->tx_queue_active &&
            (ec_eoe_tx_queued_frames(eoe) <= eoe->tx_ring_count / 2)) {
        eoe->tx_queue_active = 1;
        netif_wake_queue(eoe->dev);
#if EOE_DEBUG_LEVEL >= 2
        wakeup = 1;
#endif
    }

    eoe->tx_idle = 0;

    eoe->tx_frame_number++;
    eoe->tx_frame_number %= 16;
    eoe->tx_fragment_number = 0;
    eoe->tx_offset = 0;

    if (ec_eoe_send(eoe)) {
        dev_kfree_skb(eoe->tx_skb);
        eoe->tx_skb = NULL;
        eoe->stats.tx_errors++;
        eoe->state = ec_eoe_state_rx_start;
#if EOE_DEBUG_LEVEL >= 1
        EC_SLAVE_WARN(eoe->slave, "Send error at %s.\n", eoe->dev->name);
#endif
        return;
    }

#if EOE_DEBUG_LEVEL >= 2
    if (wakeup) {
        EC_SLAVE_DBG(eoe->slave, 0, "EoE %s waking up TX queue...\n",
                eoe->dev->name);
    }
#endif

    eoe->tries = EC_EOE_TRIES;
    eoe->state = ec_eoe_state_tx_sent;
}

/*****************************************************************************/

/** State: TX SENT.
 *
 * Checks is the previous transmit datagram succeded and sends the next
 * fragment, if necessary.
 */
void ec_eoe_state_tx_sent(ec_eoe_t *eoe /**< EoE handler */)
{
    if (eoe->datagram.state != EC_DATAGRAM_RECEIVED) {
        if (eoe->tries) {
            eoe->tries--; // try again
            eoe->queue_datagram = 1;
        } else {
#if EOE_DEBUG_LEVEL >= 1
            /* only log every 1000th */
            if (eoe->stats.tx_errors++ % 1000 == 0) {
                EC_SLAVE_WARN(eoe->slave, "Failed to receive send"
                        " datagram for %s after %u tries.\n",
                        eoe->dev->name, EC_EOE_TRIES);
            }
#else
            eoe->stats.tx_errors++;
#endif
            eoe->state = ec_eoe_state_rx_start;
        }
        return;
    }

    if (eoe->datagram.working_counter != 1) {
        if (eoe->tries) {
            eoe->tries--; // try again
            eoe->queue_datagram = 1;
        } else {
            eoe->stats.tx_errors++;
#if EOE_DEBUG_LEVEL >= 1
            EC_SLAVE_WARN(eoe->slave, "No sending response"
                    " for %s after %u tries.\n",
                    eoe->dev->name, EC_EOE_TRIES);
#endif
            eoe->state = ec_eoe_state_rx_start;
        }
        return;
    }

    // frame completely sent
    if (eoe->tx_offset >= eoe->tx_skb->len) {
        eoe->stats.tx_packets++;
        eoe->stats.tx_bytes += eoe->tx_skb->len;
        eoe->tx_counter += eoe->tx_skb->len;
        dev_kfree_skb(eoe->tx_skb);
        eoe->tx_skb = NULL;
        eoe->state = ec_eoe_state_rx_start;
    }
    else { // send next fragment
        if (ec_eoe_send(eoe)) {
            dev_kfree_skb(eoe->tx_skb);
            eoe->tx_skb = NULL;
            eoe->stats.tx_errors++;
#if EOE_DEBUG_LEVEL >= 1
            EC_SLAVE_WARN(eoe->slave, "Send error at %s.\n", eoe->dev->name);
#endif
            eoe->state = ec_eoe_state_rx_start;
        }
    }
}

/******************************************************************************
 *  NET_DEVICE functions
 *****************************************************************************/

/** Opens the virtual network device.
 *
 * \return Always zero (success).
 */
int ec_eoedev_open(struct net_device *dev /**< EoE net_device */)
{
    ec_eoe_t *eoe = *((ec_eoe_t **) netdev_priv(dev));

    // set carrier to off until we know link status
    EC_MASTER_DBG(eoe->master, 1, "%s: carrier off.\n", dev->name);
    netif_carrier_off(dev);

    ec_eoe_flush(eoe);
    eoe->opened = 1;
    eoe->rx_idle = 0;
    eoe->tx_idle = 0;
    eoe->tx_queue_active = 1;
    netif_start_queue(dev);
#if EOE_DEBUG_LEVEL >= 2
    EC_MASTER_DBG(eoe->master, 0, "%s opened.\n", dev->name);
#endif
    
    // update carrier link status
    if (eoe->slave) {
        EC_MASTER_DBG(eoe->master, 1, "%s: carrier on.\n", dev->name);
        netif_carrier_on(dev);
    }

    return 0;
}

/*****************************************************************************/

/** Stops the virtual network device.
 *
 * \return Always zero (success).
 */
int ec_eoedev_stop(struct net_device *dev /**< EoE net_device */)
{
    ec_eoe_t *eoe = *((ec_eoe_t **) netdev_priv(dev));
    
    EC_MASTER_DBG(eoe->master, 1, "%s: carrier off.\n", dev->name);
    netif_carrier_off(dev);
    netif_stop_queue(dev);
    eoe->tx_queue_active = 0;
    eoe->rx_idle = 1;
    eoe->tx_idle = 1;
    eoe->opened = 0;
    ec_eoe_flush(eoe);
#if EOE_DEBUG_LEVEL >= 2
    EC_MASTER_DBG(eoe->master, 0, "%s stopped.\n", dev->name);
#endif
    return 0;
}

/*****************************************************************************/

/** Transmits data via the virtual network device.
 *
 * \return Zero on success, non-zero on failure.
 */
int ec_eoedev_tx(struct sk_buff *skb, /**< transmit socket buffer */
                 struct net_device *dev /**< EoE net_device */
                )
{
    ec_eoe_t *eoe = *((ec_eoe_t **) netdev_priv(dev));

    if (!eoe->slave) {
        if (skb) {
            dev_kfree_skb(skb);
            eoe->stats.tx_dropped++;
        }
        
        return NETDEV_TX_OK;
    }
    
#if 0
    if (skb->len > eoe->slave->configured_tx_mailbox_size - 10) {
        EC_SLAVE_WARN(eoe->slave, "EoE TX frame (%u octets)"
                " exceeds MTU. dropping.\n", skb->len);
        dev_kfree_skb(skb);
        eoe->stats.tx_dropped++;
        return 0;
    }
#endif

    // set the skb in the ring
    eoe->tx_ring[eoe->tx_next_to_use] = skb;

    // increment index
    if (unlikely(++eoe->tx_next_to_use == eoe->tx_ring_count)) {
        eoe->tx_next_to_use = 0;
    }

    // stop the queue?
    if (eoe_tx_unused_frames(eoe) == 0) {
        netif_stop_queue(dev);
        eoe->tx_queue_active = 0;
    }

#if EOE_DEBUG_LEVEL >= 2
    EC_SLAVE_DBG(eoe->slave, 0, "EoE %s TX queued frame"
            " with %u octets (%u frames queued).\n",
            eoe->dev->name, skb->len, ec_eoe_tx_queued_frames(eoe));
    if (!eoe->tx_queue_active) {
        EC_SLAVE_WARN(eoe->slave, "EoE TX queue is now full.\n");
    }
#endif

    return 0;
}

/*****************************************************************************/

/** Gets statistics about the virtual network device.
 *
 * \return Statistics.
 */
struct net_device_stats *ec_eoedev_stats(
        struct net_device *dev /**< EoE net_device */
        )
{
    ec_eoe_t *eoe = *((ec_eoe_t **) netdev_priv(dev));
    return &eoe->stats;
}

/*****************************************************************************/
