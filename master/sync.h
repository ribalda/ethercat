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

#ifndef _EC_SYNC_H_
#define _EC_SYNC_H_

#include <linux/list.h>

#include "../include/ecrt.h"

#include "globals.h"
#include "pdo_mapping.h"

/*****************************************************************************/

/** EtherCAT sync manager PDO mapping information source.
 */
typedef enum {
    EC_SYNC_MAPPING_NONE, /**< No PDO mapping information. */
    EC_SYNC_MAPPING_SII, /**< PDO mapping information from SII. */
    EC_SYNC_MAPPING_COE, /**< PDO mapping information from CoE dictionary. */
    EC_SYNC_MAPPING_CUSTOM, /**< PDO mapping configured externally. */
} ec_sync_mapping_source_t;

/*****************************************************************************/

/** Sync manager.
 */
typedef struct {
    ec_slave_t *slave; /**< Slave, the sync manager belongs to. */
    unsigned int index; /**< Sync manager index. */
    uint16_t physical_start_address; /**< Physical start address. */
    uint16_t length; /**< Data length in bytes. */
    uint8_t control_register; /**< Control register value. */
    uint8_t enable; /**< Enable bit. */
    ec_pdo_mapping_t mapping; /**< Current Pdo mapping. */
    ec_sync_mapping_source_t mapping_source; /**< Pdo mapping source. */
} ec_sync_t;

/*****************************************************************************/

void ec_sync_init(ec_sync_t *, ec_slave_t *, unsigned int);
void ec_sync_clear(ec_sync_t *);

void ec_sync_config(const ec_sync_t *, uint16_t, uint8_t *);

int ec_sync_add_pdo(ec_sync_t *, const ec_pdo_t *);

ec_direction_t ec_sync_direction(const ec_sync_t *);

/*****************************************************************************/

#endif
