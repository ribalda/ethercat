/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2019  Florian Pose, Ingenieurgemeinschaft IgH
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
 * EtherCAT Mailbox Gateway state machine.
 */

/*****************************************************************************/

#include "globals.h"
#include "master.h"
#include "mailbox.h"
#include "fsm_mbox_gateway.h"
#include "slave_config.h"

/*****************************************************************************/

/** Enable debug output for retries.
 */
#define DEBUG_RETRIES 0

/** Enable warning output if transfers take too long.
 */
#define DEBUG_LONG 0

/*****************************************************************************/

void ec_fsm_mbg_start(ec_fsm_mbg_t *, ec_datagram_t *);
void ec_fsm_mbg_request(ec_fsm_mbg_t *, ec_datagram_t *);
void ec_fsm_mbg_check(ec_fsm_mbg_t *, ec_datagram_t *);
void ec_fsm_mbg_response(ec_fsm_mbg_t *, ec_datagram_t *);
void ec_fsm_mbg_response_data(ec_fsm_mbg_t *, ec_datagram_t *);

void ec_fsm_mbg_end(ec_fsm_mbg_t *, ec_datagram_t *);
void ec_fsm_mbg_error(ec_fsm_mbg_t *, ec_datagram_t *);

/*****************************************************************************/

/** Constructor.
 */
void ec_fsm_mbg_init(
        ec_fsm_mbg_t *fsm /**< Finite state machine */
        )
{
    fsm->state = NULL;
    fsm->datagram = NULL;
}

/*****************************************************************************/

/** Destructor.
 */
void ec_fsm_mbg_clear(
        ec_fsm_mbg_t *fsm /**< Finite state machine */
        )
{
}

/*****************************************************************************/

/** Starts to transfer a mailbox gateway request to/from a slave.
 */
void ec_fsm_mbg_transfer(
        ec_fsm_mbg_t *fsm, /**< State machine. */
        ec_slave_t *slave, /**< EtherCAT slave. */
        ec_mbg_request_t *request /**< MBox Gateway request. */
        )
{
    fsm->slave = slave;
    fsm->request = request;

    fsm->state = ec_fsm_mbg_start;
}

/*****************************************************************************/

/** Executes the current state of the state machine.
 *
 * \return 1 if the state machine is still in progress, else 0.
 */
int ec_fsm_mbg_exec(
        ec_fsm_mbg_t *fsm, /**< Finite state machine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    if (fsm->state == ec_fsm_mbg_end || fsm->state == ec_fsm_mbg_error)
        return 0;

    fsm->state(fsm, datagram);

    if (fsm->state == ec_fsm_mbg_end || fsm->state == ec_fsm_mbg_error) {
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
int ec_fsm_mbg_success(
        const ec_fsm_mbg_t *fsm /**< Finite state machine */
        )
{
    return fsm->state == ec_fsm_mbg_end;
}

/******************************************************************************
 *  MBox gateway state machine
 *****************************************************************************/

/** Prepare a request.
 *
 * \return Zero on success, otherwise a positive error code.
 */
int ec_fsm_mbg_prepare_start(
        ec_fsm_mbg_t *fsm, /**< Finite state machine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    u8 *data;
    ec_slave_t *slave = fsm->slave;
    ec_mbg_request_t *request = fsm->request;
    int ret;

    if (slave->configured_rx_mailbox_size <
            request->data_size) {
        EC_SLAVE_ERR(slave, "Mailbox too small!\n");
        request->error_code = EOVERFLOW;
        fsm->state = ec_fsm_mbg_error;
        return request->error_code;
    }

    // configure datagram header
    ret = ec_datagram_fpwr(datagram, slave->station_address,
            slave->configured_rx_mailbox_offset,
            slave->configured_rx_mailbox_size);
    if (ret) {
        request->error_code = ret;
        fsm->state = ec_fsm_mbg_error;
        return request->error_code;
    }
    
    // copy payload
    data = datagram->data;
    memcpy(data, request->data, request->data_size);

    fsm->state = ec_fsm_mbg_request;
    
    return 0;
}

/****************************************************************************/

/** convert mailbox type number to mailbox prototype flag
 *
 * \return 1 on success, otherwise 0 for unknown mailbox type.
 */
int mbox_type_to_prot(uint8_t mbox_type, uint8_t *mbox_prot)
{
    switch (mbox_type)
    {
        case EC_MBOX_TYPE_AOE : { *mbox_prot = EC_MBOX_AOE; } break;
        case EC_MBOX_TYPE_EOE : { *mbox_prot = EC_MBOX_EOE; } break;
        case EC_MBOX_TYPE_COE : { *mbox_prot = EC_MBOX_COE; } break;
        case EC_MBOX_TYPE_FOE : { *mbox_prot = EC_MBOX_FOE; } break;
        case EC_MBOX_TYPE_SOE : { *mbox_prot = EC_MBOX_SOE; } break;
        case EC_MBOX_TYPE_VOE : { *mbox_prot = EC_MBOX_VOE; } break;
        default : {
            *mbox_prot = 0;
            return 0;
        }
    }
  
  return 1;
}

/****************************************************************************/

/** MBox Gateway state: START.
 */
void ec_fsm_mbg_start(
        ec_fsm_mbg_t *fsm, /**< Finite state machine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    ec_slave_t *slave = fsm->slave;
    ec_mbg_request_t *request = fsm->request;
    uint8_t mbox_prot;

    if (fsm->slave->master->debug_level) {
        EC_SLAVE_DBG(slave, 1, "Mailbox Gateway request.\n");
        ec_print_data(request->data, request->data_size);
    }

    if (!slave->sii_image) {
        EC_SLAVE_ERR(slave, "Slave cannot process Mailbox Gateway request."
                " SII data not available.\n");
        request->error_code = EAGAIN;
        fsm->state = ec_fsm_mbg_error;
        return;
    }

    // check protocol type supported by slave
    if ( (request->data_size < EC_MBOX_HEADER_SIZE) ||
            !mbox_type_to_prot(EC_READ_U16(request->data + 5) & 0x0F, &mbox_prot) ||
            !(slave->sii_image->sii.mailbox_protocols & mbox_prot) ) {
        EC_SLAVE_ERR(slave, "Slave does not support requested mailbox type!\n");
        request->error_code = EPROTONOSUPPORT;
        fsm->state = ec_fsm_mbg_error;
        return;
    }
    
    // cache the mbox type
    request->mbox_type = EC_READ_U16(request->data + 5) & 0x0F;
    
    if (slave->configured_rx_mailbox_size <
            request->data_size) {
        EC_SLAVE_ERR(slave, "Mailbox too small!\n");
        request->error_code = EOVERFLOW;
        fsm->state = ec_fsm_mbg_error;
        return;
    }


    fsm->request->jiffies_sent = jiffies;
    fsm->retries = EC_FSM_RETRIES;

    if (ec_fsm_mbg_prepare_start(fsm, datagram)) {
        fsm->state = ec_fsm_mbg_error;
    }
}

/*****************************************************************************/

/**
   MBox Gateway: REQUEST.
   \todo Timeout behavior
*/

void ec_fsm_mbg_request(
        ec_fsm_mbg_t *fsm, /**< Finite state machine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    ec_slave_t *slave = fsm->slave;
    unsigned long diff_ms;

    if (fsm->datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--) {
        if (ec_fsm_mbg_prepare_start(fsm, datagram)) {
            fsm->state = ec_fsm_mbg_error;
        }
        return;
    }

    if (fsm->datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->request->error_code = EIO;
        fsm->state = ec_fsm_mbg_error;
        EC_SLAVE_ERR(slave, "Failed to receive MBox Gateway"
                " request datagram: ");
        ec_datagram_print_state(fsm->datagram);
        return;
    }

    diff_ms = (jiffies - fsm->request->jiffies_sent) * 1000 / HZ;

    if (fsm->datagram->working_counter != 1) {
        if (!fsm->datagram->working_counter) {
            if (diff_ms < fsm->request->response_timeout) {
#if DEBUG_RETRIES
                EC_SLAVE_DBG(slave, 1, "Slave did not respond to MBox"
                        " Gateway request. Retrying after %lu ms...\n",
                        diff_ms);
#endif
                // no response; send request datagram again
                if (ec_fsm_mbg_prepare_start(fsm, datagram)) {
                    fsm->state = ec_fsm_mbg_error;
                }
                return;
            }
        }
        fsm->request->error_code = EIO;
        fsm->state = ec_fsm_mbg_error;
        EC_SLAVE_ERR(slave, "Reception of MBox Gateway request"
                " failed with timeout after %lu ms: ", diff_ms);
        ec_datagram_print_wc_error(fsm->datagram);
        return;
    }

#if DEBUG_LONG
    if (diff_ms > 200) {
        EC_SLAVE_WARN(slave, "MBox Gateway request took %lu ms.\n", diff_ms);
    }
#endif

    fsm->jiffies_start = fsm->datagram->jiffies_sent;

    // mailbox read check is skipped if a read request is already ongoing
    if (ec_read_mbox_locked(slave)) {
        fsm->state = ec_fsm_mbg_response_data;
        // the datagram is not used and marked as invalid
        datagram->state = EC_DATAGRAM_INVALID;
    } else {
        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_mbg_check;
    }
}

/*****************************************************************************/

/** MBox Gateway state: CHECK.
 */
void ec_fsm_mbg_check(
        ec_fsm_mbg_t *fsm, /**< Finite state machine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    ec_slave_t *slave = fsm->slave;

    if (fsm->datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--) {
        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        return;
    }

    if (fsm->datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->request->error_code = EIO;
        fsm->state = ec_fsm_mbg_error;
        ec_read_mbox_lock_clear(slave);
        EC_SLAVE_ERR(slave, "Failed to receive MBox Gateway mailbox check"
                " datagram: ");
        ec_datagram_print_state(fsm->datagram);
        return;
    }

    if (fsm->datagram->working_counter != 1) {
        fsm->request->error_code = EIO;
        fsm->state = ec_fsm_mbg_error;
        ec_read_mbox_lock_clear(slave);
        EC_SLAVE_ERR(slave, "Reception of MBox Gateway check"
                " datagram failed: ");
        ec_datagram_print_wc_error(fsm->datagram);
        return;
    }

    if (!ec_slave_mbox_check(fsm->datagram)) {
        unsigned long diff_ms = 0;

        // check that data is not already received by another read request
        if (slave->mbox_mbg_data.payload_size > 0) {
            ec_read_mbox_lock_clear(slave);
            fsm->state = ec_fsm_mbg_response_data;
            fsm->state(fsm, datagram);
            return;
        }

        diff_ms = (fsm->datagram->jiffies_received - fsm->jiffies_start) *
        1000 / HZ;

        if (diff_ms >= fsm->request->response_timeout) {
            fsm->request->error_code = EIO;
            fsm->state = ec_fsm_mbg_error;
            ec_read_mbox_lock_clear(slave);
            EC_SLAVE_ERR(slave, "Timeout after %lu ms while waiting"
                    " for MBox Gateway response.\n", diff_ms);
            return;
        }

        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        return;
    }

    // Fetch response
    ec_slave_mbox_prepare_fetch(slave, datagram); // can not fail.
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_mbg_response;
}

/*****************************************************************************/

/**
   MBox Gateway state: RESPONSE.
   \todo Timeout behavior
*/

void ec_fsm_mbg_response(
        ec_fsm_mbg_t *fsm, /**< Finite state machine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    ec_slave_t *slave = fsm->slave;
    ec_mbg_request_t *request = fsm->request;

    if (fsm->datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--) {
        ec_slave_mbox_prepare_fetch(slave, datagram); // can not fail.
        return;
    }

    if (fsm->datagram->state != EC_DATAGRAM_RECEIVED) {
        request->error_code = EIO;
        fsm->state = ec_fsm_mbg_error;
        EC_SLAVE_ERR(slave, "Failed to receive MBox Gateway"
                " response datagram: ");
        ec_datagram_print_state(fsm->datagram);
        return;
    }

    if (fsm->datagram->working_counter != 1) {
        // only an error if data has not already been read by another read request
        if (slave->mbox_mbg_data.payload_size == 0) {
            request->error_code = EIO;
            fsm->state = ec_fsm_mbg_error;
            ec_read_mbox_lock_clear(slave);
            EC_SLAVE_ERR(slave, "Reception of MBox Gateway response failed: ");
            ec_datagram_print_wc_error(fsm->datagram);
            return;
        }
    }
    ec_read_mbox_lock_clear(slave);
    fsm->state = ec_fsm_mbg_response_data;
    fsm->state(fsm, datagram);
}

/*****************************************************************************/

/**
   MBox Gateway state: RESPONSE DATA.

*/

void ec_fsm_mbg_response_data(
        ec_fsm_mbg_t *fsm, /**< Finite state machine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
    ec_slave_t *slave = fsm->slave;
    ec_mbg_request_t *request = fsm->request;
    uint8_t *data, mbox_type;
    size_t data_size;
    int ret;

    // process the data available or initiate a new mailbox read check
    if (slave->mbox_mbg_data.payload_size > 0) {
        slave->mbox_mbg_data.payload_size = 0;
    } else {
        // initiate a new mailbox read check if required data is not available
        if (ec_read_mbox_locked(slave)) {
            // await current read request and mark the datagram as invalid
            datagram->state = EC_DATAGRAM_INVALID;
        } else {
            ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
            fsm->state = ec_fsm_mbg_check;
        }
        return;
    }

    // check data is assigned
    if (!slave->mbox_mbg_data.data) {
        request->error_code = EPROTO;
        fsm->state = ec_fsm_mbg_error;
        EC_SLAVE_ERR(slave, "No mailbox response data received!\n");
        return;
    }
    data      = slave->mbox_mbg_data.data;
    data_size = EC_READ_U16(data);

    // sanity check that data size received is not too big
    if (data_size + EC_MBOX_HEADER_SIZE > slave->configured_tx_mailbox_size) {
        request->error_code = EPROTO;
        fsm->state = ec_fsm_mbg_error;
        EC_SLAVE_ERR(slave, "Corrupt mailbox response received!\n");
        ec_print_data(data, slave->configured_tx_mailbox_size);
        return;
    }
    
    // check for error response, output to log, but continue
    mbox_type = EC_READ_U8(data + 5) & 0x0F;
    if (mbox_type == 0x00) {
        const ec_code_msg_t *mbox_msg;
        uint16_t code = EC_READ_U16(data + 8);

        EC_SLAVE_ERR(slave, "Mailbox Gateway error response received - ");

        for (mbox_msg = mbox_error_messages; mbox_msg->code; mbox_msg++) {
            if (mbox_msg->code != code)
                continue;
            printk(KERN_CONT "Code 0x%04X: \"%s\".\n",
                    mbox_msg->code, mbox_msg->message);
            break;
        }

        if (!mbox_msg->code) {
            printk(KERN_CONT "Unknown error reply code 0x%04X.\n", code);
        }

        if (slave->master->debug_level && data_size > 0) {
            ec_print_data(data + EC_MBOX_HEADER_SIZE, data_size);
        }
    }
    
    // add back on the header size
    data_size += EC_MBOX_HEADER_SIZE;

    // check the response matches the request mbox type
    if (mbox_type != request->mbox_type) {
        request->error_code = EIO;
        fsm->state = ec_fsm_mbg_error;
        EC_SLAVE_ERR(slave, "Received mailbox type 0x%02X as response.\n",
                mbox_type);
        return;
    }

    if (slave->master->debug_level) {
        EC_SLAVE_DBG(slave, 1, "MBox Gateway response:\n");
        ec_print_data(data, data_size);
    }
    
    ret = ec_mbg_request_copy_data(request, data, data_size);
    if (ret) {
        request->error_code = -ret;
        fsm->state = ec_fsm_mbg_error;
        return;
    }

    fsm->state = ec_fsm_mbg_end;
}

/*****************************************************************************/

/**
   State: ERROR.
*/

void ec_fsm_mbg_error(
        ec_fsm_mbg_t *fsm, /**< Finite state machine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
}

/*****************************************************************************/

/**
   State: END.
*/

void ec_fsm_mbg_end(
        ec_fsm_mbg_t *fsm, /**< Finite state machine. */
        ec_datagram_t *datagram /**< Datagram to use. */
        )
{
}

/*****************************************************************************/
