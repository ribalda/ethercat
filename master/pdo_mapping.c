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

/** Adds a Pdo to the mapping.
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
        EC_ERR("Failed to allocate memory for Pdo mapping.\n");
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

/** Add a Pdo to the mapping.
 *
 * The first call of this method will clear the default mapping.
 *
 * \retval 0 Success.
 * \retval -1 Error.
 */
int ec_pdo_mapping_add_pdo_info(
        ec_pdo_mapping_t *pm, /**< Pdo mapping. */
        const ec_pdo_info_t *pdo_info, /**< Pdo information. */
        const ec_slave_config_t *config /**< Slave configuration, to load
                                          default entries. */
        )
{
    unsigned int i;
    ec_pdo_t *pdo;
    ec_pdo_entry_t *entry;
    const ec_pdo_entry_info_t *entry_info;

    if (pm->default_mapping) {
        pm->default_mapping = 0;
        ec_pdo_mapping_clear_pdos(pm);
    }

    if (!(pdo = (ec_pdo_t *) kmalloc(sizeof(ec_pdo_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate memory for Pdo.\n");
        goto out_return;
    }

    ec_pdo_init(pdo);
    pdo->dir = pdo_info->dir;
    pdo->index = pdo_info->index;

    if (config->master->debug_level)
        EC_INFO("Adding Pdo 0x%04X to mapping.\n", pdo->index);

    if (pdo_info->n_entries && pdo_info->entries) { // configuration provided
        if (config->master->debug_level)
            EC_INFO("  Pdo configuration provided.\n");

        for (i = 0; i < pdo_info->n_entries; i++) {
            entry_info = &pdo_info->entries[i];

            if (!(entry = kmalloc(sizeof(ec_pdo_entry_t), GFP_KERNEL))) {
                EC_ERR("Failed to allocate memory for Pdo entry.\n");
                goto out_free;
            }

            ec_pdo_entry_init(entry);
            entry->index = entry_info->index;
            entry->subindex = entry_info->subindex;
            entry->bit_length = entry_info->bit_length;
            list_add_tail(&entry->list, &pdo->entries);
        }
    } else { // use default Pdo configuration
        if (config->master->debug_level)
            EC_INFO("  Using default Pdo configuration.\n");

        if (config->slave) {
            ec_sync_t *sync;
            ec_pdo_t *default_pdo;

            if ((sync = ec_slave_get_pdo_sync(config->slave, pdo->dir))) {
                list_for_each_entry(default_pdo, &sync->mapping.pdos, list) {
                    if (default_pdo->index != pdo->index)
                        continue;
                    if (config->master->debug_level)
                        EC_INFO("  Found Pdo name \"%s\".\n",
                                default_pdo->name);
                    // try to take Pdo name from mapped one
                    if (ec_pdo_set_name(pdo, default_pdo->name))
                        goto out_free;
                    // copy entries (= default Pdo configuration)
                    if (ec_pdo_copy_entries(pdo, default_pdo))
                        goto out_free;
                    if (config->master->debug_level) {
                        const ec_pdo_entry_t *entry;
                        list_for_each_entry(entry, &pdo->entries, list) {
                            EC_INFO("    Entry 0x%04X:%u.\n",
                                    entry->index, entry->subindex);
                        }
                    }
                }
            } else {
                EC_WARN("Slave %u does not provide a default Pdo"
                        " configuration!\n", config->slave->ring_position);
            }
        } else {
            EC_WARN("Failed to load default Pdo configuration for %u:%u:"
                    " Slave not found.\n", config->alias, config->position);
        }
    }

    list_add_tail(&pdo->list, &pm->pdos);
    return 0;

out_free:
    ec_pdo_clear(pdo);
    kfree(pdo);
out_return:
    return -1;
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
const ec_pdo_t *ec_pdo_mapping_find_pdo(
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
