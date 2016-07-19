/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2016       Gavin Lambert
 *  Copyright (C) 2006-2009  Florian Pose, Ingenieurgemeinschaft IgH
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

#include "include/ecrt.h"

/*****************************************************************************/

struct ec_foe_request {
    ec_foe_request_t *next; /**< List header. */
    ec_slave_config_t *config; /**< Parent slave configuration. */
    unsigned int index; /**< Request index (identifier). */
    uint8_t *data; /**< Pointer to file data. */
    size_t mem_size; /**< Size of file data memory. */
    size_t data_size; /**< Size of file data. */
    size_t progress; /**< Current position of a BUSY request. */
    ec_foe_error_t result; /**< FoE request abort code. Zero on success. */
    uint32_t error_code; /**< Error code from an FoE Error Request. */
};

/*****************************************************************************/

void ec_foe_request_clear(ec_foe_request_t *);

/*****************************************************************************/
