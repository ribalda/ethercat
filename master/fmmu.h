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
   EtherCAT FMMU structure.
*/

/*****************************************************************************/

#ifndef _EC_FMMU_H_
#define _EC_FMMU_H_

#include "../include/ecrt.h"

#include "globals.h"

/*****************************************************************************/

/** size of an FMMU configuration page */
#define EC_FMMU_SIZE 16

/*****************************************************************************/

/**
 * FMMU configuration.
 */

typedef struct
{
    const ec_slave_t *slave; /**< EtherCAT slave, the FMMU belongs to */
    unsigned int index; /**< FMMU index */
    const ec_domain_t *domain; /**< domain */
    const ec_sync_t *sync; /**< sync manager */
    uint32_t logical_start_address; /**< logical start address */
}
ec_fmmu_t;

/*****************************************************************************/

void ec_fmmu_init(ec_fmmu_t *, ec_slave_t *, unsigned int);

void ec_fmmu_config(const ec_fmmu_t *, uint8_t *);

/*****************************************************************************/

#endif
