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
   EtherCAT sync manager methods.
*/

/*****************************************************************************/

#include "globals.h"
#include "slave.h"
#include "master.h"
#include "pdo.h"
#include "sync.h"

/*****************************************************************************/

/**
 * Constructor.
 */

void ec_sync_init(
        ec_sync_t *sync, /**< EtherCAT sync manager */
        ec_slave_t *slave, /**< EtherCAT slave */
        unsigned int index /**< sync manager index */
        )
{
    sync->slave = slave;
    sync->index = index;    

    sync->est_length = 0;
    INIT_LIST_HEAD(&sync->pdos);
    sync->alt_mapping = 0;
}

/*****************************************************************************/

/**
 * Destructor.
 */

void ec_sync_clear(
        ec_sync_t *sync /**< EtherCAT sync manager */
        )
{
    ec_sync_clear_pdos(sync);
}

/*****************************************************************************/

/**
 * Calculates the size of a sync manager by evaluating PDO sizes.
 * \return sync manager size
 */

uint16_t ec_sync_size(
        const ec_sync_t *sync /**< sync manager */
        )
{
    const ec_pdo_t *pdo;
    const ec_pdo_entry_t *pdo_entry;
    unsigned int bit_size, byte_size;

    if (sync->length) return sync->length;
    if (sync->est_length) return sync->est_length;

    bit_size = 0;
    list_for_each_entry(pdo, &sync->pdos, list) {
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

/**
   Initializes a sync manager configuration page with EEPROM data.
   The referenced memory (\a data) must be at least EC_SYNC_SIZE bytes.
*/

void ec_sync_config(
        const ec_sync_t *sync, /**< sync manager */
        uint8_t *data /**> configuration memory */
        )
{
    size_t sync_size = ec_sync_size(sync);

    if (sync->slave->master->debug_level) {
        EC_DBG("SM%i: Addr 0x%04X, Size %3i, Ctrl 0x%02X, En %i\n",
               sync->index, sync->physical_start_address,
               sync_size, sync->control_register, sync->enable);
    }

    EC_WRITE_U16(data,     sync->physical_start_address);
    EC_WRITE_U16(data + 2, sync_size);
    EC_WRITE_U8 (data + 4, sync->control_register);
    EC_WRITE_U8 (data + 5, 0x00); // status byte (read only)
    EC_WRITE_U16(data + 6, sync->enable ? 0x0001 : 0x0000); // enable
}

/*****************************************************************************/

/**
 */

int ec_sync_add_pdo(
        ec_sync_t *sync, /**< EtherCAT sync manager */
        const ec_pdo_t *pdo /**< PDO to map */
        )
{
    ec_pdo_t *mapped_pdo;

    // PDO already mapped?
    list_for_each_entry(mapped_pdo, &sync->pdos, list) {
        if (mapped_pdo->index != pdo->index) continue;
        EC_ERR("PDO 0x%04X is already mapped!\n", pdo->index);
        return -1;
    }
    
    if (!(mapped_pdo = kmalloc(sizeof(ec_pdo_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate memory for PDO mapping.\n");
        return -1;
    }

    ec_pdo_init(mapped_pdo);
    if (ec_pdo_copy(mapped_pdo, pdo)) {
        ec_pdo_clear(mapped_pdo);
        kfree(mapped_pdo);
        return -1;
    }

    // set appropriate sync manager index
    mapped_pdo->sync_index = sync->index;

    list_add_tail(&mapped_pdo->list, &sync->pdos);
    sync->alt_mapping = 1;
    return 0;
}

/*****************************************************************************/

/**
 */

void ec_sync_clear_pdos(
        ec_sync_t *sync /**< EtherCAT sync manager */
        )
{
    ec_pdo_t *pdo, *next;

    // free all mapped PDOs
    list_for_each_entry_safe(pdo, next, &sync->pdos, list) {
        list_del(&pdo->list);
        ec_pdo_clear(pdo);
        kfree(pdo);
    }

    sync->alt_mapping = 1;
}

/*****************************************************************************/
