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
   EtherCAT DUMMY domain methods.
*/

/*****************************************************************************/

#include "../master/globals.h"
#include "../master/domain.h"
#include "../master/master.h"

uint8_t *get_dummy_data(void);

/*****************************************************************************/

/** \cond */

int ecrt_domain_register_pdo(
        ec_domain_t *domain, /**< EtherCAT domain */
        ec_slave_t *slave, /**< EtherCAT slave */
        uint16_t pdo_entry_index, /**< PDO entry index */
        uint8_t pdo_entry_subindex, /**< PDO entry subindex */
        void **data_ptr /**< address of the process data pointer */
        )
{
	*data_ptr = get_dummy_data();
	return 0;
}

/*****************************************************************************/

/**
 * Registers a bunch of data fields.
 * \attention The list has to be terminated with a NULL structure ({})!
 * \return 0 in case of success, else < 0
 * \ingroup RealtimeInterface
 */

int ecrt_domain_register_pdo_list(
        ec_domain_t *domain, /**< EtherCAT domain */
        const ec_pdo_reg_t *pdo_regs /**< array of PDO registrations */
        )
{
    const ec_pdo_reg_t *reg;
    
    for (reg = pdo_regs; reg->slave_address; reg++) {
		*(reg->data_ptr) = get_dummy_data();
    }

    return 0;
}

/*****************************************************************************/

/**
 * Registers a PDO range in a domain.
 * \return 0 on success, else non-zero
 * \ingroup RealtimeInterface
 */

int ecrt_domain_register_pdo_range(
        ec_domain_t *domain, /**< EtherCAT domain */
        ec_slave_t *slave, /**< EtherCAT slave */
        ec_direction_t dir, /**< data direction */
        uint16_t offset, /**< offset in slave's PDO range */
        uint16_t length, /**< length of this range */
        void **data_ptr /**< address of the process data pointer */
        )
{
	*data_ptr = get_dummy_data();
    return 0;
}

/*****************************************************************************/

/**
   Processes received process data and requeues the domain datagram(s).
   \ingroup RealtimeInterface
*/

void ecrt_domain_process(ec_domain_t *domain /**< EtherCAT domain */)
{
}

/*****************************************************************************/

/**
   Places all process data datagrams in the masters datagram queue.
   \ingroup RealtimeInterface
*/

void ecrt_domain_queue(ec_domain_t *domain /**< EtherCAT domain */)
{
}

/*****************************************************************************/

/**
   Returns the state of a domain.
   \return 0 if all datagrams were received, else -1.
   \ingroup RealtimeInterface
*/

int ecrt_domain_state(const ec_domain_t *domain /**< EtherCAT domain */)
{
    return 0;
}

/*****************************************************************************/

/** \cond */

EXPORT_SYMBOL(ecrt_domain_register_pdo);
EXPORT_SYMBOL(ecrt_domain_register_pdo_list);
EXPORT_SYMBOL(ecrt_domain_register_pdo_range);
EXPORT_SYMBOL(ecrt_domain_process);
EXPORT_SYMBOL(ecrt_domain_queue);
EXPORT_SYMBOL(ecrt_domain_state);

/** \endcond */

/*****************************************************************************/
