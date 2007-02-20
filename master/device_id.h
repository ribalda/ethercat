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
   EtherCAT device ID structure.
*/

/*****************************************************************************/

#ifndef _EC_DEVICE_ID_H_
#define _EC_DEVICE_ID_H_

#include <linux/if_ether.h>

#include "globals.h"

/*****************************************************************************/

typedef enum {
    ec_device_id_empty,
    ec_device_id_mac
}
ec_device_id_type_t;

typedef struct {
    struct list_head list;
    ec_device_id_type_t type;
    unsigned char octets[ETH_ALEN];
}
ec_device_id_t;

/*****************************************************************************/

int ec_device_id_process_params(const char *, const char *,
        struct list_head *, struct list_head *);
void ec_device_id_clear_list(struct list_head *);
int ec_device_id_check(const ec_device_id_t *, const struct net_device *,
        const char *, unsigned int);

/*****************************************************************************/

#endif
