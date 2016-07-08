/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2008  Olav Zarges, imc Messsysteme GmbH
 *                2013  Florian Pose <fp@igh-essen.com>
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
 * EtherCAT FoE state machines.
 */

/*****************************************************************************/

#include "globals.h"
#include "master.h"
#include "mailbox.h"
#include "fsm_foe.h"
#include "foe.h"

/*****************************************************************************/

/** Maximum time in jiffies to wait for responses when reading out the
 * dictionary.
 */
#define EC_FSM_FOE_TIMEOUT_JIFFIES (3 * HZ)

/** Size of the FoE header.
 */
#define EC_FOE_HEADER_SIZE 6
// uint8_t  OpCode
// uint8_t  reserved
// uint32_t PacketNo, Password, ErrorCode

//#define DEBUG_FOE

/*****************************************************************************/

/** FoE OpCodes.
 */
enum {
    EC_FOE_OPCODE_RRQ  = 1, /**< Read request. */
    EC_FOE_OPCODE_WRQ  = 2, /**< Write request. */
    EC_FOE_OPCODE_DATA = 3, /**< Data. */
    EC_FOE_OPCODE_ACK  = 4, /**< Acknowledge. */
    EC_FOE_OPCODE_ERR  = 5, /**< Error. */
    EC_FOE_OPCODE_BUSY = 6  /**< Busy. */
};

/*****************************************************************************/

int ec_foe_prepare_data_send(ec_fsm_foe_t *, ec_datagram_t *);
int ec_foe_prepare_wrq_send(ec_fsm_foe_t *, ec_datagram_t *);
int ec_foe_prepare_rrq_send(ec_fsm_foe_t *, ec_datagram_t *);
int ec_foe_prepare_send_ack(ec_fsm_foe_t *, ec_datagram_t *);

void ec_foe_set_tx_error(ec_fsm_foe_t *, uint32_t);
void ec_foe_set_rx_error(ec_fsm_foe_t *, uint32_t);

void ec_fsm_foe_end(ec_fsm_foe_t *, ec_datagram_t *);
void ec_fsm_foe_error(ec_fsm_foe_t *, ec_datagram_t *);

void ec_fsm_foe_state_wrq_sent(ec_fsm_foe_t *, ec_datagram_t *);
void ec_fsm_foe_state_rrq_sent(ec_fsm_foe_t *, ec_datagram_t *);

void ec_fsm_foe_state_ack_check(ec_fsm_foe_t *, ec_datagram_t *);
void ec_fsm_foe_state_ack_read(ec_fsm_foe_t *, ec_datagram_t *);
void ec_fsm_foe_state_ack_read_data(ec_fsm_foe_t *, ec_datagram_t *);

void ec_fsm_foe_state_data_sent(ec_fsm_foe_t *, ec_datagram_t *);

void ec_fsm_foe_state_data_check(ec_fsm_foe_t *, ec_datagram_t *);
void ec_fsm_foe_state_data_read(ec_fsm_foe_t *, ec_datagram_t *);
void ec_fsm_foe_state_data_read_data(ec_fsm_foe_t *, ec_datagram_t *);
void ec_fsm_foe_state_sent_ack(ec_fsm_foe_t *, ec_datagram_t *);

void ec_fsm_foe_write_start(ec_fsm_foe_t *, ec_datagram_t *);
void ec_fsm_foe_read_start(ec_fsm_foe_t *, ec_datagram_t *);

/*****************************************************************************/

/** Constructor.
 */
void ec_fsm_foe_init(
        ec_fsm_foe_t *fsm /**< finite state machine */
        )
{
    fsm->state = NULL;
    fsm->datagram = NULL;
}

/*****************************************************************************/

/** Destructor.
 */
void ec_fsm_foe_clear(ec_fsm_foe_t *fsm /**< finite state machine */)
{
}

/*****************************************************************************/

/** Executes the current state of the state machine.
 *
 * \return 1 if the state machine is still in progress, else 0.
 */
int ec_fsm_foe_exec(
        ec_fsm_foe_t *fsm, /**< finite state machine */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    if (fsm->state == ec_fsm_foe_end || fsm->state == ec_fsm_foe_error)
        return 0;

    if (fsm->datagram &&
            (fsm->datagram->state == EC_DATAGRAM_INIT ||
             fsm->datagram->state == EC_DATAGRAM_QUEUED ||
             fsm->datagram->state == EC_DATAGRAM_SENT)) {
        // datagram not received yet
        if (datagram != fsm->datagram)
            datagram->state = EC_DATAGRAM_INVALID;
        return 1;
    }

    fsm->state(fsm, datagram);

    if (fsm->state == ec_fsm_foe_end || fsm->state == ec_fsm_foe_error) {
        fsm->datagram = NULL;
        return 0;
    }

    fsm->datagram = datagram;
    return 1;
}

/*****************************************************************************/

/** Returns, if the state machine terminated with success.
 * \return non-zero if successful.
 */
int ec_fsm_foe_success(const ec_fsm_foe_t *fsm /**< Finite state machine */)
{
    return fsm->state == ec_fsm_foe_end;
}

/*****************************************************************************/

/** Prepares an FoE transfer.
 */
void ec_fsm_foe_transfer(
        ec_fsm_foe_t *fsm, /**< State machine. */
        ec_slave_t *slave, /**< EtherCAT slave. */
        ec_foe_request_t *request /**< Sdo request. */
        )
{
    fsm->slave = slave;
    fsm->request = request;

    fsm->buffer_offset = 0;
    if (request->dir == EC_DIR_OUTPUT) {
        fsm->buffer_size = fsm->request->data_size;
        fsm->state = ec_fsm_foe_write_start;
    } else {
        fsm->buffer_size = fsm->request->buffer_size;
        fsm->state = ec_fsm_foe_read_start;
    }
}

/*****************************************************************************/

/** State: ERROR.
 */
void ec_fsm_foe_error(
        ec_fsm_foe_t *fsm, /**< finite state machine */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
#ifdef DEBUG_FOE
    EC_SLAVE_DBG(fsm->slave, 0, "%s()\n", __func__);
#endif
}

/*****************************************************************************/

/** State: END.
 */
void ec_fsm_foe_end(
        ec_fsm_foe_t *fsm, /**< finite state machine */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
#ifdef DEBUG_FOE
    EC_SLAVE_DBG(fsm->slave, 0, "%s()\n", __func__);
#endif
}

/*****************************************************************************/

/** Sends a file or the next fragment.
 *
 * \return Zero on success, otherwise a negative error code.
 */
int ec_foe_prepare_data_send(
        ec_fsm_foe_t *fsm, /**< Finite state machine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    size_t remaining_size, current_size;
    uint8_t *data;

    remaining_size = fsm->buffer_size - fsm->buffer_offset;
    current_size = fsm->slave->configured_tx_mailbox_size
            - EC_MBOX_HEADER_SIZE - EC_FOE_HEADER_SIZE;

    if (remaining_size < current_size) {
        current_size = remaining_size;
        fsm->last_packet = 1;
    }

    data = ec_slave_mbox_prepare_send(fsm->slave,
            datagram, EC_MBOX_TYPE_FOE, current_size + EC_FOE_HEADER_SIZE);
    if (IS_ERR(data)) {
        return -1;
    }

    EC_WRITE_U16(data, EC_FOE_OPCODE_DATA);    // OpCode = DataBlock req.
    EC_WRITE_U32(data + 2, fsm->packet_no);    // PacketNo, Password
#ifdef DEBUG_FOE
    EC_SLAVE_DBG(fsm->slave, 0, "sending opcode %u packet %u\n",
            EC_FOE_OPCODE_DATA, fsm->packet_no);
#endif

    memcpy(data + EC_FOE_HEADER_SIZE,
            fsm->request->buffer + fsm->buffer_offset, current_size);
    fsm->current_size = current_size;

    return 0;
}

/*****************************************************************************/

/** Prepare a write request (WRQ) with filename
 *
 * \return Zero on success, otherwise a negative error code.
 */
int ec_foe_prepare_wrq_send(
        ec_fsm_foe_t *fsm, /**< Finite state machine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    size_t current_size;
    uint8_t *data;

    current_size = strlen(fsm->request->file_name);

    data = ec_slave_mbox_prepare_send(fsm->slave, datagram,
            EC_MBOX_TYPE_FOE, current_size + EC_FOE_HEADER_SIZE);
    if (IS_ERR(data)) {
        return -1;
    }

    EC_WRITE_U16(data, EC_FOE_OPCODE_WRQ); // fsm write request
    EC_WRITE_U32(data + 2, 0); // password
#ifdef DEBUG_FOE
    EC_SLAVE_DBG(fsm->slave, 0, "sending opcode %u\n",
            EC_FOE_OPCODE_WRQ);
#endif

    memcpy(data + EC_FOE_HEADER_SIZE, fsm->request->file_name, current_size);

    return 0;
}

/*****************************************************************************/

/** Initializes the FoE write state machine.
 */
void ec_fsm_foe_write_start(
        ec_fsm_foe_t *fsm, /**< finite state machine */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    ec_slave_t *slave = fsm->slave;

    fsm->buffer_offset = 0;
    fsm->current_size = 0;
    fsm->packet_no = 0;
    fsm->last_packet = 0;

#ifdef DEBUG_FOE
    EC_SLAVE_DBG(fsm->slave, 0, "%s()\n", __func__);
#endif

    if (!slave->sii_image) {
        ec_foe_set_tx_error(fsm, FOE_BUSY);
        EC_SLAVE_ERR(slave, "Slave cannot process FoE write request."
                " SII data not available.\n");
        return;
    }

    if (!(slave->sii_image->sii.mailbox_protocols & EC_MBOX_FOE)) {
        ec_foe_set_tx_error(fsm, FOE_MBOX_PROT_ERROR);
        EC_SLAVE_ERR(slave, "Slave does not support FoE!\n");
        return;
    }

    if (ec_foe_prepare_wrq_send(fsm, datagram)) {
        ec_foe_set_tx_error(fsm, FOE_PROT_ERROR);
        return;
    }

    fsm->state = ec_fsm_foe_state_wrq_sent;
}

/*****************************************************************************/

/** Check for acknowledge.
 */
void ec_fsm_foe_state_ack_check(
        ec_fsm_foe_t *fsm, /**< FoE statemachine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    ec_slave_t *slave = fsm->slave;

#ifdef DEBUG_FOE
    EC_SLAVE_DBG(fsm->slave, 0, "%s()\n", __func__);
#endif

    if (fsm->datagram->state != EC_DATAGRAM_RECEIVED) {
        ec_foe_set_rx_error(fsm, FOE_RECEIVE_ERROR);
        ec_read_mbox_lock_clear(slave);
        EC_SLAVE_ERR(slave, "Failed to receive FoE mailbox check datagram: ");
        ec_datagram_print_state(fsm->datagram);
        return;
    }

    if (fsm->datagram->working_counter != 1) {
        ec_foe_set_rx_error(fsm, FOE_WC_ERROR);
        ec_read_mbox_lock_clear(slave);
        EC_SLAVE_ERR(slave, "Reception of FoE mailbox check datagram"
                " failed: ");
        ec_datagram_print_wc_error(fsm->datagram);
        return;
    }

    if (!ec_slave_mbox_check(fsm->datagram)) {
        // slave did not put anything in the mailbox yet

        // check that data is not already received by another read request
        if (slave->mbox_foe_data.payload_size > 0) {
            ec_read_mbox_lock_clear(slave);
            fsm->state = ec_fsm_foe_state_ack_read_data;
            fsm->state(fsm, datagram);
            return;
        }

        if (time_after(fsm->datagram->jiffies_received,
                    fsm->jiffies_start + EC_FSM_FOE_TIMEOUT_JIFFIES)) {
            ec_foe_set_tx_error(fsm, FOE_TIMEOUT_ERROR);
            ec_read_mbox_lock_clear(slave);
            EC_SLAVE_ERR(slave, "Timeout while waiting for ack response.\n");
            return;
        }

        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        return;
    }

    // Fetch response
    ec_slave_mbox_prepare_fetch(slave, datagram); // can not fail.

    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_foe_state_ack_read;
}

/*****************************************************************************/

/** Acknowledge a read operation.
 */
void ec_fsm_foe_state_ack_read(
        ec_fsm_foe_t *fsm, /**< FoE statemachine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    ec_slave_t *slave = fsm->slave;

#ifdef DEBUG_FOE
    EC_SLAVE_DBG(fsm->slave, 0, "%s()\n", __func__);
#endif

    if (fsm->datagram->state != EC_DATAGRAM_RECEIVED) {
        ec_foe_set_rx_error(fsm, FOE_RECEIVE_ERROR);
        ec_read_mbox_lock_clear(slave);
        EC_SLAVE_ERR(slave, "Failed to receive FoE ack response datagram: ");
        ec_datagram_print_state(fsm->datagram);
        return;
    }

    if (fsm->datagram->working_counter != 1) {
        // only an error if data has not already been read by another read request
        if (slave->mbox_foe_data.payload_size == 0) {
            ec_foe_set_rx_error(fsm, FOE_WC_ERROR);
            ec_read_mbox_lock_clear(slave);
            EC_SLAVE_ERR(slave, "Reception of FoE ack response failed: ");
            ec_datagram_print_wc_error(fsm->datagram);
            return;
        }
    }
    ec_read_mbox_lock_clear(slave);
    fsm->state = ec_fsm_foe_state_ack_read_data;
    fsm->state(fsm, datagram);
}

/*****************************************************************************/

/** Process a read operation.
 */
void ec_fsm_foe_state_ack_read_data(
        ec_fsm_foe_t *fsm, /**< FoE statemachine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    ec_slave_t *slave = fsm->slave;
    uint8_t *data, mbox_prot;
    uint8_t opCode;
    size_t rec_size;

    // process the data available or initiate a new mailbox read check
    if (slave->mbox_foe_data.payload_size > 0) {
        slave->mbox_foe_data.payload_size = 0;
    } else {
        // initiate a new mailbox read check if required data is not available
        if (ec_read_mbox_locked(slave)) {
            // await current read request and mark the datagram as invalid
            datagram->state = EC_DATAGRAM_INVALID;
        } else {
            ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
            fsm->state = ec_fsm_foe_state_ack_check;
        }
        return;
    }

    data = ec_slave_mbox_fetch(slave, &slave->mbox_foe_data, &mbox_prot, &rec_size);
    if (IS_ERR(data)) {
        ec_foe_set_tx_error(fsm, FOE_PROT_ERROR);
        return;
    }

    if (mbox_prot != EC_MBOX_TYPE_FOE) {
        ec_foe_set_tx_error(fsm, FOE_MBOX_PROT_ERROR);
        EC_SLAVE_ERR(slave, "Received mailbox protocol 0x%02X as response.\n",
                mbox_prot);
        return;
    }

    opCode = EC_READ_U8(data);
#ifdef DEBUG_FOE
    EC_SLAVE_DBG(fsm->slave, 0, "received opcode %u\n", opCode);
#endif

    if (opCode == EC_FOE_OPCODE_BUSY) {
        // slave not ready
        if (ec_foe_prepare_data_send(fsm, datagram)) {
            ec_foe_set_tx_error(fsm, FOE_PROT_ERROR);
            EC_SLAVE_ERR(slave, "Slave is busy.\n");
            return;
        }
        fsm->state = ec_fsm_foe_state_data_sent;
        return;
    }

    if (opCode == EC_FOE_OPCODE_ERR) {
        fsm->request->error_code = EC_READ_U32(data + 2);
        EC_SLAVE_ERR(slave, "Received FoE Error Request (code 0x%08x).\n",
                fsm->request->error_code);
        if (rec_size > 6 && data[6]) {
            uint8_t text[256];
            strncpy(text, data + 6, min(rec_size - 6, sizeof(text)));
            text[sizeof(text)-1] = 0;
            EC_SLAVE_ERR(slave, "FoE Error Text: %s\n", text);
        }
        ec_foe_set_tx_error(fsm, FOE_OPCODE_ERROR);
        return;
    }

    if (opCode == EC_FOE_OPCODE_ACK) {
        fsm->packet_no++;
        fsm->buffer_offset += fsm->current_size;

        if (fsm->last_packet) {
            fsm->state = ec_fsm_foe_end;
            return;
        }

        if (ec_foe_prepare_data_send(fsm, datagram)) {
            ec_foe_set_tx_error(fsm, FOE_PROT_ERROR);
            return;
        }
        fsm->state = ec_fsm_foe_state_data_sent;
        return;
    }
    ec_foe_set_tx_error(fsm, FOE_ACK_ERROR);
}

/*****************************************************************************/

/** State: WRQ SENT.
 *
 * Checks is the previous transmit datagram succeded and sends the next
 * fragment, if necessary.
 */
void ec_fsm_foe_state_wrq_sent(
        ec_fsm_foe_t *fsm, /**< FoE statemachine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    ec_slave_t *slave = fsm->slave;

#ifdef DEBUG_FOE
    EC_SLAVE_DBG(fsm->slave, 0, "%s()\n", __func__);
#endif

    if (fsm->datagram->state != EC_DATAGRAM_RECEIVED) {
        ec_foe_set_rx_error(fsm, FOE_RECEIVE_ERROR);
        EC_SLAVE_ERR(slave, "Failed to send FoE WRQ: ");
        ec_datagram_print_state(fsm->datagram);
        return;
    }

    if (fsm->datagram->working_counter != 1) {
        // slave did not put anything in the mailbox yet
        ec_foe_set_rx_error(fsm, FOE_WC_ERROR);
        EC_SLAVE_ERR(slave, "Reception of FoE WRQ failed: ");
        ec_datagram_print_wc_error(fsm->datagram);
        return;
    }

    fsm->jiffies_start = fsm->datagram->jiffies_sent;

    // mailbox read check is skipped if a read request is already ongoing
    if (ec_read_mbox_locked(slave)) {
        fsm->state = ec_fsm_foe_state_ack_read_data;
        // the datagram is not used and marked as invalid
        datagram->state = EC_DATAGRAM_INVALID;
    } else {
        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_foe_state_ack_check;
    }
}

/*****************************************************************************/

/** State: WRQ SENT.
 *
 * Checks is the previous transmit datagram succeded and sends the next
 * fragment, if necessary.
 */
void ec_fsm_foe_state_data_sent(
        ec_fsm_foe_t *fsm, /**< Foe statemachine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    ec_slave_t *slave = fsm->slave;

#ifdef DEBUG_FOE
    EC_SLAVE_DBG(fsm->slave, 0, "%s()\n", __func__);
#endif

    if (fsm->datagram->state != EC_DATAGRAM_RECEIVED) {
        ec_foe_set_tx_error(fsm, FOE_RECEIVE_ERROR);
        EC_SLAVE_ERR(slave, "Failed to receive FoE ack response datagram: ");
        ec_datagram_print_state(fsm->datagram);
        return;
    }

    if (fsm->datagram->working_counter != 1) {
        ec_foe_set_tx_error(fsm, FOE_WC_ERROR);
        EC_SLAVE_ERR(slave, "Reception of FoE data send failed: ");
        ec_datagram_print_wc_error(fsm->datagram);
        return;
    }

    fsm->jiffies_start = jiffies;

    // mailbox read check is skipped if a read request is already ongoing
    if (ec_read_mbox_locked(slave)) {
        fsm->state = ec_fsm_foe_state_ack_read_data;
        // the datagram is not used and marked as invalid
        datagram->state = EC_DATAGRAM_INVALID;
    } else {
        ec_slave_mbox_prepare_check(slave, datagram);
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_foe_state_ack_check;
    }
}

/*****************************************************************************/

/** Prepare a read request (RRQ) with filename
 *
 * \return Zero on success, otherwise a negative error code.
 */
int ec_foe_prepare_rrq_send(
        ec_fsm_foe_t *fsm, /**< Finite state machine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    size_t current_size;
    uint8_t *data;

    current_size = strlen(fsm->request->file_name);

    data = ec_slave_mbox_prepare_send(fsm->slave, datagram,
            EC_MBOX_TYPE_FOE, current_size + EC_FOE_HEADER_SIZE);
    if (IS_ERR(data)) {
        return -1;
    }

    EC_WRITE_U16(data, EC_FOE_OPCODE_RRQ); // fsm read request
    EC_WRITE_U32(data + 2, 0x00000000); // no passwd
    memcpy(data + EC_FOE_HEADER_SIZE, fsm->request->file_name, current_size);
#ifdef DEBUG_FOE
    EC_SLAVE_DBG(fsm->slave, 0, "sending opcode %u\n", EC_FOE_OPCODE_RRQ);
#endif

    if (fsm->slave->master->debug_level) {
        EC_SLAVE_DBG(fsm->slave, 1, "FoE Read Request:\n");
        ec_print_data(data, current_size + EC_FOE_HEADER_SIZE);
    }

    return 0;
}

/*****************************************************************************/

/** Prepare to send an acknowledge.
 *
 * \return Zero on success, otherwise a negative error code.
 */
int ec_foe_prepare_send_ack(
        ec_fsm_foe_t *fsm, /**< FoE statemachine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    uint8_t *data;

    data = ec_slave_mbox_prepare_send(fsm->slave, datagram,
            EC_MBOX_TYPE_FOE, EC_FOE_HEADER_SIZE);
    if (IS_ERR(data)) {
        return -1;
    }

    EC_WRITE_U16(data, EC_FOE_OPCODE_ACK);
    EC_WRITE_U32(data + 2, fsm->packet_no);
#ifdef DEBUG_FOE
    EC_SLAVE_DBG(fsm->slave, 0, "sending opcode %u packet %u\n",
            EC_FOE_OPCODE_ACK, fsm->packet_no);
#endif

    return 0;
}

/*****************************************************************************/

/** State: RRQ SENT.
 *
 * Checks is the previous transmit datagram succeeded and sends the next
 * fragment, if necessary.
 */
void ec_fsm_foe_state_rrq_sent(
        ec_fsm_foe_t *fsm, /**< FoE statemachine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    ec_slave_t *slave = fsm->slave;

#ifdef DEBUG_FOE
    EC_SLAVE_DBG(fsm->slave, 0, "%s()\n", __func__);
#endif

    if (fsm->datagram->state != EC_DATAGRAM_RECEIVED) {
        ec_foe_set_rx_error(fsm, FOE_RECEIVE_ERROR);
        ec_read_mbox_lock_clear(slave);
        EC_SLAVE_ERR(slave, "Failed to send FoE RRQ: ");
        ec_datagram_print_state(fsm->datagram);
        return;
    }

    if (fsm->datagram->working_counter != 1) {
        // slave did not put anything in the mailbox yet
        ec_foe_set_rx_error(fsm, FOE_WC_ERROR);
        ec_read_mbox_lock_clear(slave);
        EC_SLAVE_ERR(slave, "Reception of FoE RRQ failed: ");
        ec_datagram_print_wc_error(fsm->datagram);
        return;
    }

    fsm->jiffies_start = fsm->datagram->jiffies_sent;

    // mailbox read check is skipped if a read request is already ongoing
    if (ec_read_mbox_locked(slave)) {
        fsm->state = ec_fsm_foe_state_data_read_data;
        // the datagram is not used and marked as invalid
        datagram->state = EC_DATAGRAM_INVALID;
    } else {
        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_foe_state_data_check;
    }
}

/*****************************************************************************/

/** Starting state for read operations.
 */
void ec_fsm_foe_read_start(
        ec_fsm_foe_t *fsm, /**< FoE statemachine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    ec_slave_t *slave = fsm->slave;

    fsm->buffer_offset = 0;
    fsm->packet_no = 1;
    fsm->last_packet = 0;

#ifdef DEBUG_FOE
    EC_SLAVE_DBG(fsm->slave, 0, "%s()\n", __func__);
#endif

    if (!slave->sii_image) {
        ec_foe_set_rx_error(fsm, FOE_BUSY);
        EC_SLAVE_ERR(slave, "Slave cannot process FoE read request."
                " SII data not available.\n");
        return;
    }

    if (!(slave->sii_image->sii.mailbox_protocols & EC_MBOX_FOE)) {
        ec_foe_set_rx_error(fsm, FOE_MBOX_PROT_ERROR);
        EC_SLAVE_ERR(slave, "Slave does not support FoE!\n");
        return;
    }

    if (ec_foe_prepare_rrq_send(fsm, datagram)) {
        ec_foe_set_rx_error(fsm, FOE_PROT_ERROR);
        return;
    }

    fsm->state = ec_fsm_foe_state_rrq_sent;
}

/*****************************************************************************/

/** Check for data.
 */
void ec_fsm_foe_state_data_check(
        ec_fsm_foe_t *fsm, /**< FoE statemachine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    ec_slave_t *slave = fsm->slave;

#ifdef DEBUG_FOE
    EC_SLAVE_DBG(fsm->slave, 0, "%s()\n", __func__);
#endif

    if (fsm->datagram->state != EC_DATAGRAM_RECEIVED) {
        ec_foe_set_rx_error(fsm, FOE_RECEIVE_ERROR);
        ec_read_mbox_lock_clear(slave);
        EC_SLAVE_ERR(slave, "Failed to send FoE DATA READ: ");
        ec_datagram_print_state(fsm->datagram);
        return;
    }

    if (fsm->datagram->working_counter != 1) {
        ec_foe_set_rx_error(fsm, FOE_WC_ERROR);
        ec_read_mbox_lock_clear(slave);
        EC_SLAVE_ERR(slave, "Reception of FoE DATA READ: ");
        ec_datagram_print_wc_error(fsm->datagram);
        return;
    }

    if (!ec_slave_mbox_check(fsm->datagram)) {
        // check that data is not already received by another read request
        if (slave->mbox_foe_data.payload_size > 0) {
            ec_read_mbox_lock_clear(slave);
            fsm->state = ec_fsm_foe_state_data_read_data;
            fsm->state(fsm, datagram);
            return;
        }

        if (time_after(fsm->datagram->jiffies_received,
                    fsm->jiffies_start + EC_FSM_FOE_TIMEOUT_JIFFIES)) {
            ec_foe_set_tx_error(fsm, FOE_TIMEOUT_ERROR);
            ec_read_mbox_lock_clear(slave);
            EC_SLAVE_ERR(slave, "Timeout while waiting for ack response.\n");
            return;
        }

        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        return;
    }

    // Fetch response
    ec_slave_mbox_prepare_fetch(slave, datagram); // can not fail.

    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_foe_state_data_read;
}

/*****************************************************************************/

/** Start reading data.
 */
void ec_fsm_foe_state_data_read(
        ec_fsm_foe_t *fsm, /**< FoE statemachine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    ec_slave_t *slave = fsm->slave;

#ifdef DEBUG_FOE
    EC_SLAVE_DBG(fsm->slave, 0, "%s()\n", __func__);
#endif

    if (fsm->datagram->state != EC_DATAGRAM_RECEIVED) {
        ec_foe_set_rx_error(fsm, FOE_RECEIVE_ERROR);
        ec_read_mbox_lock_clear(slave);
        EC_SLAVE_ERR(slave, "Failed to receive FoE DATA READ datagram: ");
        ec_datagram_print_state(fsm->datagram);
        return;
    }

    if (fsm->datagram->working_counter != 1) {
        // only an error if data has not already been read by another read request
        if (slave->mbox_foe_data.payload_size == 0) {
            ec_foe_set_rx_error(fsm, FOE_WC_ERROR);
            ec_read_mbox_lock_clear(slave);
            EC_SLAVE_ERR(slave, "Reception of FoE DATA READ failed: ");
            ec_datagram_print_wc_error(fsm->datagram);
            return;
        }
    }
    ec_read_mbox_lock_clear(slave);
    fsm->state = ec_fsm_foe_state_data_read_data;
    fsm->state(fsm, datagram);
}

/*****************************************************************************/

/** Process a read data operation.
 */
void ec_fsm_foe_state_data_read_data(
        ec_fsm_foe_t *fsm, /**< FoE statemachine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    ec_slave_t *slave = fsm->slave;
    size_t rec_size;
    uint8_t *data, opCode, packet_no, mbox_prot;

    // process the data available or initiate a new mailbox read check
    if (slave->mbox_foe_data.payload_size > 0) {
        slave->mbox_foe_data.payload_size = 0;
    } else {
        // initiate a new mailbox read check if required data is not available
        if (ec_read_mbox_locked(slave)) {
            // await current read request and mark the datagram as invalid
            datagram->state = EC_DATAGRAM_INVALID;
        } else {
            ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
            fsm->state = ec_fsm_foe_state_data_check;
        }
        return;
    }

    data = ec_slave_mbox_fetch(slave, &slave->mbox_foe_data, &mbox_prot, &rec_size);
    if (IS_ERR(data)) {
        ec_foe_set_rx_error(fsm, FOE_MBOX_FETCH_ERROR);
        return;
    }

    if (mbox_prot != EC_MBOX_TYPE_FOE) {
        EC_SLAVE_ERR(slave, "Received mailbox protocol 0x%02X as response.\n",
                mbox_prot);
        ec_foe_set_rx_error(fsm, FOE_PROT_ERROR);
        return;
    }

    opCode = EC_READ_U8(data);
#ifdef DEBUG_FOE
    EC_SLAVE_DBG(fsm->slave, 0, "received opcode %u\n", opCode);
#endif

    if (opCode == EC_FOE_OPCODE_BUSY) {
        fsm->packet_no--;
        if (ec_foe_prepare_send_ack(fsm, datagram)) {
            ec_foe_set_rx_error(fsm, FOE_PROT_ERROR);
        } else {
            fsm->state = ec_fsm_foe_state_sent_ack;
        }
#ifdef DEBUG_FOE
        EC_SLAVE_DBG(fsm->slave, 0, "%s() busy. Next pkt %u\n", __func__,
                fsm->packet_no);
#endif
        return;
    }

    if (opCode == EC_FOE_OPCODE_ERR) {
        fsm->request->error_code = EC_READ_U32(data + 2);
        EC_SLAVE_ERR(slave, "Received FoE Error Request (code 0x%08x).\n",
                fsm->request->error_code);
        if (rec_size > 6 && data[6]) {
            uint8_t text[256];
            strncpy(text, data + 6, min(rec_size - 6, sizeof(text)));
            text[sizeof(text)-1] = 0;
            EC_SLAVE_ERR(slave, "FoE Error Text: %s\n", text);
        }
        ec_foe_set_rx_error(fsm, FOE_OPCODE_ERROR);
        return;
    }

    if (opCode != EC_FOE_OPCODE_DATA) {
        EC_SLAVE_ERR(slave, "Received OPCODE %x, expected %x.\n",
                opCode, EC_FOE_OPCODE_DATA);
        fsm->request->error_code = 0x00000000;
        ec_foe_set_rx_error(fsm, FOE_OPCODE_ERROR);
        return;
    }

    packet_no = EC_READ_U32(data + 2);
    if (packet_no != fsm->packet_no) {
        EC_SLAVE_ERR(slave, "Received packet number %u, expected %u.\n",
                packet_no, fsm->packet_no);
        ec_foe_set_rx_error(fsm, FOE_PACKETNO_ERROR);
        return;
    }

    rec_size -= EC_FOE_HEADER_SIZE;

    if (fsm->buffer_size >= fsm->buffer_offset + rec_size) {
        memcpy(fsm->request->buffer + fsm->buffer_offset,
                data + EC_FOE_HEADER_SIZE, rec_size);
        fsm->buffer_offset += rec_size;
    }

    fsm->last_packet =
        (rec_size + EC_MBOX_HEADER_SIZE + EC_FOE_HEADER_SIZE
         != slave->configured_rx_mailbox_size);

    if (fsm->last_packet ||
            (slave->configured_rx_mailbox_size - EC_MBOX_HEADER_SIZE
             - EC_FOE_HEADER_SIZE + fsm->buffer_offset)
            <= fsm->buffer_size) {
        // either it was the last packet or a new packet will fit into the
        // delivered buffer
#ifdef DEBUG_FOE
        EC_SLAVE_DBG(fsm->slave, 0, "last_packet=true\n");
#endif
        if (ec_foe_prepare_send_ack(fsm, datagram)) {
            ec_foe_set_rx_error(fsm, FOE_RX_DATA_ACK_ERROR);
            return;
        }

        fsm->state = ec_fsm_foe_state_sent_ack;
    }
    else {
        // no more data fits into the delivered buffer
        // ... wait for new read request
        EC_SLAVE_ERR(slave, "Data do not fit in receive buffer!\n");
        printk("  rx_buffer_size = %d\n", fsm->buffer_size);
        printk("rx_buffer_offset = %d\n", fsm->buffer_offset);
        printk("        rec_size = %zd\n", rec_size);
        printk(" rx_mailbox_size = %d\n", slave->configured_rx_mailbox_size);
        printk("  rx_last_packet = %d\n", fsm->last_packet);
        fsm->request->data_size = fsm->buffer_offset;
        ec_foe_set_rx_error(fsm, FOE_READ_OVER_ERROR);
    }
}

/*****************************************************************************/

/** Sent an acknowledge.
 */
void ec_fsm_foe_state_sent_ack(
        ec_fsm_foe_t *fsm, /**< FoE statemachine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    ec_slave_t *slave = fsm->slave;

#ifdef DEBUG_FOE
    EC_SLAVE_DBG(fsm->slave, 0, "%s()\n", __func__);
#endif

    if (fsm->datagram->state != EC_DATAGRAM_RECEIVED) {
        ec_foe_set_rx_error(fsm, FOE_RECEIVE_ERROR);
        EC_SLAVE_ERR(slave, "Failed to send FoE ACK: ");
        ec_datagram_print_state(fsm->datagram);
        return;
    }

    if (fsm->datagram->working_counter != 1) {
        // slave did not put anything into the mailbox yet
        ec_foe_set_rx_error(fsm, FOE_WC_ERROR);
        EC_SLAVE_ERR(slave, "Reception of FoE ACK failed: ");
        ec_datagram_print_wc_error(fsm->datagram);
        return;
    }

    fsm->jiffies_start = fsm->datagram->jiffies_sent;

    if (fsm->last_packet) {
        fsm->packet_no = 0;
        fsm->request->data_size = fsm->buffer_offset;
        fsm->state = ec_fsm_foe_end;
    }
    else {
        fsm->packet_no++;

        // mailbox read check is skipped if a read request is already ongoing
        if (ec_read_mbox_locked(slave)) {
            fsm->state = ec_fsm_foe_state_data_read_data;
            // the datagram is not used and marked as invalid
            datagram->state = EC_DATAGRAM_INVALID;
        } else {
            ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
            fsm->retries = EC_FSM_RETRIES;
            fsm->state = ec_fsm_foe_state_data_check;
        }
    }
}

/*****************************************************************************/

/** Set an error code and go to the send error state.
 */
void ec_foe_set_tx_error(
        ec_fsm_foe_t *fsm, /**< FoE statemachine. */
        uint32_t errorcode /**< FoE error code. */
        )
{
    fsm->request->result = errorcode;
    fsm->state = ec_fsm_foe_error;
}

/*****************************************************************************/

/** Set an error code and go to the receive error state.
 */
void ec_foe_set_rx_error(
        ec_fsm_foe_t *fsm, /**< FoE statemachine. */
        uint32_t errorcode /**< FoE error code. */
        )
{
    fsm->request->result = errorcode;
    fsm->state = ec_fsm_foe_error;
}

/*****************************************************************************/
