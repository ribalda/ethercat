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
   Ethernet over EtherCAT (EoE)
*/

/*****************************************************************************/

#ifndef __EC_ETHERNET_H__
#define __EC_ETHERNET_H__

#include <linux/list.h>
#include <linux/netdevice.h>

#include "globals.h"
#include "locks.h"
#include "slave.h"
#include "datagram.h"

/*****************************************************************************/

/** EoE frame types.
 */
enum {
    EC_EOE_TYPE_FRAME_FRAG =      0x00, /** EoE Frame Fragment Tx & Rx. */
    EC_EOE_TYPE_TIMESTAMP_RES =   0x01, /** EoE Timestamp Response. */
    EC_EOE_TYPE_INIT_REQ =        0x02, /** EoE Init, Set IP Parameter Request. */
    EC_EOE_TYPE_INIT_RES =        0x03, /** EoE Init, Set IP Parameter Response. */
    EC_EOE_TYPE_MACFILTER_REQ =   0x04, /** EoE Set MAC Address Filter Request. */
    EC_EOE_TYPE_MACFILTER_RES =   0x05, /** EoE Set MAC Address Filter Response. */
};

/*****************************************************************************/

typedef struct ec_eoe ec_eoe_t; /**< \see ec_eoe */

/**
   Ethernet over EtherCAT (EoE) handler.
   The master creates one of these objects for each slave that supports the
   EoE protocol.
*/

struct ec_eoe
{
    struct list_head list; /**< list item */
    ec_master_t *master; /**< pointer to the corresponding master */
    ec_slave_t *slave; /**< pointer to the corresponding slave */
    ec_datagram_t datagram; /**< datagram */
    unsigned int queue_datagram; /**< the datagram is ready for queuing */
    void (*state)(ec_eoe_t *); /**< state function for the state machine */
    struct net_device *dev; /**< net_device for virtual ethernet device */
    struct net_device_stats stats; /**< device statistics */
    unsigned int opened; /**< net_device is opened */
    unsigned long rate_jiffies; /**< time of last rate output */
    unsigned int have_mbox_lock; /**< flag to track if we have the mbox lock */
    unsigned int auto_created; /**< auto created flag. */

    struct sk_buff *rx_skb; /**< current rx socket buffer */
    off_t rx_skb_offset; /**< current write pointer in the socket buffer */
    size_t rx_skb_size; /**< size of the allocated socket buffer memory */
    uint8_t rx_expected_fragment; /**< next expected fragment number */
    uint32_t rx_counter; /**< octets received during last second */
    uint32_t rx_rate; /**< receive rate (bps) */
    unsigned int rx_idle; /**< Idle flag. */

    struct sk_buff **tx_ring; /**< ring for frames to send */
    unsigned int tx_ring_count; /**< Transmit ring count. */
    unsigned int tx_ring_size; /**< Transmit ring size. */
    unsigned int tx_next_to_use; /**< index of frames added to the ring */
    unsigned int tx_next_to_clean; /**< index of frames being used from the ring */
    unsigned int tx_queue_active; /**< kernel netif queue started */
    struct sk_buff *tx_skb; /**< current TX frame */
    uint8_t tx_frame_number; /**< number of the transmitted frame */
    uint8_t tx_fragment_number; /**< number of the fragment */
    size_t tx_offset; /**< number of octets sent */
    uint32_t tx_counter; /**< octets transmitted during last second */
    uint32_t tx_rate; /**< transmit rate (bps) */
    unsigned int tx_idle; /**< Idle flag. */

    unsigned int tries; /**< Tries. */
};

/*****************************************************************************/

int ec_eoe_parse(const char *, int *, uint16_t *, uint16_t *);

int ec_eoe_init(ec_master_t *, ec_eoe_t *, uint16_t/*alias*/, uint16_t/*posn*/);
int ec_eoe_auto_init(ec_eoe_t *, ec_slave_t *);
void ec_eoe_link_slave(ec_eoe_t *, ec_slave_t *);
void ec_eoe_clear_slave(ec_eoe_t *);
void ec_eoe_clear(ec_eoe_t *);
void ec_eoe_run(ec_eoe_t *);
void ec_eoe_queue(ec_eoe_t *);
int ec_eoe_is_open(const ec_eoe_t *);
int ec_eoe_is_idle(const ec_eoe_t *);
char *ec_eoe_name(const ec_eoe_t *);
unsigned int ec_eoe_tx_queued_frames(const ec_eoe_t *);

/*****************************************************************************/

#endif

/*****************************************************************************/
