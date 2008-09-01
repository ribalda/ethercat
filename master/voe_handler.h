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
   Vendor-specific-over-EtherCAT protocol handler.
*/

/*****************************************************************************/

#ifndef __EC_VOE_HANDLER_H__
#define __EC_VOE_HANDLER_H__

#include <linux/list.h>

#include "globals.h"
#include "datagram.h"

/*****************************************************************************/

/** Vendor-specific-over-EtherCAT handler.
 */
struct ec_voe_handler {
    struct list_head list; /**< List item. */
    ec_slave_config_t *config; /**< Parent slave configuration. */
    ec_datagram_t datagram; /**< State machine datagram. */
    size_t data_size; /**< Size of Sdo data. */
    ec_direction_t dir; /**< Direction. EC_DIR_OUTPUT means writing to
                          the slave, EC_DIR_INPUT means reading from the
                          slave. */
    void (*state)(ec_voe_handler_t *); /**< State function */
    ec_internal_request_state_t request_state; /**< Handler state. */
    unsigned int retries; /**< retries upon datagram timeout */
    unsigned long jiffies_start; /**< Timestamp for timeout calculation. */
};

/*****************************************************************************/

int ec_voe_handler_init(ec_voe_handler_t *, ec_slave_config_t *, size_t);
void ec_voe_handler_clear(ec_voe_handler_t *);

/*****************************************************************************/

#endif
