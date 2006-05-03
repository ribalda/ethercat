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
    EC_EOE_IDLE,     /**< Idle. The next step ist to check for data. */
    EC_EOE_CHECKING, /**< Checking frame was sent. */
    EC_EOE_FETCHING  /**< There is new data. Fetching frame was sent. */
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
    ec_eoe_state_t rx_state; /**< state of the state machine */
    struct net_device *dev; /**< net_device for virtual ethernet device */
    uint8_t opened; /**< net_device is opened */
    struct sk_buff *skb; /**< current rx socket buffer */
    struct net_device_stats stats; /**< device statistics */
    struct list_head tx_queue; /**< queue for frames to send */
    unsigned int queued_frames; /**< number of frames in the queue */
    spinlock_t tx_queue_lock; /**< spinlock for the send queue */
}
ec_eoe_t;

/*****************************************************************************/

int ec_eoe_init(ec_eoe_t *, ec_slave_t *);
void ec_eoe_clear(ec_eoe_t *);
void ec_eoe_run(ec_eoe_t *);
void ec_eoe_print(const ec_eoe_t *);

/*****************************************************************************/
