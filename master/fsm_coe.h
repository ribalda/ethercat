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
   EtherCAT CoE state machines.
*/

/*****************************************************************************/

#ifndef __EC_FSM_COE__
#define __EC_FSM_COE__

#include "globals.h"
#include "../include/ecrt.h"
#include "datagram.h"
#include "slave.h"
#include "canopen.h"

/*****************************************************************************/

typedef struct ec_fsm_coe ec_fsm_coe_t; /**< \see ec_fsm_coe */

/**
   Finite state machine of an EtherCAT master.
*/

struct ec_fsm_coe
{
    ec_slave_t *slave; /**< slave the FSM runs on */
    ec_datagram_t *datagram; /**< datagram used in the state machine */
    unsigned int retries; /**< retries upon datagram timeout */

    void (*state)(ec_fsm_coe_t *); /**< CoE state function */
    ec_sdo_data_t *sdodata; /**< input/output: SDO data object */
    cycles_t cycles_start; /**< CoE timestamp */
    ec_sdo_t *sdo; /**< current SDO */
    uint8_t subindex; /**< current subindex */
    ec_sdo_request_t *request; /**< SDO request */
    uint8_t toggle; /**< toggle bit for segment commands */
};

/*****************************************************************************/

void ec_fsm_coe_init(ec_fsm_coe_t *, ec_datagram_t *);
void ec_fsm_coe_clear(ec_fsm_coe_t *);

void ec_fsm_coe_dictionary(ec_fsm_coe_t *, ec_slave_t *);
void ec_fsm_coe_download(ec_fsm_coe_t *, ec_slave_t *, ec_sdo_data_t *);
void ec_fsm_coe_upload(ec_fsm_coe_t *, ec_slave_t *, ec_sdo_request_t *);

int ec_fsm_coe_exec(ec_fsm_coe_t *);
int ec_fsm_coe_success(ec_fsm_coe_t *);

/*****************************************************************************/

#endif
