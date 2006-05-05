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
   Ethernet-over-EtherCAT (EoE)
*/

/*****************************************************************************/

#include <linux/list.h>
#include <linux/netdevice.h>

#include "../include/ecrt.h"
#include "globals.h"
#include "slave.h"
#include "command.h"

/*****************************************************************************/

/**
   State of an EoE object.
*/

typedef enum
{
    EC_EOE_RX_START, /**< start receiving and check for data. */
    EC_EOE_RX_CHECK, /**< checking frame was sent. */
    EC_EOE_RX_FETCH, /**< there is new data; fetching frame was sent. */
    EC_EOE_TX_START, /**< start sending a queued frame. */
    EC_EOE_TX_SENT,  /**< queued frame was sent; start checking. */
    EC_EOE_TX_CHECK, /**< check mailbox for acknowledgement. */
    EC_EOE_TX_FETCH, /**< receive mailbox response */
}
ec_eoe_state_t;

/*****************************************************************************/

/**
   Ethernet-over-EtherCAT (EoE) Object.
   The master creates one of these objects for each slave that supports the
   EoE protocol.
*/

typedef struct
{
    struct list_head list; /**< list item */
    ec_slave_t *slave; /**< pointer to the corresponding slave */
    ec_eoe_state_t state; /**< state of the state machine */
    struct net_device *dev; /**< net_device for virtual ethernet device */
    uint8_t opened; /**< net_device is opened */
    struct sk_buff *skb; /**< current rx socket buffer */
    off_t skb_offset; /**< current write pointer in the socket buffer */
    size_t skb_size; /**< size of the allocated socket buffer memory */
    uint8_t expected_fragment; /**< expected fragment */
    struct net_device_stats stats; /**< device statistics */
    struct list_head tx_queue; /**< queue for frames to send */
    unsigned int tx_queue_active; /**< kernel netif queue started */
    unsigned int queued_frames; /**< number of frames in the queue */
    spinlock_t tx_queue_lock; /**< spinlock for the send queue */
    uint8_t tx_frame_number; /**< Number of the transmitted frame */
    size_t last_tx_bytes; /**< number of bytes currently transmitted */
}
ec_eoe_t;

/*****************************************************************************/

int ec_eoe_init(ec_eoe_t *, ec_slave_t *);
void ec_eoe_clear(ec_eoe_t *);
void ec_eoe_run(ec_eoe_t *);
void ec_eoe_print(const ec_eoe_t *);

/*****************************************************************************/
