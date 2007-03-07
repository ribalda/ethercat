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
#include "sync.h"

/*****************************************************************************/

/**
 * Constructor.
 */

void ec_sync_init(
        ec_sync_t *sync, /**< EtherCAT sync manager */
        const ec_slave_t *slave, /**< EtherCAT slave */
        unsigned int index /**< sync manager index */
        )
{
    sync->slave = slave;
    sync->index = index;    

    sync->est_length = 0;
}

/*****************************************************************************/

/**
 * Destructor.
 */

void ec_sync_clear(
        ec_sync_t *sync /**< EtherCAT sync manager */
        )
{
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
    size_t sync_size;

    sync_size = ec_slave_calc_sync_size(sync->slave, sync);

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
