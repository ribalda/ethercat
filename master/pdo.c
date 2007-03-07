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
   EtherCAT process data object methods.
*/

/*****************************************************************************/

#include <linux/slab.h>

#include "pdo.h"

/*****************************************************************************/

/**
 * PDO constructor.
 */

void ec_pdo_init(ec_pdo_t *pdo /**< EtherCAT PDO */)
{
    pdo->name = NULL;
    INIT_LIST_HEAD(&pdo->entries);
}

/*****************************************************************************/

/**
 * PDO destructor.
 */

void ec_pdo_clear(ec_pdo_t *pdo /**< EtherCAT PDO */)
{
    ec_pdo_entry_t *entry, *next;

    // free all PDO entries
    list_for_each_entry_safe(entry, next, &pdo->entries, list) {
        list_del(&entry->list);
        kfree(entry);
    }
}

/*****************************************************************************/

/**
 * Makes a deep copy of a PDO.
 */

int ec_pdo_copy(ec_pdo_t *pdo, const ec_pdo_t *other_pdo)
{
    ec_pdo_entry_t *entry, *other_entry, *next;

    // make flat copy
    *pdo = *other_pdo;

    INIT_LIST_HEAD(&pdo->entries);
    list_for_each_entry(other_entry, &other_pdo->entries, list) {
        if (!(entry = (ec_pdo_entry_t *)
                    kmalloc(sizeof(ec_pdo_entry_t), GFP_KERNEL))) {
            EC_ERR("Failed to allocate memory for PDO entry copy.\n");
            goto out_free;
        }

        *entry = *other_entry; // flat copy is sufficient
        list_add_tail(&entry->list, &pdo->entries);
    }

    return 0;

out_free:
    list_for_each_entry_safe(entry, next, &pdo->entries, list) {
        list_del(&entry->list);
        kfree(entry);
    }
    return -1;
}

/*****************************************************************************/
