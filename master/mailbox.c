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
   Mailbox functionality.
*/

/*****************************************************************************/

#include <linux/slab.h>
#include <linux/delay.h>

#include "mailbox.h"
#include "datagram.h"
#include "master.h"

/*****************************************************************************/

/**
   Prepares a mailbox-send datagram.
   \return pointer to mailbox datagram data
*/

uint8_t *ec_slave_mbox_prepare_send(const ec_slave_t *slave, /**< slave */
                                    ec_datagram_t *datagram, /**< datagram */
                                    uint8_t type, /**< mailbox protocol */
                                    size_t size /**< size of the data */
                                    )
{
    size_t total_size;

    if (unlikely(!slave->sii_mailbox_protocols)) {
        EC_ERR("Slave %i does not support mailbox communication!\n",
               slave->ring_position);
        return NULL;
    }

    total_size = size + 6;
    if (unlikely(total_size > slave->sii_rx_mailbox_size)) {
        EC_ERR("Data size does not fit in mailbox!\n");
        return NULL;
    }

    if (ec_datagram_npwr(datagram, slave->station_address,
                         slave->sii_rx_mailbox_offset,
                         slave->sii_rx_mailbox_size))
        return NULL;

    EC_WRITE_U16(datagram->data,     size); // mailbox service data length
    EC_WRITE_U16(datagram->data + 2, slave->station_address); // station addr.
    EC_WRITE_U8 (datagram->data + 4, 0x00); // channel & priority
    EC_WRITE_U8 (datagram->data + 5, type); // underlying protocol type

    return datagram->data + 6;
}

/*****************************************************************************/

/**
   Prepares a datagram for checking the mailbox state.
   \return 0 in case of success, else < 0
*/

int ec_slave_mbox_prepare_check(const ec_slave_t *slave, /**< slave */
                                ec_datagram_t *datagram /**< datagram */
                                )
{
    // FIXME: second sync manager?
    if (ec_datagram_nprd(datagram, slave->station_address, 0x808, 8))
        return -1;

    return 0;
}

/*****************************************************************************/

/**
   Processes a mailbox state checking datagram.
   \return 0 in case of success, else < 0
*/

int ec_slave_mbox_check(const ec_datagram_t *datagram /**< datagram */)
{
    return EC_READ_U8(datagram->data + 5) & 8 ? 1 : 0;
}

/*****************************************************************************/

/**
   Prepares a datagram to fetch mailbox data.
   \return 0 in case of success, else < 0
*/

int ec_slave_mbox_prepare_fetch(const ec_slave_t *slave, /**< slave */
                                ec_datagram_t *datagram /**< datagram */
                                )
{
    if (ec_datagram_nprd(datagram, slave->station_address,
                         slave->sii_tx_mailbox_offset,
                         slave->sii_tx_mailbox_size)) return -1;
    return 0;
}

/*****************************************************************************/

/**
   Mailbox error codes.
*/

const ec_code_msg_t mbox_error_messages[] = {
    {0x00000001, "MBXERR_SYNTAX"},
    {0x00000002, "MBXERR_UNSUPPORTEDPROTOCOL"},
    {0x00000003, "MBXERR_INVAILDCHANNEL"},
    {0x00000004, "MBXERR_SERVICENOTSUPPORTED"},
    {0x00000005, "MBXERR_INVALIDHEADER"},
    {0x00000006, "MBXERR_SIZETOOSHORT"},
    {0x00000007, "MBXERR_NOMOREMEMORY"},
    {0x00000008, "MBXERR_INVALIDSIZE"},
    {}
};

/*****************************************************************************/

/**
   Processes received mailbox data.
   \return pointer to the received data
*/

uint8_t *ec_slave_mbox_fetch(const ec_slave_t *slave, /**< slave */
                             ec_datagram_t *datagram, /**< datagram */
                             uint8_t *type, /**< expected mailbox protocol */
                             size_t *size /**< size of the received data */
                             )
{
    size_t data_size;

    if ((data_size = EC_READ_U16(datagram->data)) >
        slave->sii_tx_mailbox_size - 6) {
        EC_ERR("Corrupt mailbox response detected!\n");
        return NULL;
    }

    *type = EC_READ_U8(datagram->data + 5) & 0x0F;
    *size = data_size;

    if (*type == 0x00) {
        const ec_code_msg_t *mbox_msg;
	uint16_t code = EC_READ_U16(datagram->data + 8);

        EC_ERR("Mailbox error response received.\n");
	for (mbox_msg = mbox_error_messages; mbox_msg->code; mbox_msg++) {
            if (mbox_msg->code != code) continue;
            EC_ERR("Error reply code: 0x%04X: \"%s\".\n",
                   mbox_msg->code, mbox_msg->message);
            break;
        }

        if (!mbox_msg->code)
            EC_ERR("Unknown error reply code 0x%04X.\n", code);

        return NULL;
    }

    return datagram->data + 6;
}

/*****************************************************************************/
