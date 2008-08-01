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
   EtherCAT process data object entry methods.
*/

/*****************************************************************************/

#include <linux/slab.h>

#include "pdo_entry.h"

/*****************************************************************************/

/** Pdo entry constructor.
 */
void ec_pdo_entry_init(
        ec_pdo_entry_t *entry /**< Pdo entry. */
        )
{
    entry->name = NULL;
}

/*****************************************************************************/

/** Pdo entry copy constructor.
 */
int ec_pdo_entry_init_copy(
        ec_pdo_entry_t *entry, /**< Pdo entry. */
        const ec_pdo_entry_t *other /**< Pdo entry to copy from. */
        )
{
    entry->index = other->index;
    entry->subindex = other->subindex;
    entry->name = NULL;
    entry->bit_length = other->bit_length;

    if (ec_pdo_entry_set_name(entry, other->name))
        return -1;

    return 0;
}

/*****************************************************************************/

/** Pdo entry destructor.
 */
void ec_pdo_entry_clear(ec_pdo_entry_t *entry /**< Pdo entry. */)
{
    if (entry->name)
        kfree(entry->name);
}

/*****************************************************************************/

/** Set Pdo entry name.
 */
int ec_pdo_entry_set_name(
        ec_pdo_entry_t *entry, /**< Pdo entry. */
        const char *name /**< New name. */
        )
{
    unsigned int len;

    if (entry->name && name && !strcmp(entry->name, name))
        return 0;
    
    if (entry->name)
        kfree(entry->name);

    if (name && (len = strlen(name))) {
        if (!(entry->name = (char *) kmalloc(len + 1, GFP_KERNEL))) {
            EC_ERR("Failed to allocate Pdo entry name.\n");
            return -1;
        }
        memcpy(entry->name, name, len + 1);
    } else {
        entry->name = NULL;
    }

    return 0;
}

/*****************************************************************************/

/** Compares two Pdo entries.
 *
 * \retval 1 The entries are equal.
 * \retval 0 The entries differ.
 */
int ec_pdo_entry_equal(
        const ec_pdo_entry_t *entry1, /**< First Pdo entry. */
        const ec_pdo_entry_t *entry2 /**< Second Pdo entry. */
        )
{
    return entry1->index == entry2->index
        && entry1->subindex == entry2->subindex
        && entry1->bit_length == entry2->bit_length;
}

/*****************************************************************************/
