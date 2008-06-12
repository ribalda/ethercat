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
   CANopen Sdo functions.
*/

/*****************************************************************************/

#include <linux/slab.h>

#include "master.h"

#include "sdo.h"

/*****************************************************************************/

/** Constructor.
 */
void ec_sdo_init(
        ec_sdo_t *sdo, /**< Sdo. */
        ec_slave_t *slave, /**< Parent slave. */
        uint16_t index /**< Sdo index. */
        )
{
    sdo->slave = slave;
    sdo->index = index;
    sdo->object_code = 0x00;
    sdo->name = NULL;
    sdo->max_subindex = 0;
    INIT_LIST_HEAD(&sdo->entries);
}

/*****************************************************************************/

/** Sdo destructor.
 *
 * Clears and frees an Sdo object.
 */
void ec_sdo_clear(
        ec_sdo_t *sdo /**< Sdo. */
        )
{
    ec_sdo_entry_t *entry, *next;

    // free all entries
    list_for_each_entry_safe(entry, next, &sdo->entries, list) {
        list_del(&entry->list);
        ec_sdo_entry_clear(entry);
        kfree(entry);
    }

    if (sdo->name)
        kfree(sdo->name);
}

/*****************************************************************************/

/** Get an Sdo entry from an Sdo via its subindex.
 * 
 * \retval >0 Pointer to the requested Sdo entry.
 * \retval NULL Sdo entry not found.
 */
ec_sdo_entry_t *ec_sdo_get_entry(
        ec_sdo_t *sdo, /**< Sdo. */
        uint8_t subindex /**< Entry subindex. */
        )
{
    ec_sdo_entry_t *entry;

    list_for_each_entry(entry, &sdo->entries, list) {
        if (entry->subindex != subindex)
            continue;
        return entry;
    }

    return NULL;
}

/*****************************************************************************/

/** Get an Sdo entry from an Sdo via its subindex.
 *
 * const version.
 * 
 * \retval >0 Pointer to the requested Sdo entry.
 * \retval NULL Sdo entry not found.
 */
const ec_sdo_entry_t *ec_sdo_get_entry_const(
        const ec_sdo_t *sdo, /**< Sdo. */
        uint8_t subindex /**< Entry subindex. */
        )
{
    const ec_sdo_entry_t *entry;

    list_for_each_entry(entry, &sdo->entries, list) {
        if (entry->subindex != subindex)
            continue;
        return entry;
    }

    return NULL;
}

/*****************************************************************************/
