/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2008  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version 2, as
 *  published by the Free Software Foundation.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  ---
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 *****************************************************************************/

/** \file
 * EtherCAT tty driver module.
 */

/*****************************************************************************/

#include <linux/module.h>
#include <linux/err.h>

#include "../master/globals.h"

/*****************************************************************************/

int __init ec_tty_init_module(void);
void __exit ec_tty_cleanup_module(void);

unsigned int debug_level = 0;
char *ec_master_version_str = EC_MASTER_VERSION; /**< Version string. */

/*****************************************************************************/

/** \cond */

MODULE_AUTHOR("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION("EtherCAT TTY driver module");
MODULE_LICENSE("GPL");
MODULE_VERSION(EC_MASTER_VERSION);

module_param_named(debug_level, debug_level, uint, S_IRUGO);
MODULE_PARM_DESC(debug_level, "Debug level");

/** \endcond */

/*****************************************************************************/

/** Module initialization.
 *
 * \return 0 on success, else < 0
 */
int __init ec_tty_init_module(void)
{
    int ret = 0;

    EC_INFO("TTY driver %s\n", EC_MASTER_VERSION);

    return ret;
}

/*****************************************************************************/

/** Module cleanup.
 *
 * Clears all master instances.
 */
void __exit ec_tty_cleanup_module(void)
{
    EC_INFO("TTY module cleaned up.\n");
}

/*****************************************************************************/

/** \cond */

module_init(ec_tty_init_module);
module_exit(ec_tty_cleanup_module);

/** \endcond */

/*****************************************************************************/
