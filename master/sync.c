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
 * EtherCAT sync manager methods.
 */

/*****************************************************************************/

#include "globals.h"
#include "slave.h"
#include "master.h"
#include "pdo.h"
#include "sync.h"

/*****************************************************************************/

/** Constructor.
 */
void ec_sync_init(
        ec_sync_t *sync, /**< EtherCAT sync manager. */
        ec_slave_t *slave, /**< EtherCAT slave. */
        unsigned int index /**< Sync manager index. */
        )
{
    sync->slave = slave;
    sync->index = index;    

    ec_pdo_list_init(&sync->pdos);
    sync->assign_source = EC_ASSIGN_NONE;
}

/*****************************************************************************/

/** Copy constructor.
 */
void ec_sync_init_copy(
        ec_sync_t *sync, /**< EtherCAT sync manager. */
        const ec_sync_t *other /**< Sync manager to copy from. */
        )
{
   sync->slave = other->slave;
   sync->index = other->index;
   sync->physical_start_address = other->physical_start_address;
   sync->length = other->length;
   sync->control_register = other->control_register;
   sync->enable = other->enable;
   
   ec_pdo_list_init(&sync->pdos);
   ec_pdo_list_copy(&sync->pdos, &other->pdos);

   sync->assign_source = other->assign_source;
}

/*****************************************************************************/

/** Destructor.
 */
void ec_sync_clear(
        ec_sync_t *sync /**< EtherCAT sync manager. */
        )
{
    ec_pdo_list_clear(&sync->pdos);
}

/*****************************************************************************/

/** Initializes a sync manager configuration page with SII data.
 *
 * The referenced memory (\a data) must be at least \a EC_SYNC_SIZE bytes.
 */
void ec_sync_config(
        const ec_sync_t *sync, /**< Sync manager. */
        uint16_t data_size, /**< Data size. */
        uint8_t *data /**> Configuration memory. */
        )
{
    // enable only if SII enable is set and size is > 0.
    uint16_t enable = sync->enable && data_size;

    if (sync->slave->master->debug_level) {
        EC_DBG("SM%u: Addr 0x%04X, Size %3u, Ctrl 0x%02X, En %u\n",
               sync->index, sync->physical_start_address,
               data_size, sync->control_register, enable);
    }

    EC_WRITE_U16(data,     sync->physical_start_address);
    EC_WRITE_U16(data + 2, data_size);
    EC_WRITE_U8 (data + 4, sync->control_register);
    EC_WRITE_U8 (data + 5, 0x00); // status byte (read only)
    EC_WRITE_U16(data + 6, enable);
}

/*****************************************************************************/

/** Adds a Pdo to the list of known mapped Pdos.
 *
 * \return 0 on success, else < 0
 */
int ec_sync_add_pdo(
        ec_sync_t *sync, /**< EtherCAT sync manager. */
        const ec_pdo_t *pdo /**< Pdo to map. */
        )
{
    return ec_pdo_list_add_pdo_copy(&sync->pdos, pdo);
}

/*****************************************************************************/

/** Get direction covered by sync manager.
 *
 * \return Direction covered by the given sync manager.
 */
ec_direction_t ec_sync_direction(
        const ec_sync_t *sync /**< EtherCAT sync manager. */
        )
{
    int index = sync->index;

    if (sync->slave->sii.sync_count != 1) {
        if (sync->slave && sync->slave->sii.mailbox_protocols) {
            index -= 2;
        }

        if (index < 0 || index > 1) {
            EC_WARN("ec_sync_get_direction(): invalid sync manager index.\n");
            return EC_DIR_OUTPUT;
        }
    } else { // sync_count == 1
        if (!list_empty(&sync->pdos.list)) {
            const ec_pdo_t *pdo =
                list_entry(sync->pdos.list.next, ec_pdo_t, list);
            return pdo->dir;
        }
        
    }

    return (ec_direction_t) index;
}

/*****************************************************************************/
