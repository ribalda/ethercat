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
   EtherCAT Pdo mapping methods.
*/

/*****************************************************************************/

#include <linux/module.h>

#include "globals.h"
#include "pdo.h"
#include "slave_config.h"
#include "master.h"

#include "pdo_mapping.h"

/*****************************************************************************/

/** Pdo mapping constructor.
 */
void ec_pdo_mapping_init(
        ec_pdo_mapping_t *pm /**< Pdo mapping. */
        )
{
    INIT_LIST_HEAD(&pm->pdos);
    pm->default_mapping = 1;
}

/*****************************************************************************/

/** Pdo mapping destructor.
 */
void ec_pdo_mapping_clear(ec_pdo_mapping_t *pm /**< Pdo mapping. */)
{
    ec_pdo_mapping_clear_pdos(pm);
}

/*****************************************************************************/

/** Clears the list of mapped Pdos.
 */
void ec_pdo_mapping_clear_pdos(ec_pdo_mapping_t *pm /**< Pdo mapping. */)
{
    ec_pdo_t *pdo, *next;

    list_for_each_entry_safe(pdo, next, &pm->pdos, list) {
        list_del_init(&pdo->list);
        ec_pdo_clear(pdo);
        kfree(pdo);
    }
}

/*****************************************************************************/

/** Calculates the total size of the mapped Pdo entries.
 *
 * \retval Data size in byte.
 */
uint16_t ec_pdo_mapping_total_size(
        const ec_pdo_mapping_t *pm /**< Pdo mapping. */
        )
{
    unsigned int bit_size;
    const ec_pdo_t *pdo;
    const ec_pdo_entry_t *pdo_entry;
    uint16_t byte_size;

    bit_size = 0;
    list_for_each_entry(pdo, &pm->pdos, list) {
        list_for_each_entry(pdo_entry, &pdo->entries, list) {
            bit_size += pdo_entry->bit_length;
        }
    }

    if (bit_size % 8) // round up to full bytes
        byte_size = bit_size / 8 + 1;
    else
        byte_size = bit_size / 8;

    return byte_size;
}

/*****************************************************************************/

/** Add a new Pdo to the mapping.
 *
 * \retval >0 Pointer to new Pdo.
 * \retval NULL No memory.
 */
ec_pdo_t *ec_pdo_mapping_add_pdo(
        ec_pdo_mapping_t *pm, /**< Pdo mapping. */
        uint16_t index, /**< Pdo index. */
        ec_direction_t dir /**< Direction. */
        )
{
    ec_pdo_t *pdo;

    if (!(pdo = (ec_pdo_t *) kmalloc(sizeof(ec_pdo_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate memory for Pdo.\n");
        return NULL;
    }

    ec_pdo_init(pdo);
    pdo->dir = dir;
    pdo->index = index;
    list_add_tail(&pdo->list, &pm->pdos);
    return pdo;
}

/*****************************************************************************/

/** Add the copy of an existing Pdo to the mapping.
 *
 * \return 0 on success, else < 0
 */
int ec_pdo_mapping_add_pdo_copy(
        ec_pdo_mapping_t *pm, /**< Pdo mapping. */
        const ec_pdo_t *pdo /**< Pdo to add. */
        )
{
    ec_pdo_t *mapped_pdo;

    // Pdo already mapped?
    list_for_each_entry(mapped_pdo, &pm->pdos, list) {
        if (mapped_pdo->index != pdo->index) continue;
        EC_ERR("Pdo 0x%04X is already mapped!\n", pdo->index);
        return -1;
    }
    
    if (!(mapped_pdo = kmalloc(sizeof(ec_pdo_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate Pdo memory.\n");
        return -1;
    }

    if (ec_pdo_init_copy(mapped_pdo, pdo)) {
        kfree(mapped_pdo);
        return -1;
    }

    list_add_tail(&mapped_pdo->list, &pm->pdos);
    return 0;
}

/*****************************************************************************/

/** Makes a deep copy of another Pdo mapping.
 *
 * \return 0 on success, else < 0
 */
int ec_pdo_mapping_copy(
        ec_pdo_mapping_t *pm, /**< Pdo mapping. */
        const ec_pdo_mapping_t *other /**< Pdo mapping to copy from. */
        )
{
    ec_pdo_t *other_pdo;

    ec_pdo_mapping_clear_pdos(pm);

    // Pdo already mapped?
    list_for_each_entry(other_pdo, &other->pdos, list) {
        if (ec_pdo_mapping_add_pdo_copy(pm, other_pdo))
            return -1;
    }
    
    return 0;
}

/*****************************************************************************/

/** Compares two Pdo mappings.
 *
 * Only the mapping is compared, not the Pdo entries (i. e. the Pdo
 * configuration).
 *
 * \retval 1 The given Pdo mappings are equal.
 * \retval 0 The given Pdo mappings differ.
 */
int ec_pdo_mapping_equal(
        const ec_pdo_mapping_t *pm1, /**< First mapping. */
        const ec_pdo_mapping_t *pm2 /**< Second mapping. */
        )
{
    const struct list_head *h1, *h2, *l1, *l2;
    const ec_pdo_t *p1, *p2;

    h1 = l1 = &pm1->pdos;
    h2 = l2 = &pm2->pdos;

    while (1) {
        l1 = l1->next;
        l2 = l2->next;

        if ((l1 == h1) ^ (l2 == h2)) // unequal lengths
            return 0;
        if (l1 == h1) // both finished
            break;

        p1 = list_entry(l1, ec_pdo_t, list);
        p2 = list_entry(l2, ec_pdo_t, list);

        if (p1->index != p2->index)
            return 0;
    }

    return 1;
}

/*****************************************************************************/

/** Finds a Pdo with the given index.
 */
ec_pdo_t *ec_pdo_mapping_find_pdo(
        const ec_pdo_mapping_t *pm, /**< Pdo mapping. */
        uint16_t index /**< Pdo index. */
        )
{
    ec_pdo_t *pdo;

    list_for_each_entry(pdo, &pm->pdos, list) {
        if (pdo->index != index)
            continue;
        return pdo;
    }

    return NULL;
}

/*****************************************************************************/

/** Finds a Pdo with the given index and returns a const pointer.
 */
const ec_pdo_t *ec_pdo_mapping_find_pdo_const(
        const ec_pdo_mapping_t *pm, /**< Pdo mapping. */
        uint16_t index /**< Pdo index. */
        )
{
    const ec_pdo_t *pdo;

    list_for_each_entry(pdo, &pm->pdos, list) {
        if (pdo->index != index)
            continue;
        return pdo;
    }

    return NULL;
}

/*****************************************************************************/
