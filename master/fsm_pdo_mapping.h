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

/** \file
 * EtherCAT Pdo configuration state machine structures.
 */

/*****************************************************************************/

#ifndef __EC_FSM_PDO_MAPPING_H__
#define __EC_FSM_PDO_MAPPING_H__

#include "globals.h"
#include "../include/ecrt.h"
#include "datagram.h"
#include "fsm_coe.h"

/*****************************************************************************/

/**
 * \see ec_fsm_pdo_mapping
 */
typedef struct ec_fsm_pdo_mapping ec_fsm_pdo_mapping_t;

/** Pdo configuration state machine.
 */
struct ec_fsm_pdo_mapping
{
    void (*state)(ec_fsm_pdo_mapping_t *); /**< state function */
    ec_fsm_coe_t *fsm_coe; /**< CoE state machine to use */
    ec_slave_t *slave; /**< Slave the FSM runs on. */

    const ec_pdo_t *pdo; /**< Current Pdo to configure. */
    const ec_pdo_entry_t *entry; /**< Current entry. */

    ec_sdo_request_t request; /**< Sdo request. */
    unsigned int entry_count; /**< Number of configured entries. */
};

/*****************************************************************************/

void ec_fsm_pdo_mapping_init(ec_fsm_pdo_mapping_t *, ec_fsm_coe_t *);
void ec_fsm_pdo_mapping_clear(ec_fsm_pdo_mapping_t *);

void ec_fsm_pdo_mapping_start(ec_fsm_pdo_mapping_t *, ec_slave_t *);
int ec_fsm_pdo_mapping_exec(ec_fsm_pdo_mapping_t *);
int ec_fsm_pdo_mapping_success(const ec_fsm_pdo_mapping_t *);

/*****************************************************************************/

#endif
