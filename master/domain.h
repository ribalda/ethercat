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

#ifndef __EC_DOMAIN_H__
#define __EC_DOMAIN_H__

#include <linux/list.h>
#include <linux/kobject.h>

#include "globals.h"
#include "datagram.h"
#include "master.h"
#include "fmmu_config.h"

/*****************************************************************************/

/**
   EtherCAT domain.
   Handles the process data and the therefore needed datagrams of a certain
   group of slaves.
*/

struct ec_domain
{
    struct kobject kobj; /**< kobject. */
    struct list_head list; /**< List item. */
    ec_master_t *master; /**< EtherCAT master owning the domain. */
    unsigned int index; /**< Index (just a number). */
    size_t data_size; /**< Size of the process data. */
    uint16_t expected_working_counter; /**< Expected working counter. */
    uint8_t *data; /**< Memory for the process data. */
    ec_origin_t data_origin; /**< Origin of the \a data memory. */
    struct list_head datagrams; /**< Datagrams for process data exchange. */
    uint32_t logical_base_address; /**< Logical offset address of the
                                     process data. */
    uint16_t working_counter; /**< Last working counter value. */
    unsigned int working_counter_changes; /**< Working counter changes
                                             since last notification. */
    unsigned long notify_jiffies; /**< Time of last notification. */
};

/*****************************************************************************/

int ec_domain_init(ec_domain_t *, ec_master_t *, unsigned int);
void ec_domain_destroy(ec_domain_t *);

void ec_domain_add_fmmu_config(ec_domain_t *, ec_fmmu_config_t *);
int ec_domain_finish(ec_domain_t *, uint32_t);

/*****************************************************************************/

#endif
