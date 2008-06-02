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
   EtherCAT Process data object structure.
*/

/*****************************************************************************/

#ifndef __EC_PDO_H__
#define __EC_PDO_H__

#include <linux/list.h>

#include "../include/ecrt.h"

#include "globals.h"
#include "pdo_entry.h"

/*****************************************************************************/

/** Pdo description.
 */
typedef struct {
    struct list_head list; /**< List item. */
    ec_direction_t dir; /**< Pdo direction. */
    uint16_t index; /**< Pdo index. */
    int8_t sync_index; /**< Assigned sync manager. */
    char *name; /**< Pdo name. */
    struct list_head entries; /**< List of Pdo entries. */
    unsigned int default_config; /**< The entries contain the default Pdo
                                   configuration. */
} ec_pdo_t;

/*****************************************************************************/

void ec_pdo_init(ec_pdo_t *);
int ec_pdo_init_copy(ec_pdo_t *, const ec_pdo_t *);
void ec_pdo_clear(ec_pdo_t *);
void ec_pdo_clear_entries(ec_pdo_t *);
int ec_pdo_set_name(ec_pdo_t *, const char *);
ec_pdo_entry_t *ec_pdo_add_entry(ec_pdo_t *, uint16_t, uint8_t, uint8_t);
int ec_pdo_copy_entries(ec_pdo_t *, const ec_pdo_t *);
int ec_pdo_equal_entries(const ec_pdo_t *, const ec_pdo_t *);
unsigned int ec_pdo_entry_count(const ec_pdo_t *);
const ec_pdo_entry_t *ec_pdo_find_entry_by_pos_const(
        const ec_pdo_t *, unsigned int);

/*****************************************************************************/

#endif
