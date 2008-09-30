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

/** \file
 * Vendor-specific-over-EtherCAT protocol handler functions.
 */

/*****************************************************************************/

#include <linux/module.h>

#include "master.h"
#include "slave_config.h"
#include "mailbox.h"
#include "voe_handler.h"

/** VoE mailbox type.
 */
#define EC_MBOX_TYPE_VOE 0x0f

/** VoE header size.
 */
#define EC_VOE_HEADER_SIZE 6

/** VoE response timeout in [ms].
 */
#define EC_VOE_RESPONSE_TIMEOUT 500

/*****************************************************************************/

void ec_voe_handler_clear_data(ec_voe_handler_t *);

void ec_voe_handler_state_write_start(ec_voe_handler_t *);
void ec_voe_handler_state_write_response(ec_voe_handler_t *);

void ec_voe_handler_state_read_start(ec_voe_handler_t *);
void ec_voe_handler_state_read_check(ec_voe_handler_t *);
void ec_voe_handler_state_read_response(ec_voe_handler_t *);

void ec_voe_handler_state_end(ec_voe_handler_t *);
void ec_voe_handler_state_error(ec_voe_handler_t *);

/*****************************************************************************/

/** VoE handler constructor.
 */
int ec_voe_handler_init(
        ec_voe_handler_t *voe, /**< VoE handler. */
        ec_slave_config_t *sc, /**< Parent slave configuration. */
        size_t size /**< Size of memory to reserve. */
        )
{
    voe->config = sc;
    voe->vendor_id = 0x00000000;
    voe->vendor_type = 0x0000;
    voe->data_size = 0;
    voe->dir = EC_DIR_INVALID;
    voe->state = ec_voe_handler_state_error;
    voe->request_state = EC_INT_REQUEST_INIT;

    ec_datagram_init(&voe->datagram);
    if (ec_datagram_prealloc(&voe->datagram,
                size + EC_MBOX_HEADER_SIZE + EC_VOE_HEADER_SIZE))
        return -1;

    return 0;
}

/*****************************************************************************/

/** VoE handler destructor.
 */
void ec_voe_handler_clear(
        ec_voe_handler_t *voe /**< VoE handler. */
        )
{
    ec_datagram_clear(&voe->datagram);
}

/*****************************************************************************
 * Application interface.
 ****************************************************************************/

void ecrt_voe_handler_send_header(ec_voe_handler_t *voe, uint32_t vendor_id,
        uint16_t vendor_type)
{
    voe->vendor_id = vendor_id;
    voe->vendor_type = vendor_type;
}

/*****************************************************************************/

void ecrt_voe_handler_received_header(const ec_voe_handler_t *voe,
        uint32_t *vendor_id, uint16_t *vendor_type)
{
    uint8_t *header = voe->datagram.data + EC_MBOX_HEADER_SIZE;

    if (vendor_id)
        *vendor_id = EC_READ_U32(header);
    if (vendor_type)
        *vendor_type = EC_READ_U16(header + 4);
}

/*****************************************************************************/

uint8_t *ecrt_voe_handler_data(ec_voe_handler_t *voe)
{
    return voe->datagram.data + EC_MBOX_HEADER_SIZE + EC_VOE_HEADER_SIZE;
}

/*****************************************************************************/

size_t ecrt_voe_handler_data_size(const ec_voe_handler_t *voe)
{
    return voe->data_size;
}

/*****************************************************************************/

void ecrt_voe_handler_read(ec_voe_handler_t *voe)
{
    voe->dir = EC_DIR_INPUT;
    voe->state = ec_voe_handler_state_read_start;
    voe->request_state = EC_INT_REQUEST_QUEUED;
}

/*****************************************************************************/

void ecrt_voe_handler_write(ec_voe_handler_t *voe, size_t size)
{
    voe->dir = EC_DIR_OUTPUT;
    voe->data_size = size;
    voe->state = ec_voe_handler_state_write_start;
    voe->request_state = EC_INT_REQUEST_QUEUED;
}

/*****************************************************************************/

ec_request_state_t ecrt_voe_handler_execute(ec_voe_handler_t *voe)
{
    if (voe->config->slave) {
        voe->state(voe);
        if (voe->request_state == EC_REQUEST_BUSY)
            ec_master_queue_datagram(voe->config->master, &voe->datagram);
    } else {
        voe->state = ec_voe_handler_state_error;
        voe->request_state = EC_INT_REQUEST_FAILURE;
    }

    return ec_request_state_translation_table[voe->request_state];
}

/******************************************************************************
 * State functions.
 *****************************************************************************/

void ec_voe_handler_state_write_start(ec_voe_handler_t *voe)
{
    ec_slave_t *slave = voe->config->slave;
    uint8_t *data;

    if (slave->master->debug_level) {
        EC_DBG("Writing %u bytes of VoE data to slave %u.\n",
               voe->data_size, slave->ring_position);
        ec_print_data(ecrt_voe_handler_data(voe), voe->data_size);
    }

    if (!(slave->sii.mailbox_protocols & EC_MBOX_VOE)) {
        EC_ERR("Slave %u does not support VoE!\n", slave->ring_position);
        voe->state = ec_voe_handler_state_error;
        voe->request_state = EC_INT_REQUEST_FAILURE;
        return;
    }
	
    if (!(data = ec_slave_mbox_prepare_send(slave, &voe->datagram,
                    EC_MBOX_TYPE_VOE, EC_VOE_HEADER_SIZE + voe->data_size))) {
        voe->state = ec_voe_handler_state_error;
        voe->request_state = EC_INT_REQUEST_FAILURE;
        return;
    }

    EC_WRITE_U32(data,     voe->vendor_id);
    EC_WRITE_U16(data + 4, voe->vendor_type);

    voe->retries = EC_FSM_RETRIES;
    voe->jiffies_start = jiffies;
    voe->state = ec_voe_handler_state_write_response;
}

/*****************************************************************************/

void ec_voe_handler_state_write_response(ec_voe_handler_t *voe)
{
    ec_datagram_t *datagram = &voe->datagram;
    ec_slave_t *slave = voe->config->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && voe->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        voe->state = ec_voe_handler_state_error;
        voe->request_state = EC_INT_REQUEST_FAILURE;
        EC_ERR("Failed to receive VoE write request datagram for"
               " slave %u (datagram state %u).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        if (!datagram->working_counter) {
            unsigned long diff_ms =
                (jiffies - voe->jiffies_start) * 1000 / HZ;
            if (diff_ms < EC_VOE_RESPONSE_TIMEOUT) {
                if (slave->master->debug_level) {
                    EC_DBG("Slave %u did not respond to VoE write request. "
                            "Retrying after %u ms...\n",
                            slave->ring_position, (u32) diff_ms);
                }
                // no response; send request datagram again
                return;
            }
        }
        voe->state = ec_voe_handler_state_error;
        voe->request_state = EC_INT_REQUEST_FAILURE;
        EC_ERR("Reception of VoE write request failed on slave %u: ",
                slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    if (voe->config->master->debug_level)
        EC_DBG("VoE write request successful.\n");

    voe->request_state = EC_INT_REQUEST_SUCCESS;
    voe->state = ec_voe_handler_state_end;
}

/*****************************************************************************/

void ec_voe_handler_state_read_start(ec_voe_handler_t *voe)
{
    ec_datagram_t *datagram = &voe->datagram;
    ec_slave_t *slave = voe->config->slave;

    if (slave->master->debug_level)
        EC_DBG("Reading VoE data to slave %u.\n", slave->ring_position);

    if (!(slave->sii.mailbox_protocols & EC_MBOX_VOE)) {
        EC_ERR("Slave %u does not support VoE!\n", slave->ring_position);
        voe->state = ec_voe_handler_state_error;
        voe->request_state = EC_INT_REQUEST_FAILURE;
        return;
    }
	
    ec_slave_mbox_prepare_check(slave, datagram); // can not fail.

    voe->jiffies_start = jiffies;
    voe->retries = EC_FSM_RETRIES;
    voe->state = ec_voe_handler_state_read_check;
}

/*****************************************************************************/

void ec_voe_handler_state_read_check(ec_voe_handler_t *voe)
{
    ec_datagram_t *datagram = &voe->datagram;
    ec_slave_t *slave = voe->config->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && voe->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        voe->state = ec_voe_handler_state_error;
        voe->request_state = EC_INT_REQUEST_FAILURE;
        EC_ERR("Failed to receive VoE mailbox check datagram from slave %u"
                " (datagram state %u).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        voe->state = ec_voe_handler_state_error;
        voe->request_state = EC_INT_REQUEST_FAILURE;
        EC_ERR("Reception of VoE mailbox check"
                " datagram failed on slave %u: ", slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    if (!ec_slave_mbox_check(datagram)) {
        unsigned long diff_ms =
            (datagram->jiffies_received - voe->jiffies_start) * 1000 / HZ;
        if (diff_ms >= EC_VOE_RESPONSE_TIMEOUT) {
            voe->state = ec_voe_handler_state_error;
            voe->request_state = EC_INT_REQUEST_FAILURE;
            EC_ERR("Timeout while waiting for VoE data on "
                    "slave %u.\n", slave->ring_position);
            return;
        }

        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        voe->retries = EC_FSM_RETRIES;
        return;
    }

    // Fetch response
    ec_slave_mbox_prepare_fetch(slave, datagram); // can not fail.
    voe->retries = EC_FSM_RETRIES;
    voe->state = ec_voe_handler_state_read_response;
}

/*****************************************************************************/

void ec_voe_handler_state_read_response(ec_voe_handler_t *voe)
{
    ec_datagram_t *datagram = &voe->datagram;
    ec_slave_t *slave = voe->config->slave;
    ec_master_t *master = voe->config->master;
    uint8_t *data, mbox_prot;
    size_t rec_size;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && voe->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        voe->state = ec_voe_handler_state_error;
        voe->request_state = EC_INT_REQUEST_FAILURE;
        EC_ERR("Failed to receive VoE read datagram for"
               " slave %u (datagram state %u).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        voe->state = ec_voe_handler_state_error;
        voe->request_state = EC_INT_REQUEST_FAILURE;
        EC_ERR("Reception of VoE read response failed on slave %u: ",
                slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    if (!(data = ec_slave_mbox_fetch(slave, datagram,
				     &mbox_prot, &rec_size))) {
        voe->state = ec_voe_handler_state_error;
        voe->request_state = EC_INT_REQUEST_FAILURE;
        return;
    }

    if (mbox_prot != EC_MBOX_TYPE_VOE) {
        voe->state = ec_voe_handler_state_error;
        voe->request_state = EC_INT_REQUEST_FAILURE;
        EC_WARN("Received mailbox protocol 0x%02X as response.\n", mbox_prot);
        ec_print_data(data, rec_size);
        return;
    }

    if (rec_size < EC_VOE_HEADER_SIZE) {
        voe->state = ec_voe_handler_state_error;
        voe->request_state = EC_INT_REQUEST_FAILURE;
        EC_ERR("Received VoE header is incomplete (%u bytes)!\n", rec_size);
        return;
    }

    if (master->debug_level) {
        EC_DBG("VoE data:\n");
        ec_print_data(data, rec_size);
    }

    voe->data_size = rec_size - EC_VOE_HEADER_SIZE;
    voe->request_state = EC_INT_REQUEST_SUCCESS;
    voe->state = ec_voe_handler_state_end; // success
}

/*****************************************************************************/

void ec_voe_handler_state_end(ec_voe_handler_t *voe)
{
}

/*****************************************************************************/

void ec_voe_handler_state_error(ec_voe_handler_t *voe)
{
}

/*****************************************************************************/

/** \cond */

EXPORT_SYMBOL(ecrt_voe_handler_send_header);
EXPORT_SYMBOL(ecrt_voe_handler_received_header);
EXPORT_SYMBOL(ecrt_voe_handler_data);
EXPORT_SYMBOL(ecrt_voe_handler_data_size);
EXPORT_SYMBOL(ecrt_voe_handler_read);
EXPORT_SYMBOL(ecrt_voe_handler_write);
EXPORT_SYMBOL(ecrt_voe_handler_execute);

/** \endcond */

/*****************************************************************************/
