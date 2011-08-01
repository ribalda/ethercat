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
   Mailbox functionality.
*/

/*****************************************************************************/

#ifndef __EC_MAILBOX_H__
#define __EC_MAILBOX_H__

#include "globals.h"
#include "datagram.h"

/*****************************************************************************/

/** Size of the mailbox header.
 */
#define EC_MBOX_HEADER_SIZE 6

/*****************************************************************************/

/** EtherCAT slave mailbox.
 */
struct ec_mailbox
{
    ec_datagram_t *datagram; /**< Datagram used for the mailbox content. */
#ifdef EC_REDUCE_MBOXFRAMESIZE
    ec_datagram_t end_datagram; /**< Datagram used for writing the end byte to
                                  the mailbox. */
#endif
};
typedef struct ec_mailbox ec_mailbox_t; /**< \see ec_mailbox. */

/*****************************************************************************/

void ec_mbox_init(ec_mailbox_t *, ec_datagram_t *);
void ec_mbox_clear(ec_mailbox_t *);

/**
   Checks the datagrams states.
*/
static inline int ec_mbox_is_datagram_state(
        ec_mailbox_t *mbox,
        ec_datagram_state_t state
        )
{
    return (mbox->datagram->state == state)
#ifdef EC_REDUCE_MBOXFRAMESIZE
         && (mbox->end_datagram.type == EC_DATAGRAM_NONE
                 || mbox->end_datagram.state == state)
#endif
         ;
}

/**
   Checks the datagrams working counter.
*/
static inline int ec_mbox_is_datagram_wc(
        ec_mailbox_t *mbox,
        size_t wc
        )
{
    return (mbox->datagram->working_counter == wc)
#ifdef EC_REDUCE_MBOXFRAMESIZE
         && (mbox->end_datagram.type == EC_DATAGRAM_NONE
                 || mbox->end_datagram.working_counter == wc)
#endif
         ;
}

void ec_slave_mbox_queue_datagrams(const ec_slave_t *, ec_mailbox_t *);
void ec_master_mbox_queue_datagrams(ec_master_t *, ec_mailbox_t *);
uint8_t *ec_slave_mbox_prepare_send(const ec_slave_t *, ec_mailbox_t *,
                                    uint8_t, size_t);
int ec_slave_mbox_prepare_check(const ec_slave_t *, ec_mailbox_t *);
int ec_slave_mbox_check(ec_mailbox_t *);
int ec_slave_mbox_prepare_fetch(const ec_slave_t *, ec_mailbox_t *);
uint8_t *ec_slave_mbox_fetch(const ec_slave_t *, ec_mailbox_t *,
                             uint8_t *, size_t *);

/*****************************************************************************/

#endif
