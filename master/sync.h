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
 * EtherCAT sync manager.
 */

/*****************************************************************************/

#ifndef __EC_SYNC_H__
#define __EC_SYNC_H__

#include "../include/ecrt.h"

#include "globals.h"
#include "pdo_list.h"

/*****************************************************************************/

/** Sync manager.
 */
typedef struct {
    ec_slave_t *slave; /**< Slave, the sync manager belongs to. */
    uint16_t physical_start_address; /**< Physical start address. */
    uint16_t default_length; /**< Data length in bytes. */
    uint8_t control_register; /**< Control register value. */
    uint8_t enable; /**< Enable bit. */
    ec_pdo_list_t pdos; /**< Current Pdo assignment. */
} ec_sync_t;

/*****************************************************************************/

void ec_sync_init(ec_sync_t *, ec_slave_t *);
void ec_sync_init_copy(ec_sync_t *, const ec_sync_t *);
void ec_sync_clear(ec_sync_t *);
void ec_sync_page(const ec_sync_t *, uint8_t, uint16_t, ec_direction_t,
        uint8_t *);
int ec_sync_add_pdo(ec_sync_t *, const ec_pdo_t *);
ec_direction_t ec_sync_default_direction(const ec_sync_t *);

/*****************************************************************************/

#endif
