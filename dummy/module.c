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
   Dummy EtherCAT master driver module.
*/

/*****************************************************************************/

#include <linux/module.h>
//#include <linux/kernel.h>
//#include <linux/init.h>

#include "../master/globals.h"
#include "../include/ecrt.h"

/*****************************************************************************/

int __init ec_init_module(void);
void __exit ec_cleanup_module(void);

char *ec_master_version_str = EC_MASTER_VERSION;

/*****************************************************************************/

/** \cond */

MODULE_AUTHOR("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION("DUMMY EtherCAT master driver module");
MODULE_LICENSE("GPL");
MODULE_VERSION(EC_MASTER_VERSION);

/** \endcond */

/*****************************************************************************/

/**
 * Module initialization.
 * Initializes \a ec_master_count masters.
 * \return 0 on success, else < 0
 */

int __init ec_init_module(void)
{
    EC_INFO("Master DUMMY driver %s\n", EC_MASTER_VERSION);
    return 0;
}

/*****************************************************************************/

/**
   Module cleanup.
   Clears all master instances.
*/

void __exit ec_cleanup_module(void)
{
    EC_INFO("Master DUMMY module cleaned up.\n");
}

/******************************************************************************
 *  Realtime interface
 *****************************************************************************/

unsigned int ecrt_version_magic(void)
{
    return ECRT_VERSION_MAGIC;
}

/*****************************************************************************/

ec_master_t *ecrt_request_master(unsigned int master_index
                                 /**< master index */
                                 )
{
    EC_INFO("Requesting DUMMY master %u...\n", master_index);
    return (void *) master_index + 1;
}

/*****************************************************************************/

/**
   Releases a reserved EtherCAT master.
   \ingroup RealtimeInterface
*/

void ecrt_release_master(ec_master_t *master /**< EtherCAT master */)
{
    EC_INFO("Released DUMMY master %u.\n", (unsigned int) master - 1);
}

/*****************************************************************************/

/** \cond */

module_init(ec_init_module);
module_exit(ec_cleanup_module);

EXPORT_SYMBOL(ecrt_request_master);
EXPORT_SYMBOL(ecrt_release_master);
EXPORT_SYMBOL(ecrt_version_magic);

/** \endcond */

/*****************************************************************************/
