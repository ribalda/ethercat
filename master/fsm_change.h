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
   EtherCAT state change FSM.
*/

/*****************************************************************************/

#ifndef __EC_FSM_CHANGE_H__
#define __EC_FSM_CHANGE_H__

#include "globals.h"
#include "../include/ecrt.h"
#include "datagram.h"
#include "slave.h"

/*****************************************************************************/

/**
   Mode of the change state machine.
*/

typedef enum {
    EC_FSM_CHANGE_MODE_FULL, /**< full state change */
    EC_FSM_CHANGE_MODE_ACK_ONLY /**< only state acknowledgement */
}
ec_fsm_change_mode_t;

/*****************************************************************************/

typedef struct ec_fsm_change ec_fsm_change_t; /**< \see ec_fsm_change */

/**
   EtherCAT state change FSM.
*/

struct ec_fsm_change
{
    ec_slave_t *slave; /**< slave the FSM runs on */
    ec_datagram_t *datagram; /**< datagram used in the state machine */
    unsigned int retries; /**< retries upon datagram timeout */

    void (*state)(ec_fsm_change_t *); /**< slave state change state function */
    ec_fsm_change_mode_t mode; /**< full state change, or ack only. */
    ec_slave_state_t requested_state; /**< input: state */
    ec_slave_state_t old_state; /**< prior slave state */
    unsigned long jiffies_start; /**< change timer */
    uint8_t take_time; /**< take sending timestamp */
    uint8_t spontaneous_change; /**< spontaneous state change detected */
};

/*****************************************************************************/

void ec_fsm_change_init(ec_fsm_change_t *, ec_datagram_t *);
void ec_fsm_change_clear(ec_fsm_change_t *);

void ec_fsm_change_start(ec_fsm_change_t *, ec_slave_t *, ec_slave_state_t);
void ec_fsm_change_ack(ec_fsm_change_t *, ec_slave_t *);

int ec_fsm_change_exec(ec_fsm_change_t *);
int ec_fsm_change_success(ec_fsm_change_t *);

/*****************************************************************************/

#endif
