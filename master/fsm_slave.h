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
   EtherCAT slave request (SDO) state machine.
*/

/*****************************************************************************/
#ifndef __EC_FSM_SLAVE_H__
#define __EC_FSM_SLAVE_H__

#include "globals.h"
#include "datagram.h"
#include "sdo_request.h"
#include "fsm_coe.h"
#include "fsm_foe.h"

typedef struct ec_fsm_slave ec_fsm_slave_t; /**< \see ec_fsm_slave */

/** Finite state machine of an EtherCAT slave.
 */
struct ec_fsm_slave {
    ec_slave_t *slave; /**< slave the FSM runs on */
    ec_datagram_t *datagram; /**< datagram used in the state machine */

    void (*state)(ec_fsm_slave_t *); /**< master state function */
    ec_sdo_request_t *sdo_request; /**< SDO request to process. */
    ec_foe_request_t *foe_request; /**< FoE request to process. */
    off_t foe_index; /**< index to FoE write request data */

    ec_fsm_coe_t fsm_coe; /**< CoE state machine */
    ec_fsm_foe_t fsm_foe; /**< FoE state machine */
};

/*****************************************************************************/

void ec_fsm_slave_init(ec_fsm_slave_t *, ec_slave_t *, ec_datagram_t *);
void ec_fsm_slave_clear(ec_fsm_slave_t *);

void ec_fsm_slave_exec(ec_fsm_slave_t *);
int ec_fsm_slave_idle(const ec_fsm_slave_t *);

/*****************************************************************************/


#endif // __EC_FSM_SLAVE_H__
