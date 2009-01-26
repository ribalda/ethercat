/******************************************************************************
 *
 *  $Id:$
 *
 *  Copyright (C) 2008  Olav Zarges, imc Meﬂsysteme GmbH
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
   EtherCAT FoE state machines.
*/

/*****************************************************************************/

#ifndef __EC_FSM_FOE_H__
#define __EC_FSM_FOE_H__

#include "globals.h"
#include "../include/ecrt.h"
#include "datagram.h"
#include "slave.h"
#include "foe_request.h"

/*****************************************************************************/

typedef struct ec_fsm_foe ec_fsm_foe_t; /**< \see ec_fsm_foe */

/** Finite state machines for the CANopen-over-EtherCAT protocol.
 */
struct ec_fsm_foe {
    ec_slave_t *slave; /**< slave the FSM runs on */
    ec_datagram_t *datagram; /**< datagram used in the state machine */
    unsigned int retries; /**< retries upon datagram timeout */

    void (*state)(ec_fsm_foe_t *); /**< FoE state function */
    unsigned long jiffies_start; /**< FoE timestamp. */
    uint8_t subindex; /**< current subindex */
    ec_foe_request_t *request; /**< FoE request */
    uint8_t toggle; /**< toggle bit for segment commands */

    uint32_t tx_errors;
    uint8_t *tx_buffer;
    uint32_t tx_buffer_size;
    uint32_t tx_buffer_offset;
    uint32_t tx_last_packet;
    uint32_t tx_packet_no;
    uint32_t tx_current_size;
    uint8_t *tx_filename;
    uint32_t tx_filename_len;


    uint32_t rx_errors;
    uint8_t *rx_buffer;
    uint32_t rx_buffer_size;
    uint32_t rx_buffer_offset;
    uint32_t rx_current_size;
    uint32_t rx_packet_no;
    uint32_t rx_expected_packet_no;
    uint32_t rx_last_packet;
    uint8_t *rx_filename;
    uint32_t rx_filename_len;
};

/*****************************************************************************/

void ec_fsm_foe_init(ec_fsm_foe_t *, ec_datagram_t *);
void ec_fsm_foe_clear(ec_fsm_foe_t *);

int ec_fsm_foe_exec(ec_fsm_foe_t *);
int ec_fsm_foe_success(ec_fsm_foe_t *);

void ec_fsm_foe_transfer(ec_fsm_foe_t *, ec_slave_t *, ec_foe_request_t *);

/*****************************************************************************/

#endif
