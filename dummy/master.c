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
   EtherCAT DUMMY master methods.
*/

/*****************************************************************************/

#include "../include/ecrt.h"
#include "../master/master.h"

/******************************************************************************
 *  Realtime interface
 *****************************************************************************/

/** \cond */

ec_domain_t *ecrt_master_create_domain(ec_master_t *master)
{
    return (ec_domain_t *) 1;
}

/*****************************************************************************/

int ecrt_master_activate(ec_master_t *master)
{
    return 0;
}

/*****************************************************************************/

void ecrt_master_send(ec_master_t *master)
{
}

/*****************************************************************************/

void ecrt_master_receive(ec_master_t *master)
{
}

/*****************************************************************************/

ec_slave_t *ecrt_master_get_slave(
        const ec_master_t *master,
        const char *address,
        uint32_t v,
        uint32_t p
        )
{
    return (ec_slave_t *) 1;
}

/*****************************************************************************/

void ecrt_master_callbacks(ec_master_t *m,
                           int (*q)(void *),
                           void (*l)(void *),
                           void *u
                           )
{
}

/*****************************************************************************/

void ecrt_master_get_status(const ec_master_t *master, /**< EtherCAT master */
        ec_master_status_t *status /**< target status object */
        )
{
    status->bus_status = EC_BUS_OK;
    status->bus_tainted = 0; 
    status->slaves_responding = 999;
}

/*****************************************************************************/

EXPORT_SYMBOL(ecrt_master_create_domain);
EXPORT_SYMBOL(ecrt_master_activate);
EXPORT_SYMBOL(ecrt_master_send);
EXPORT_SYMBOL(ecrt_master_receive);
EXPORT_SYMBOL(ecrt_master_callbacks);
EXPORT_SYMBOL(ecrt_master_get_slave);
EXPORT_SYMBOL(ecrt_master_get_status);

/** \endcond */

/*****************************************************************************/
