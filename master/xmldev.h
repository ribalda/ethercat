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
   EtherCAT XML device.
*/

/*****************************************************************************/

#ifndef _EC_XMLDEV_H_
#define _EC_XMLDEV_H_

#include <linux/fs.h>
#include <linux/cdev.h>

#include "globals.h"
#include "../include/ecrt.h"

/*****************************************************************************/

/**
   EtherCAT XML character device.
*/

typedef struct
{
    ec_master_t *master; /**< master owning the device */
    struct cdev cdev; /**< character device */
    atomic_t available; /**< allow only one open() */
}
ec_xmldev_t;

/*****************************************************************************/

/** \cond */

int ec_xmldev_init(ec_xmldev_t *, ec_master_t *, dev_t);
void ec_xmldev_clear(ec_xmldev_t *);

int ec_xmldev_request(ec_xmldev_t *, uint32_t, uint32_t);

/** \endcond */

/*****************************************************************************/

#endif
