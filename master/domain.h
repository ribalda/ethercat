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
   EtherCAT domain structure.
*/

/*****************************************************************************/

#ifndef _EC_DOMAIN_H_
#define _EC_DOMAIN_H_

#include <linux/list.h>
#include <linux/kobject.h>

#include "globals.h"
#include "datagram.h"
#include "master.h"

/*****************************************************************************/

/**
   EtherCAT domain.
   Handles the process data and the therefore needed datagrams of a certain
   group of slaves.
*/

struct ec_domain
{
    struct kobject kobj; /**< kobject */
    struct list_head list; /**< list item */
    unsigned int index; /**< domain index (just a number) */
    ec_master_t *master; /**< EtherCAT master owning the domain */
    size_t data_size; /**< size of the process data */
    struct list_head datagrams; /**< process data datagrams */
    uint32_t base_address; /**< logical offset address of the process data */
    unsigned int response_count; /**< number of responding slaves */
    struct list_head data_regs; /**< PDO data registrations */
    unsigned int working_counter_changes; /**< working counter changes
                                             since last notification */
    cycles_t t_last; /**< time of last notification */
};

/*****************************************************************************/

int ec_domain_init(ec_domain_t *, ec_master_t *, unsigned int);
void ec_domain_clear(struct kobject *);
int ec_domain_alloc(ec_domain_t *, uint32_t);
void ec_domain_queue(ec_domain_t *);

/*****************************************************************************/

#endif
