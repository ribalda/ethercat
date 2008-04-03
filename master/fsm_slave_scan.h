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
   EtherCAT slave scanning state machine.
*/

/*****************************************************************************/

#ifndef __EC_FSM_SLAVE_SCAN_H__
#define __EC_FSM_SLAVE_SCAN_H__

#include "../include/ecrt.h"

#include "globals.h"
#include "datagram.h"
#include "slave.h"
#include "fsm_sii.h"
#include "fsm_change.h"
#include "fsm_coe.h"
#include "fsm_coe_map.h"

/*****************************************************************************/

/** \see ec_fsm_slave_scan */
typedef struct ec_fsm_slave_scan ec_fsm_slave_scan_t;

/** Finite state machine for scanning an EtherCAT slave.
 */
struct ec_fsm_slave_scan
{
    ec_slave_t *slave; /**< Slave the FSM runs on. */
    ec_datagram_t *datagram; /**< Datagram used in the state machine. */
    ec_fsm_slave_config_t *fsm_slave_config; /**< Slave configuration state
                                               machine to use. */
    ec_fsm_coe_map_t *fsm_coe_map; /**< Pdo mapping state machine to use. */
    unsigned int retries; /**< Retries on datagram timeout. */

    void (*state)(ec_fsm_slave_scan_t *); /**< State function. */
    uint16_t sii_offset; /**< SII offset in words. */

    ec_fsm_sii_t fsm_sii; /**< SII state machine. */
};

/*****************************************************************************/

void ec_fsm_slave_scan_init(ec_fsm_slave_scan_t *, ec_datagram_t *,
        ec_fsm_slave_config_t *, ec_fsm_coe_map_t *);
void ec_fsm_slave_scan_clear(ec_fsm_slave_scan_t *);

void ec_fsm_slave_scan_start(ec_fsm_slave_scan_t *, ec_slave_t *);

int ec_fsm_slave_scan_exec(ec_fsm_slave_scan_t *);
int ec_fsm_slave_scan_success(const ec_fsm_slave_scan_t *);

/*****************************************************************************/

#endif
