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

/**
   \file
   EtherCAT SoE state machines.
*/

/*****************************************************************************/

#include "globals.h"
#include "master.h"
#include "mailbox.h"
#include "fsm_soe.h"

/*****************************************************************************/

/** Mailbox type for SoE.
 */
#define EC_MBOX_TYPE_SOE 0x05

/** SoE operations
 */
enum ec_soe_opcodes {
    OPCODE_READ_REQUEST   = 0x01, /**< Read request. */
    OPCODE_READ_RESPONSE  = 0x02, /**< Read response. */
    OPCODE_WRITE_REQUEST  = 0x03, /**< Write request. */
    OPCODE_WRITE_RESPONSE = 0x04  /**< Write response. */
};

/** Size of all SoE headers.
 */
#define EC_SOE_SIZE 0x04

/** SoE response timeout [ms].
 */
#define EC_SOE_RESPONSE_TIMEOUT 1000

/*****************************************************************************/

void ec_fsm_soe_read_start(ec_fsm_soe_t *);
void ec_fsm_soe_read_request(ec_fsm_soe_t *);
void ec_fsm_soe_read_check(ec_fsm_soe_t *);
void ec_fsm_soe_read_response(ec_fsm_soe_t *);

void ec_fsm_soe_write_start(ec_fsm_soe_t *);
void ec_fsm_soe_write_request(ec_fsm_soe_t *);
void ec_fsm_soe_write_check(ec_fsm_soe_t *);
void ec_fsm_soe_write_response(ec_fsm_soe_t *);

void ec_fsm_soe_end(ec_fsm_soe_t *);
void ec_fsm_soe_error(ec_fsm_soe_t *);

/*****************************************************************************/

extern const ec_code_msg_t soe_error_codes[];

/*****************************************************************************/

/** Outputs an SoE error code.
*/
void ec_print_soe_error(uint16_t error_code)
{
    const ec_code_msg_t *error_msg;

    for (error_msg = soe_error_codes; error_msg->code; error_msg++) {
        if (error_msg->code == error_code) {
            EC_ERR("SoE error 0x%04X: \"%s\".\n",
                   error_msg->code, error_msg->message);
            return;
        }
    }

    EC_ERR("Unknown SoE error 0x%04X.\n", error_code);
}

/*****************************************************************************/

/** Constructor.
 */
void ec_fsm_soe_init(
        ec_fsm_soe_t *fsm, /**< finite state machine */
        ec_datagram_t *datagram /**< datagram */
        )
{
    fsm->state = NULL;
    fsm->datagram = datagram;
}

/*****************************************************************************/

/** Destructor.
 */
void ec_fsm_soe_clear(
        ec_fsm_soe_t *fsm /**< finite state machine */
        )
{
}

/*****************************************************************************/

/** Starts to transfer an IDN to/from a slave.
 */
void ec_fsm_soe_transfer(
        ec_fsm_soe_t *fsm, /**< State machine. */
        ec_slave_t *slave, /**< EtherCAT slave. */
        ec_soe_request_t *request /**< SoE request. */
        )
{
    fsm->slave = slave;
    fsm->request = request;
    if (request->dir == EC_DIR_OUTPUT) {
        fsm->state = ec_fsm_soe_write_start;
    } else {
        fsm->state = ec_fsm_soe_read_start;
    }
}

/*****************************************************************************/

/**
   Executes the current state of the state machine.
   \return false, if state machine has terminated
*/

int ec_fsm_soe_exec(ec_fsm_soe_t *fsm /**< finite state machine */)
{
    fsm->state(fsm);

    return fsm->state != ec_fsm_soe_end && fsm->state != ec_fsm_soe_error;
}

/*****************************************************************************/

/**
   Returns, if the state machine terminated with success.
   \return non-zero if successful.
*/

int ec_fsm_soe_success(ec_fsm_soe_t *fsm /**< Finite state machine */)
{
    return fsm->state == ec_fsm_soe_end;
}

/*****************************************************************************/

/** Output information about a failed SoE transfer.
 */
void ec_fsm_soe_print_error(ec_fsm_soe_t *fsm /**< Finite state machine */)
{
    ec_soe_request_t *request = fsm->request;

    EC_ERR("");

    if (request->dir == EC_DIR_OUTPUT) {
        printk("Writing");
    } else {
        printk("Reading");
    }

    printk(" IDN 0x%04X failed on slave %u.\n",
            request->idn, fsm->slave->ring_position);
}

/******************************************************************************
 * SoE read state machine
 *****************************************************************************/

/** SoE state: READ START.
 */
void ec_fsm_soe_read_start(ec_fsm_soe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = slave->master;
    ec_soe_request_t *request = fsm->request;
    uint8_t *data;

    if (master->debug_level)
        EC_DBG("Reading IDN 0x%04X from slave %u.\n",
               request->idn, slave->ring_position);

    if (!(slave->sii.mailbox_protocols & EC_MBOX_SOE)) {
        EC_ERR("Slave does not support SoE!\n");
        fsm->state = ec_fsm_soe_error;
        ec_fsm_soe_print_error(fsm);
        return;
    }

    data = ec_slave_mbox_prepare_send(slave, datagram, EC_MBOX_TYPE_SOE,
            EC_SOE_SIZE);
    if (IS_ERR(data)) {
        fsm->state = ec_fsm_soe_error;
        ec_fsm_soe_print_error(fsm);
        return;
    }

    EC_WRITE_U8(data, OPCODE_READ_REQUEST);
    EC_WRITE_U8(data + 1, 1 << 6); // request value
    EC_WRITE_U16(data + 2, request->idn);

    if (master->debug_level) {
        EC_DBG("SCC read request:\n");
        ec_print_data(data, EC_SOE_SIZE);
    }

    fsm->request->data_size = 0;
    fsm->request->jiffies_sent = jiffies;
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_soe_read_request;
}

/*****************************************************************************/

/** SoE state: READ REQUEST.
 */
void ec_fsm_soe_read_request(ec_fsm_soe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    unsigned long diff_ms;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return; // FIXME: check for response first?

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_soe_error;
        EC_ERR("Failed to receive SoE read request: ");
        ec_datagram_print_state(datagram);
        ec_fsm_soe_print_error(fsm);
        return;
    }

    diff_ms = (jiffies - fsm->request->jiffies_sent) * 1000 / HZ;

    if (datagram->working_counter != 1) {
        if (!datagram->working_counter) {
            if (diff_ms < EC_SOE_RESPONSE_TIMEOUT) {
                // no response; send request datagram again
                return;
            }
        }
        fsm->state = ec_fsm_soe_error;
        EC_ERR("Reception of SoE read request failed after %u ms: ",
                (u32) diff_ms);
        ec_datagram_print_wc_error(datagram);
        ec_fsm_soe_print_error(fsm);
        return;
    }

    fsm->jiffies_start = datagram->jiffies_sent;
    ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_soe_read_check;
}

/*****************************************************************************/

/** CoE state: READ CHECK.
 */
void ec_fsm_soe_read_check(ec_fsm_soe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_soe_error;
        EC_ERR("Failed to receive SoE mailbox check datagram: ");
        ec_datagram_print_state(datagram);
        ec_fsm_soe_print_error(fsm);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_soe_error;
        EC_ERR("Reception of SoE mailbox check datagram failed: ");
        ec_datagram_print_wc_error(datagram);
        ec_fsm_soe_print_error(fsm);
        return;
    }

    if (!ec_slave_mbox_check(datagram)) {
        unsigned long diff_ms =
            (datagram->jiffies_received - fsm->jiffies_start) * 1000 / HZ;
        if (diff_ms >= EC_SOE_RESPONSE_TIMEOUT) {
            fsm->state = ec_fsm_soe_error;
            EC_ERR("Timeout after %u ms while waiting for read response.\n",
                    (u32) diff_ms);
            ec_fsm_soe_print_error(fsm);
            return;
        }

        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        return;
    }

    // Fetch response
    ec_slave_mbox_prepare_fetch(slave, datagram); // can not fail.
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_soe_read_response;
}

/*****************************************************************************/

/** SoE state: READ RESPONSE.
 */
void ec_fsm_soe_read_response(ec_fsm_soe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = slave->master;
    uint8_t *data, mbox_prot, header, opcode, incomplete, error_flag,
            value_included;
    size_t rec_size, data_size;
    ec_soe_request_t *req = fsm->request;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return; // FIXME: request again?

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_soe_error;
        EC_ERR("Failed to receive SoE read response datagram: ");
        ec_datagram_print_state(datagram);
        ec_fsm_soe_print_error(fsm);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_soe_error;
        EC_ERR("Reception of SoE read response failed: ");
        ec_datagram_print_wc_error(datagram);
        ec_fsm_soe_print_error(fsm);
        return;
    }

    data = ec_slave_mbox_fetch(slave, datagram, &mbox_prot, &rec_size);
    if (IS_ERR(data)) {
        fsm->state = ec_fsm_soe_error;
        ec_fsm_soe_print_error(fsm);
        return;
    }

    if (master->debug_level) {
        EC_DBG("SCC read response:\n");
        ec_print_data(data, rec_size);
    }

    if (mbox_prot != EC_MBOX_TYPE_SOE) {
        fsm->state = ec_fsm_soe_error;
        EC_ERR("Received mailbox protocol 0x%02X as response.\n", mbox_prot);
        ec_fsm_soe_print_error(fsm);
        return;
    }

    if (rec_size < EC_SOE_SIZE) {
        fsm->state = ec_fsm_soe_error;
        EC_ERR("Received currupted SoE read response"
                " (%zu bytes)!\n", rec_size);
        ec_print_data(data, rec_size);
        ec_fsm_soe_print_error(fsm);
        return;
    }

    header = EC_READ_U8(data);
    opcode = header & 0x7;
    incomplete = (header >> 3) & 1;
    error_flag = (header >> 4) & 1;

    if (opcode != OPCODE_READ_RESPONSE) {
        EC_ERR("Received no read response (opcode %x).\n", opcode);
        ec_print_data(data, rec_size);
        ec_fsm_soe_print_error(fsm);
        fsm->state = ec_fsm_soe_error;
        return;
    }

    if (error_flag) {
        req->error_code = EC_READ_U16(data + rec_size - 2);
        EC_ERR("Received error response:\n");
        ec_print_soe_error(req->error_code);
        ec_fsm_soe_print_error(fsm);
        fsm->state = ec_fsm_soe_error;
        return;
    } else {
        req->error_code = 0x0000;
    }

    value_included = (EC_READ_U8(data + 1) >> 6) & 1;
    if (!value_included) {
        EC_ERR("No value included!\n");
        ec_fsm_soe_print_error(fsm);
        fsm->state = ec_fsm_soe_error;
        return;
    }

    data_size = rec_size - EC_SOE_SIZE;
    if (ec_soe_request_append_data(req,
                data + EC_SOE_SIZE, data_size)) {
        fsm->state = ec_fsm_soe_error;
        ec_fsm_soe_print_error(fsm);
        return;
    }

    if (incomplete) {
        if (master->debug_level) {
            EC_DBG("SoE data incomplete. Waiting for fragment"
                    " at offset %zu.\n", req->data_size);
        }
        fsm->jiffies_start = datagram->jiffies_sent;
        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_soe_read_check;
    } else {
        if (master->debug_level) {
            EC_DBG("IDN data:\n");
            ec_print_data(req->data, req->data_size);
        }

        fsm->state = ec_fsm_soe_end; // success
    }
}

/******************************************************************************
 * SoE write state machine
 *****************************************************************************/

/** Write next fragment.
 */
void ec_fsm_soe_write_next_fragment(
        ec_fsm_soe_t *fsm /**< finite state machine */
        )
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = slave->master;
    ec_soe_request_t *req = fsm->request;
    uint8_t incomplete, *data;
    size_t header_size, max_fragment_size, remaining_size, fragment_size;
    uint16_t fragments_left;

    header_size = EC_MBOX_HEADER_SIZE + EC_SOE_SIZE;
    if (slave->configured_rx_mailbox_size <= header_size) {
        EC_ERR("Mailbox size (%u) too small for SoE write.\n",
                slave->configured_rx_mailbox_size);
        fsm->state = ec_fsm_soe_error;
        ec_fsm_soe_print_error(fsm);
        return;
    }

    remaining_size = req->data_size - fsm->offset;
    max_fragment_size = slave->configured_rx_mailbox_size - header_size;
    incomplete = remaining_size > max_fragment_size;
    fragment_size = incomplete ? max_fragment_size : remaining_size;
    fragments_left = remaining_size / fragment_size - 1;
    if (remaining_size % fragment_size) {
        fragments_left++;
    }

    data = ec_slave_mbox_prepare_send(slave, datagram, EC_MBOX_TYPE_SOE,
            EC_SOE_SIZE + fragment_size);
    if (IS_ERR(data)) {
        fsm->state = ec_fsm_soe_error;
        ec_fsm_soe_print_error(fsm);
        return;
    }

    EC_WRITE_U8(data, OPCODE_WRITE_REQUEST | incomplete << 3);
    EC_WRITE_U8(data + 1, 1 << 6); // only value included
    EC_WRITE_U16(data + 2, incomplete ? fragments_left : req->idn);
    memcpy(data + 4, req->data + fsm->offset, fragment_size);
    fsm->offset += fragment_size;

    if (master->debug_level) {
        EC_DBG("SCC write request:\n");
        ec_print_data(data, EC_SOE_SIZE + fragment_size);
    }

    req->jiffies_sent = jiffies;
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_soe_write_request;
}

/*****************************************************************************/

/** SoE state: WRITE START.
 */
void ec_fsm_soe_write_start(ec_fsm_soe_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = slave->master;
    ec_soe_request_t *req = fsm->request;

    if (master->debug_level)
        EC_DBG("Writing IDN 0x%04X (%zu byte) to slave %u.\n",
               req->idn, req->data_size, slave->ring_position);

    if (!(slave->sii.mailbox_protocols & EC_MBOX_SOE)) {
        EC_ERR("Slave does not support SoE!\n");
        fsm->state = ec_fsm_soe_error;
        ec_fsm_soe_print_error(fsm);
        return;
    }

    fsm->offset = 0;
    ec_fsm_soe_write_next_fragment(fsm);
}

/*****************************************************************************/

/** SoE state: WRITE REQUEST.
 */
void ec_fsm_soe_write_request(ec_fsm_soe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    unsigned long diff_ms;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return; // FIXME: check for response first?

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_soe_error;
        EC_ERR("Failed to receive SoE write request: ");
        ec_datagram_print_state(datagram);
        ec_fsm_soe_print_error(fsm);
        return;
    }

    diff_ms = (jiffies - fsm->request->jiffies_sent) * 1000 / HZ;

    if (datagram->working_counter != 1) {
        if (!datagram->working_counter) {
            if (diff_ms < EC_SOE_RESPONSE_TIMEOUT) {
                // no response; send request datagram again
                return;
            }
        }
        fsm->state = ec_fsm_soe_error;
        EC_ERR("Reception of SoE write request failed after %u ms: ",
                (u32) diff_ms);
        ec_datagram_print_wc_error(datagram);
        ec_fsm_soe_print_error(fsm);
        return;
    }

    fsm->jiffies_start = datagram->jiffies_sent;

    ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_soe_write_check;
}

/*****************************************************************************/

/** CoE state: WRITE CHECK.
 */
void ec_fsm_soe_write_check(ec_fsm_soe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    ec_soe_request_t *req = fsm->request;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_soe_error;
        EC_ERR("Failed to receive SoE write request datagram: ");
        ec_datagram_print_state(datagram);
        ec_fsm_soe_print_error(fsm);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_soe_error;
        EC_ERR("Reception of SoE write request datagram: ");
        ec_datagram_print_wc_error(datagram);
        ec_fsm_soe_print_error(fsm);
        return;
    }

    if (fsm->offset < req->data_size) {
        ec_fsm_soe_write_next_fragment(fsm);
    } else {
        if (!ec_slave_mbox_check(datagram)) {
            unsigned long diff_ms =
                (datagram->jiffies_received - fsm->jiffies_start) * 1000 / HZ;
            if (diff_ms >= EC_SOE_RESPONSE_TIMEOUT) {
                fsm->state = ec_fsm_soe_error;
                EC_ERR("Timeout after %u ms while waiting"
                        " for write response.\n", (u32) diff_ms);
                ec_fsm_soe_print_error(fsm);
                return;
            }

            ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
            fsm->retries = EC_FSM_RETRIES;
            return;
        }

        // Fetch response
        ec_slave_mbox_prepare_fetch(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_soe_write_response;
    }
}

/*****************************************************************************/

/** SoE state: WRITE RESPONSE.
 */
void ec_fsm_soe_write_response(ec_fsm_soe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = slave->master;
    ec_soe_request_t *req = fsm->request;
    uint8_t *data, mbox_prot, opcode, error_flag;
    uint16_t idn;
    size_t rec_size;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return; // FIXME: request again?

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_soe_error;
        EC_ERR("Failed to receive SoE write response datagram: ");
        ec_datagram_print_state(datagram);
        ec_fsm_soe_print_error(fsm);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_soe_error;
        EC_ERR("Reception of SoE write response failed: ");
        ec_datagram_print_wc_error(datagram);
        ec_fsm_soe_print_error(fsm);
        return;
    }

    data = ec_slave_mbox_fetch(slave, datagram, &mbox_prot, &rec_size);
    if (IS_ERR(data)) {
        fsm->state = ec_fsm_soe_error;
        ec_fsm_soe_print_error(fsm);
        return;
    }

    if (master->debug_level) {
        EC_DBG("SCC write response:\n");
        ec_print_data(data, rec_size);
    }

    if (mbox_prot != EC_MBOX_TYPE_SOE) {
        fsm->state = ec_fsm_soe_error;
        EC_ERR("Received mailbox protocol 0x%02X as response.\n", mbox_prot);
        ec_fsm_soe_print_error(fsm);
        return;
    }

    if (rec_size < EC_SOE_SIZE) {
        fsm->state = ec_fsm_soe_error;
        EC_ERR("Received currupted SoE write response (%zu bytes)!\n",
                rec_size);
        ec_print_data(data, rec_size);
        ec_fsm_soe_print_error(fsm);
        return;
    }

    opcode = EC_READ_U8(data) & 0x7;
    if (opcode != OPCODE_WRITE_RESPONSE) {
        EC_ERR("Received no write response (opcode %x).\n", opcode);
        ec_print_data(data, rec_size);
        ec_fsm_soe_print_error(fsm);
        fsm->state = ec_fsm_soe_error;
        return;
    }

    idn = EC_READ_U16(data + 2);
    if (idn != req->idn) {
        EC_ERR("Received response for wrong IDN 0x%04x.\n", idn);
        ec_print_data(data, rec_size);
        ec_fsm_soe_print_error(fsm);
        fsm->state = ec_fsm_soe_error;
        return;
    }

    error_flag = (EC_READ_U8(data) >> 4) & 1;
    if (error_flag) {
        if (rec_size < EC_SOE_SIZE + 2) {
            EC_ERR("Received corrupted error response - error flag set,"
                    " but received size is %zu.\n", rec_size);
        } else {
            req->error_code = EC_READ_U16(data + EC_SOE_SIZE);
            EC_ERR("Received error response:\n");
            ec_print_soe_error(req->error_code);
        }
        ec_print_data(data, rec_size);
        ec_fsm_soe_print_error(fsm);
        fsm->state = ec_fsm_soe_error;
        return;
    } else {
        req->error_code = 0x0000;
    }

    fsm->state = ec_fsm_soe_end; // success
}

/*****************************************************************************/

/** State: ERROR.
 */
void ec_fsm_soe_error(ec_fsm_soe_t *fsm /**< finite state machine */)
{
}

/*****************************************************************************/

/** State: END.
 */
void ec_fsm_soe_end(ec_fsm_soe_t *fsm /**< finite state machine */)
{
}

/*****************************************************************************/
