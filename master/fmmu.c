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
   EtherCAT FMMU methods.
*/

/*****************************************************************************/

#include "globals.h"
#include "slave.h"
#include "master.h"
#include "fmmu.h"

/*****************************************************************************/

/**
 * FMMU Constructor.
 */

void ec_fmmu_init(
        ec_fmmu_t *fmmu, /**< EtherCAT FMMU */
        ec_slave_t *slave, /**< EtherCAT slave */
        unsigned int index /**< FMMU index */
        )
{
    fmmu->slave = slave;
    fmmu->index = index;
}

/*****************************************************************************/

/**
 * Initializes an FMMU configuration page.
 * The referenced memory (\a data) must be at least EC_FMMU_SIZE bytes.
 */

void ec_fmmu_config(
        const ec_fmmu_t *fmmu, /**< EtherCAT FMMU */
        uint8_t *data /**> configuration memory */
        )
{
    size_t sync_size = ec_sync_size(fmmu->sync);

    if (fmmu->slave->master->debug_level) {
        EC_DBG("FMMU%u: LogAddr 0x%08X, Size %3i, PhysAddr 0x%04X, Dir %s\n",
               fmmu->index, fmmu->logical_start_address,
               sync_size, fmmu->sync->physical_start_address,
               ((fmmu->sync->control_register & 0x04) ? "out" : "in"));
    }

    EC_WRITE_U32(data,      fmmu->logical_start_address);
    EC_WRITE_U16(data + 4,  sync_size); // size of fmmu
    EC_WRITE_U8 (data + 6,  0x00); // logical start bit
    EC_WRITE_U8 (data + 7,  0x07); // logical end bit
    EC_WRITE_U16(data + 8,  fmmu->sync->physical_start_address);
    EC_WRITE_U8 (data + 10, 0x00); // physical start bit
    EC_WRITE_U8 (data + 11, ((fmmu->sync->control_register & 0x04)
                             ? 0x02 : 0x01));
    EC_WRITE_U16(data + 12, 0x0001); // enable
    EC_WRITE_U16(data + 14, 0x0000); // reserved
}

/*****************************************************************************/
