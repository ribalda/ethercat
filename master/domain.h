/******************************************************************************
 *
 *  d o m a i n . h
 *
 *  EtherCAT domain structure.
 *
 *  $Id$
 *
 *  Copyright (C) 2006  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; version 2 of the License.
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
 *****************************************************************************/

#ifndef _EC_DOMAIN_H_
#define _EC_DOMAIN_H_

#include <linux/list.h>
#include <linux/kobject.h>

#include "globals.h"
#include "slave.h"
#include "command.h"

/*****************************************************************************/

/**
   \defgroup Domain EtherCAT domain
   Data types and methods for EtherCAT domains.
   A domain handles process data IO with a group of slaves.
   \{
*/

/*****************************************************************************/

/**
   Data field registration type.
*/

typedef struct
{
    struct list_head list; /**< list item */
    ec_slave_t *slave; /**< slave */
    const ec_sync_t *sync; /**< sync manager */
    uint32_t field_offset; /**< data field offset */
    void **data_ptr; /**< pointer to process data pointer(s) */
}
ec_field_reg_t;

/*****************************************************************************/

/**
   EtherCAT domain.
   Handles the process data and the therefore needed commands of a certain
   group of slaves.
*/

struct ec_domain
{
    struct kobject kobj; /**< kobject */
    struct list_head list; /**< list item */
    unsigned int index; /**< domain index (just a number) */
    ec_master_t *master; /**< EtherCAT master owning the domain */
    size_t data_size; /**< size of the process data */
    struct list_head commands; /**< process data commands */
    uint32_t base_address; /**< logical offset address of the process data */
    unsigned int response_count; /**< number of responding slaves */
    struct list_head field_regs; /**< data field registrations */
};

/** \} */

/*****************************************************************************/

int ec_domain_init(ec_domain_t *, ec_master_t *, unsigned int);
void ec_domain_clear(struct kobject *);
int ec_domain_alloc(ec_domain_t *, uint32_t);

/*****************************************************************************/

#endif
