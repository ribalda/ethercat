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
   EtherCAT finite state machines.
*/

/*****************************************************************************/

#ifndef __EC_STATES__
#define __EC_STATES__

#include "globals.h"
#include "../include/ecrt.h"
#include "datagram.h"
#include "slave.h"

/*****************************************************************************/

typedef struct ec_fsm ec_fsm_t; /**< \see ec_fsm */

/**
   Finite state machine of an EtherCAT master.
*/

struct ec_fsm
{
    ec_master_t *master; /**< master the FSM runs on */
    ec_slave_t *slave; /**< slave the FSM runs on */
    ec_datagram_t datagram; /**< datagram used in the state machine */

    void (*master_state)(ec_fsm_t *); /**< master state function */
    unsigned int master_slaves_responding; /**< number of responding slaves */
    ec_slave_state_t master_slave_states; /**< states of responding slaves */
    unsigned int master_validation; /**< non-zero, if validation to do */

    void (*slave_state)(ec_fsm_t *); /**< slave state function */

    void (*sii_state)(ec_fsm_t *); /**< SII state function */
    uint16_t sii_offset; /**< input: offset in SII */
    unsigned int sii_mode; /**< SII reading done by APRD (0) or NPRD (1) */
    uint8_t sii_value[4]; /**< raw SII value (32bit) */
    cycles_t sii_start; /**< sii start */

    void (*change_state)(ec_fsm_t *); /**< slave state change state function */
    ec_slave_state_t change_new; /**< input: new state */
    cycles_t change_start; /**< change start */

    void (*coe_state)(ec_fsm_t *); /**< CoE state function */
    ec_sdo_data_t *sdodata; /**< input/output: SDO data object */
    cycles_t coe_start; /**< CoE timestamp */
};

/*****************************************************************************/

int ec_fsm_init(ec_fsm_t *, ec_master_t *);
void ec_fsm_clear(ec_fsm_t *);
void ec_fsm_reset(ec_fsm_t *);
void ec_fsm_execute(ec_fsm_t *);

void ec_fsm_startup(ec_fsm_t *);
int ec_fsm_startup_running(ec_fsm_t *);
int ec_fsm_startup_success(ec_fsm_t *);

void ec_fsm_configuration(ec_fsm_t *);
int ec_fsm_configuration_running(ec_fsm_t *);
int ec_fsm_configuration_success(ec_fsm_t *);

/*****************************************************************************/

#endif
