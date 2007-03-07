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

#ifndef _EC_PDO_H_
#define _EC_PDO_H_

#include <linux/list.h>

#include "globals.h"

/*****************************************************************************/

/**
 * PDO type.
 */

typedef enum
{
    EC_RX_PDO, /**< Reveive PDO */
    EC_TX_PDO /**< Transmit PDO */
}
ec_pdo_type_t;

/*****************************************************************************/

/**
 * PDO description.
 */

typedef struct
{
    struct list_head list; /**< list item */
    ec_pdo_type_t type; /**< PDO type */
    uint16_t index; /**< PDO index */
    int8_t sync_index; /**< assigned sync manager */
    char *name; /**< PDO name */
    struct list_head entries; /**< entry list */
}
ec_pdo_t;

/*****************************************************************************/

/**
 * PDO entry description.
 */

typedef struct
{
    struct list_head list; /**< list item */
    uint16_t index; /**< PDO entry index */
    uint8_t subindex; /**< PDO entry subindex */
    char *name; /**< entry name */
    uint8_t bit_length; /**< entry length in bit */
}
ec_pdo_entry_t;

/*****************************************************************************/

void ec_pdo_init(ec_pdo_t *);
void ec_pdo_clear(ec_pdo_t *);
int ec_pdo_copy(ec_pdo_t *, const ec_pdo_t *);

/*****************************************************************************/

#endif
