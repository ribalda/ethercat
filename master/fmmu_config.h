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
 * EtherCAT FMMU configuration structure.
 */

/*****************************************************************************/

#ifndef _EC_FMMU_CONFIG_H_
#define _EC_FMMU_CONFIG_H_

#include "../include/ecrt.h"

#include "globals.h"

/*****************************************************************************/

/** FMMU configuration.
 */
typedef struct
{
    const ec_slave_config_t *sc; /**< EtherCAT slave config. */
    const ec_domain_t *domain; /**< Domain. */
    ec_direction_t dir; /**< Pdo direction. */

    uint32_t logical_start_address; /**< Logical start address. */
    unsigned int data_size; /**< Covered Pdo size. */
}
ec_fmmu_config_t;

/*****************************************************************************/

void ec_fmmu_config_init(ec_fmmu_config_t *, ec_slave_config_t *,
        ec_domain_t *, ec_direction_t);

void ec_fmmu_config_page(const ec_fmmu_config_t *, const ec_sync_t *,
        uint8_t *);

/*****************************************************************************/

#endif
