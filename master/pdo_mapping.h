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
   EtherCAT Pdo mapping structure.
*/

/*****************************************************************************/

#ifndef _EC_PDO_MAPPING_H_
#define _EC_PDO_MAPPING_H_

#include <linux/list.h>

#include "../include/ecrt.h"

#include "globals.h"

/*****************************************************************************/

/** EtherCAT PDO mapping.
 */
typedef struct {
    struct list_head pdos; /**< List of Pdos. */
    unsigned int default_mapping; /**< This is the default mapping. */
} ec_pdo_mapping_t;

/*****************************************************************************/

void ec_pdo_mapping_init(ec_pdo_mapping_t *);
void ec_pdo_mapping_clear(ec_pdo_mapping_t *);

void ec_pdo_mapping_clear_pdos(ec_pdo_mapping_t *);

int ec_pdo_mapping_add_pdo(ec_pdo_mapping_t *, const ec_pdo_t *);
int ec_pdo_mapping_add_pdo_info(ec_pdo_mapping_t *, const ec_pdo_info_t *,
        const ec_slave_config_t *);

int ec_pdo_mapping_copy(ec_pdo_mapping_t *, const ec_pdo_mapping_t *);

uint16_t ec_pdo_mapping_total_size(const ec_pdo_mapping_t *);
int ec_pdo_mapping_equal(const ec_pdo_mapping_t *, const ec_pdo_mapping_t *);

const ec_pdo_t *ec_pdo_mapping_find_pdo(const ec_pdo_mapping_t *, uint16_t);

/*****************************************************************************/

#endif
