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
   EtherCAT CoE mapping state machines.
*/

/*****************************************************************************/

#ifndef __EC_FSM_COE_MAP__
#define __EC_FSM_COE_MAP__

#include "globals.h"
#include "datagram.h"
#include "slave.h"
#include "fsm_coe.h"

/*****************************************************************************/

typedef struct ec_fsm_coe_map ec_fsm_coe_map_t; /**< \see ec_fsm_coe_map */

/**
 * \todo doc
 */
struct ec_fsm_coe_map
{
    void (*state)(ec_fsm_coe_map_t *); /**< CoE mapping state function */
    ec_fsm_coe_t *fsm_coe; /**< CoE state machine to use */

    ec_slave_t *slave; /**< EtherCAT slave */
    ec_sdo_request_t request; /**< Sdo request */

    unsigned int sync_index; /**< index of the current sync manager */
    ec_sdo_t *sync_sdo; /**< pointer to the sync managers mapping Sdo */
    uint8_t sync_subindices; /**< number of mapped Pdos */
    uint16_t sync_subindex; /**< current subindex in mapping Sdo */

    ec_pdo_mapping_t mapping; /**< Mapping to apply. */
    ec_pdo_t *pdo; /**< current Pdo */
    ec_sdo_t *pdo_sdo; /**< current Pdo Sdo */
    uint8_t pdo_subindices; /**< number of Pdo entries */
    uint16_t pdo_subindex; /**< current subindex in Pdo Sdo */
};

/*****************************************************************************/

void ec_fsm_coe_map_init(ec_fsm_coe_map_t *, ec_fsm_coe_t *);
void ec_fsm_coe_map_clear(ec_fsm_coe_map_t *);

void ec_fsm_coe_map_start(ec_fsm_coe_map_t *, ec_slave_t *);

int ec_fsm_coe_map_exec(ec_fsm_coe_map_t *);
int ec_fsm_coe_map_success(ec_fsm_coe_map_t *);

/*****************************************************************************/

#endif
