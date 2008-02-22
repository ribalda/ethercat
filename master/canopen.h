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
   EtherCAT CANopen structures.
*/

/*****************************************************************************/

#ifndef _EC_CANOPEN_H_
#define _EC_CANOPEN_H_

#include <linux/list.h>
#include <linux/kobject.h>

#include "globals.h"
#include "slave.h"

/*****************************************************************************/

/**
   CANopen Sdo.
*/

struct ec_sdo
{
    struct kobject kobj; /**< kobject */
    struct list_head list; /**< list item */
    ec_slave_t *slave; /**< parent slave */
    uint16_t index; /**< Sdo index */
    uint8_t object_code; /**< object code */
    char *name; /**< Sdo name */
    uint8_t subindices; /**< subindices */
    struct list_head entries; /**< entry list */
};

/*****************************************************************************/

/**
   CANopen Sdo entry.
*/

typedef struct
{
    struct kobject kobj; /**< kobject */
    struct list_head list; /**< list item */
    ec_sdo_t *sdo; /**< parent Sdo */
    uint8_t subindex; /**< entry subindex */
    uint16_t data_type; /**< entry data type */
    uint16_t bit_length; /**< entry length in bit */
    char *description; /**< entry description */
}
ec_sdo_entry_t;

/*****************************************************************************/

/**
   CANopen Sdo configuration data.
*/

typedef struct
{
    struct list_head list; /**< list item */
    uint16_t index; /**< Sdo index */
    uint8_t subindex; /**< Sdo subindex */
    uint8_t *data; /**< pointer to Sdo data */
    size_t size; /**< size of Sdo data */
}
ec_sdo_data_t;

/*****************************************************************************/

/**
   CANopen Sdo request.
*/

typedef struct
{
    struct list_head list; /**< list item */
    ec_sdo_entry_t *entry; /**< Sdo entry */
    uint8_t *data; /**< pointer to Sdo data */
    size_t size; /**< size of Sdo data */
    ec_request_state_t state; /**< Sdo request state */
}
ec_sdo_request_t;

/*****************************************************************************/

int ec_sdo_init(ec_sdo_t *, uint16_t, ec_slave_t *);
void ec_sdo_destroy(ec_sdo_t *);
ec_sdo_entry_t *ec_sdo_get_entry(ec_sdo_t *, uint8_t);

int ec_sdo_entry_init(ec_sdo_entry_t *, uint8_t, ec_sdo_t *);
void ec_sdo_entry_destroy(ec_sdo_entry_t *);

void ec_sdo_request_init_read(ec_sdo_request_t *, ec_sdo_entry_t *);
void ec_sdo_request_clear(ec_sdo_request_t *);

/*****************************************************************************/

#endif
