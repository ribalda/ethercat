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
   EtherCAT slave types.
*/

/*****************************************************************************/

#ifndef _EC_TYPES_H_
#define _EC_TYPES_H_

#include <linux/types.h>

#include "../include/ecrt.h"

/*****************************************************************************/

#define EC_MAX_FIELDS 10 /**< maximal number of data fields per sync manager */
#define EC_MAX_SYNC   16 /**< maximal number of sync managers per type */

/*****************************************************************************/

/**
   Special slaves.
*/

typedef enum
{
    EC_TYPE_NORMAL, /**< no special slave */
    EC_TYPE_BUS_COUPLER, /**< slave is a bus coupler */
    EC_TYPE_INFRA, /**< infrastructure slaves, that contain no process data */
    EC_TYPE_EOE /**< slave is an EoE switch */
}
ec_special_type_t;

/*****************************************************************************/

/**
   Process data field.
*/

typedef struct
{
    const char *name; /**< field name */
    size_t size; /**< field size in bytes */
}
ec_field_t;

/*****************************************************************************/

/**
   Sync manager.
*/

typedef struct
{
    uint16_t physical_start_address; /**< physical start address */
    uint16_t size; /**< size in bytes */
    uint8_t control_byte; /**< control register value */
    const ec_field_t *fields[EC_MAX_FIELDS]; /**< field array */
}
ec_sync_t;

/*****************************************************************************/

/**
   Slave description type.
*/

typedef struct ec_slave_type
{
    const char *vendor_name; /**< vendor name*/
    const char *product_name; /**< product name */
    const char *description; /**< free description */
    ec_special_type_t special; /**< special slave type? */
    const ec_sync_t *sync_managers[EC_MAX_SYNC]; /**< sync managers */
}
ec_slave_type_t;

/*****************************************************************************/

/**
   Slave type identification.
*/

typedef struct
{
    uint32_t vendor_id; /**< vendor id */
    uint32_t product_code; /**< product code */
    const ec_slave_type_t *type; /**< associated slave description object */
}
ec_slave_ident_t;

extern ec_slave_ident_t slave_idents[]; /**< array with slave descriptions */

/*****************************************************************************/

#endif
