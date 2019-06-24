/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2012  Florian Pose, Ingenieurgemeinschaft IgH
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
   EtherCAT slave request state machine.
*/

/*****************************************************************************/

#ifndef __EC_FSM_SLAVE_H__
#define __EC_FSM_SLAVE_H__

#include "globals.h"
#include "datagram.h"
#include "sdo_request.h"
#include "reg_request.h"
#include "eoe_request.h"
#include "mbox_gateway_request.h"
#include "dict_request.h"
#include "fsm_coe.h"
#include "fsm_foe.h"
#include "fsm_soe.h"
#ifdef EC_EOE
#include "fsm_eoe.h"
#endif
#include "fsm_mbox_gateway.h"
#include "fsm_slave_config.h"
#include "fsm_slave_scan.h"

/*****************************************************************************/

typedef struct ec_fsm_slave ec_fsm_slave_t; /**< \see ec_fsm_slave */

/** Finite state machine of an EtherCAT slave.
 */
struct ec_fsm_slave {
    ec_slave_t *slave; /**< slave the FSM runs on */
    struct list_head list; /**< Used for execution list. */
    ec_dict_request_t int_dict_request; /**< Internal dictionary request. */

    void (*state)(ec_fsm_slave_t *, ec_datagram_t *); /**< State function. */
    ec_datagram_t *datagram; /**< Previous state datagram. */
    ec_sdo_request_t *sdo_request; /**< SDO request to process. */
    ec_reg_request_t *reg_request; /**< Register request to process. */
    ec_foe_request_t *foe_request; /**< FoE request to process. */
    off_t foe_index; /**< Index to FoE write request data. */
    ec_soe_request_t *soe_request; /**< SoE request to process. */
#ifdef EC_EOE
    ec_eoe_request_t *eoe_request; /**< EoE request to process. */
#endif
    ec_mbg_request_t *mbg_request; /**< MBox Gateway request to process. */
    ec_dict_request_t *dict_request; /**< Dictionary request to process. */

    ec_fsm_coe_t fsm_coe; /**< CoE state machine. */
    ec_fsm_foe_t fsm_foe; /**< FoE state machine. */
    ec_fsm_soe_t fsm_soe; /**< SoE state machine. */
#ifdef EC_EOE
    ec_fsm_eoe_t fsm_eoe; /**< EoE state machine. */
#endif
    ec_fsm_mbg_t fsm_mbg; /**< MBox Gateway state machine. */
    ec_fsm_pdo_t fsm_pdo; /**< PDO configuration state machine. */
    ec_fsm_change_t fsm_change; /**< State change state machine */
    ec_fsm_slave_scan_t fsm_slave_scan; /**< slave scan state machine */
    ec_fsm_slave_config_t fsm_slave_config; /**< slave config state machine. */
};

/*****************************************************************************/

void ec_fsm_slave_init(ec_fsm_slave_t *, ec_slave_t *);
void ec_fsm_slave_clear(ec_fsm_slave_t *);

int ec_fsm_slave_exec(ec_fsm_slave_t *, ec_datagram_t *);
void ec_fsm_slave_set_ready(ec_fsm_slave_t *);
int ec_fsm_slave_set_unready(ec_fsm_slave_t *);
int ec_fsm_slave_is_ready(const ec_fsm_slave_t *);

/*****************************************************************************/


#endif // __EC_FSM_SLAVE_H__
