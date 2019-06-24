/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2019  Florian Pose, Ingenieurgemeinschaft IgH
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
   EtherCAT Mailbox Gateway state machine.
   Note: message fragmentation (segmentation) not supported
*/

/*****************************************************************************/

#ifndef __EC_FSM_MBG_H__
#define __EC_FSM_MBG_H__

#include "globals.h"
#include "datagram.h"
#include "slave.h"
#include "mbox_gateway_request.h"

/*****************************************************************************/

typedef struct ec_fsm_mbg ec_fsm_mbg_t; /**< \see ec_fsm_mbg */

/** Finite state machines for the CANopen over EtherCAT protocol.
 */
struct ec_fsm_mbg {
    ec_slave_t *slave; /**< slave the FSM runs on */
    unsigned int retries; /**< retries upon datagram timeout */

    void (*state)(ec_fsm_mbg_t *, ec_datagram_t *); /**< mbox state function */
    ec_datagram_t *datagram; /**< Datagram used in last step. */
    unsigned long jiffies_start; /**< MBox Gateway timestamp. */
    ec_mbg_request_t *request; /**< MBox Gateway request */
    uint8_t mbox_type; /**< MBox Gateway header type */
};

/*****************************************************************************/

void ec_fsm_mbg_init(ec_fsm_mbg_t *);
void ec_fsm_mbg_clear(ec_fsm_mbg_t *);

void ec_fsm_mbg_transfer(ec_fsm_mbg_t *, ec_slave_t *, ec_mbg_request_t *);

int ec_fsm_mbg_exec(ec_fsm_mbg_t *, ec_datagram_t *);
int ec_fsm_mbg_success(const ec_fsm_mbg_t *);

/*****************************************************************************/

#endif
