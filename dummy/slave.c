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
   EtherCAT DUMMY slave methods.
*/

/*****************************************************************************/

#include "../include/ecrt.h"
#include "../master/master.h"
#include "../master/slave.h"

/*****************************************************************************/

/** \cond */

int ecrt_slave_conf_sdo8(ec_slave_t *slave, /**< EtherCAT slave */
                         uint16_t sdo_index, /**< SDO index */
                         uint8_t sdo_subindex, /**< SDO subindex */
                         uint8_t value /**< new SDO value */
                         )
{
    return 0;
}

/*****************************************************************************/

/**
   \return 0 in case of success, else < 0
   \ingroup RealtimeInterface
*/

int ecrt_slave_conf_sdo16(ec_slave_t *slave, /**< EtherCAT slave */
                          uint16_t sdo_index, /**< SDO index */
                          uint8_t sdo_subindex, /**< SDO subindex */
                          uint16_t value /**< new SDO value */
                          )
{
    return 0;
}

/*****************************************************************************/

/**
   \return 0 in case of success, else < 0
   \ingroup RealtimeInterface
*/

int ecrt_slave_conf_sdo32(ec_slave_t *slave, /**< EtherCAT slave */
                          uint16_t sdo_index, /**< SDO index */
                          uint8_t sdo_subindex, /**< SDO subindex */
                          uint32_t value /**< new SDO value */
                          )
{
    return 0;
}

/*****************************************************************************/

void ecrt_slave_pdo_mapping_clear(
        ec_slave_t *slave, /**< EtherCAT slave */
        ec_direction_t dir /**< output/input */
        )
{
}

/*****************************************************************************/

int ecrt_slave_pdo_mapping_add(
        ec_slave_t *slave, /**< EtherCAT slave */
        ec_direction_t dir, /**< input/output */
        uint16_t pdo_index /**< Index of PDO mapping list */)
{
    return 0;
}

/*****************************************************************************/

int ecrt_slave_pdo_mapping(ec_slave_t *slave, /**< EtherCAT slave */
        ec_direction_t dir, /**< input/output */
        unsigned int num_args, /**< Number of following arguments */
        ... /**< PDO indices to map */
        )
{
    return 0;
}


/*****************************************************************************/

/** \cond */

EXPORT_SYMBOL(ecrt_slave_conf_sdo8);
EXPORT_SYMBOL(ecrt_slave_conf_sdo16);
EXPORT_SYMBOL(ecrt_slave_conf_sdo32);
EXPORT_SYMBOL(ecrt_slave_pdo_mapping_clear);
EXPORT_SYMBOL(ecrt_slave_pdo_mapping_add);
EXPORT_SYMBOL(ecrt_slave_pdo_mapping);

/** \endcond */

/*****************************************************************************/
