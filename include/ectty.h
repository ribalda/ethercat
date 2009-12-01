/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2008  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT master userspace library.
 *  
 *  The IgH EtherCAT master userspace library is free software; you can
 *  redistribute it and/or modify it under the terms of the GNU Lesser General
 *  Public License as published by the Free Software Foundation; version 2.1
 *  of the License.
 *
 *  The IgH EtherCAT master userspace library is distributed in the hope that
 *  it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with the IgH EtherCAT master userspace library. If not, see
 *  <http://www.gnu.org/licenses/>.
 *  
 *  ---
 *  
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 *****************************************************************************/

/** \file
 *
 * EtherCAT virtual TTY interface.
 *
 * \defgroup TTYInterface EtherCAT Virtual TTY Interface
 *
 * @{
 */

/*****************************************************************************/

#ifndef __ECTTY_H__
#define __ECTTY_H__

/******************************************************************************
 * Data types 
 *****************************************************************************/

struct ec_tty;
typedef struct ec_tty ec_tty_t; /**< \see ec_tty */

/******************************************************************************
 * Global functions
 *****************************************************************************/

/** Create a virtual TTY interface.
 * 
 * \return Pointer to the interface object, otherwise an ERR_PTR value.
 */
ec_tty_t *ectty_create(void);

/******************************************************************************
 * TTY interface methods
 *****************************************************************************/

/** Releases a virtual TTY interface.
 */
void ectty_free(
        ec_tty_t *tty /**< TTY interface. */
        );

/*****************************************************************************/

/** @} */

#endif
