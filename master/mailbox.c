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
   Processes received mailbox data.
   \return pointer to the received data
*/

uint8_t *ec_slave_mbox_fetch(const ec_slave_t *slave, /**< slave */
                             ec_datagram_t *datagram, /**< datagram */
                             uint8_t type, /**< expected mailbox protocol */
                             size_t *size /**< size of the received data */
                             )
{
    size_t data_size;

    if ((EC_READ_U8(datagram->data + 5) & 0x0F) != type) {
        EC_ERR("Unexpected mailbox protocol 0x%02X (exp.: 0x%02X) at"
               " slave %i!\n", EC_READ_U8(datagram->data + 5), type,
               slave->ring_position);
        return NULL;
    }

    if ((data_size = EC_READ_U16(datagram->data)) >
        slave->sii_tx_mailbox_size - 6) {
        EC_ERR("Currupt mailbox response detected!\n");
        return NULL;
    }

    *size = data_size;
    return datagram->data + 6;
}

/*****************************************************************************/

/**
   Sends a mailbox datagram and waits for its reception.
   \return pointer to the received data
*/

uint8_t *ec_slave_mbox_simple_io(const ec_slave_t *slave, /**< slave */
                                 ec_datagram_t *datagram, /**< datagram */
                                 size_t *size /**< size of the received data */
                                 )
{
    uint8_t type;

    type = EC_READ_U8(datagram->data + 5);

    if (unlikely(ec_master_simple_io(slave->master, datagram))) {
        EC_ERR("Mailbox checking failed on slave %i!\n",
               slave->ring_position);
        return NULL;
    }

    return ec_slave_mbox_simple_receive(slave, datagram, type, size);
}

/*****************************************************************************/

/**
   Waits for the reception of a mailbox datagram.
   \return pointer to the received data
*/

uint8_t *ec_slave_mbox_simple_receive(const ec_slave_t *slave, /**< slave */
                                      ec_datagram_t *datagram, /**< datagram */
                                      uint8_t type, /**< expected protocol */
                                      size_t *size /**< received data size */
                                      )
{
    cycles_t start, end, timeout;

    start = get_cycles();
    timeout = (cycles_t) 100 * cpu_khz; // 100ms

    while (1)
    {
        if (ec_slave_mbox_prepare_check(slave, datagram)) return NULL;
        if (unlikely(ec_master_simple_io(slave->master, datagram))) {
            EC_ERR("Mailbox checking failed on slave %i!\n",
                   slave->ring_position);
            return NULL;
        }

        end = get_cycles();

        if (ec_slave_mbox_check(datagram))
            break; // proceed with receiving data

        if ((end - start) >= timeout) {
            EC_ERR("Mailbox check - Slave %i timed out.\n",
                   slave->ring_position);
            return NULL;
        }

        udelay(100);
    }

    if (ec_slave_mbox_prepare_fetch(slave, datagram)) return NULL;
    if (unlikely(ec_master_simple_io(slave->master, datagram))) {
        EC_ERR("Mailbox receiving failed on slave %i!\n",
               slave->ring_position);
        return NULL;
    }

    if (unlikely(slave->master->debug_level) > 1)
        EC_DBG("Mailbox receive took %ius.\n",
               ((unsigned int) (end - start) * 1000 / cpu_khz));

    return ec_slave_mbox_fetch(slave, datagram, type, size);
}

/*****************************************************************************/
