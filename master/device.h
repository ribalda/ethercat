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
   EtherCAT device structure.
*/

/*****************************************************************************/

#ifndef _EC_DEVICE_H_
#define _EC_DEVICE_H_

#include <linux/interrupt.h>

#include "../include/ecrt.h"
#include "../devices/ecdev.h"
#include "globals.h"

#ifdef EC_DEBUG_IF
#include "debug.h"
#endif

#ifdef EC_DEBUG_RING
#define EC_DEBUG_RING_SIZE 10

typedef enum {
    TX, RX
} ec_debug_frame_dir_t;

typedef struct {
    ec_debug_frame_dir_t dir;
    struct timeval t;
    unsigned int addr;
    uint8_t data[EC_MAX_DATA_SIZE];
    unsigned int data_size;
} ec_debug_frame_t;

#endif

/*****************************************************************************/

/**
   EtherCAT device.
   An EtherCAT device is a network interface card, that is owned by an
   EtherCAT master to send and receive EtherCAT frames with.
*/

struct ec_device
{
    ec_master_t *master; /**< EtherCAT master */
    struct net_device *dev; /**< pointer to the assigned net_device */
    ec_pollfunc_t poll; /**< pointer to the device's poll function */
    struct module *module; /**< pointer to the device's owning module */
    uint8_t open; /**< true, if the net_device has been opened */
    uint8_t link_state; /**< device link state */
    struct sk_buff *tx_skb; /**< transmit socket buffer */
    struct ethhdr *eth; /**< pointer to ethernet header in socket buffer */
    cycles_t cycles_poll; /**< cycles of last poll */
#ifdef EC_DEBUG_RING
    struct timeval timeval_poll;
#endif
    unsigned long jiffies_poll; /**< jiffies of last poll */
    unsigned int tx_count; /**< number of frames sent */
    unsigned int rx_count; /**< number of frames received */
#ifdef EC_DEBUG_IF
    ec_debug_t dbg; /**< debug device */
#endif
#ifdef EC_DEBUG_RING
    ec_debug_frame_t debug_frames[EC_DEBUG_RING_SIZE];
    unsigned int debug_frame_index;
    unsigned int debug_frame_count;
#endif
};

/*****************************************************************************/

int ec_device_init(ec_device_t *, ec_master_t *);
void ec_device_clear(ec_device_t *);

void ec_device_attach(ec_device_t *, struct net_device *, ec_pollfunc_t,
        struct module *);
void ec_device_detach(ec_device_t *);

int ec_device_open(ec_device_t *);
int ec_device_close(ec_device_t *);

void ec_device_poll(ec_device_t *);
uint8_t *ec_device_tx_data(ec_device_t *);
void ec_device_send(ec_device_t *, size_t);

#ifdef EC_DEBUG_RING
void ec_device_debug_ring_append(ec_device_t *, ec_debug_frame_dir_t,
        const void *, size_t);
void ec_device_debug_ring_print(const ec_device_t *);
#endif

/*****************************************************************************/

#endif
