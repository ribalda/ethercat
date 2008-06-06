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
   EtherCAT CoE state machines.
*/

/*****************************************************************************/

#include "globals.h"
#include "master.h"
#include "mailbox.h"
#include "fsm_coe.h"

/*****************************************************************************/

void ec_fsm_coe_dict_start(ec_fsm_coe_t *);
void ec_fsm_coe_dict_request(ec_fsm_coe_t *);
void ec_fsm_coe_dict_check(ec_fsm_coe_t *);
void ec_fsm_coe_dict_response(ec_fsm_coe_t *);
void ec_fsm_coe_dict_desc_request(ec_fsm_coe_t *);
void ec_fsm_coe_dict_desc_check(ec_fsm_coe_t *);
void ec_fsm_coe_dict_desc_response(ec_fsm_coe_t *);
void ec_fsm_coe_dict_entry_request(ec_fsm_coe_t *);
void ec_fsm_coe_dict_entry_check(ec_fsm_coe_t *);
void ec_fsm_coe_dict_entry_response(ec_fsm_coe_t *);

void ec_fsm_coe_down_start(ec_fsm_coe_t *);
void ec_fsm_coe_down_request(ec_fsm_coe_t *);
void ec_fsm_coe_down_check(ec_fsm_coe_t *);
void ec_fsm_coe_down_response(ec_fsm_coe_t *);

void ec_fsm_coe_up_start(ec_fsm_coe_t *);
void ec_fsm_coe_up_request(ec_fsm_coe_t *);
void ec_fsm_coe_up_check(ec_fsm_coe_t *);
void ec_fsm_coe_up_response(ec_fsm_coe_t *);
void ec_fsm_coe_up_seg_request(ec_fsm_coe_t *);
void ec_fsm_coe_up_seg_check(ec_fsm_coe_t *);
void ec_fsm_coe_up_seg_response(ec_fsm_coe_t *);

void ec_fsm_coe_end(ec_fsm_coe_t *);
void ec_fsm_coe_error(ec_fsm_coe_t *);

/*****************************************************************************/

/**
   Sdo abort messages.
   The "abort Sdo transfer request" supplies an abort code,
   which can be translated to clear text. This table does
   the mapping of the codes and messages.
*/

const ec_code_msg_t sdo_abort_messages[] = {
    {0x05030000, "Toggle bit not changed"},
    {0x05040000, "Sdo protocol timeout"},
    {0x05040001, "Client/Server command specifier not valid or unknown"},
    {0x05040005, "Out of memory"},
    {0x06010000, "Unsupported access to an object"},
    {0x06010001, "Attempt to read a write-only object"},
    {0x06010002, "Attempt to write a read-only object"},
    {0x06020000, "This object does not exist in the object directory"},
    {0x06040041, "The object cannot be mapped into the Pdo"},
    {0x06040042, "The number and length of the objects to be mapped would"
     " exceed the Pdo length"},
    {0x06040043, "General parameter incompatibility reason"},
    {0x06040047, "Gerneral internal incompatibility in device"},
    {0x06060000, "Access failure due to a hardware error"},
    {0x06070010, "Data type does not match, length of service parameter does"
     " not match"},
    {0x06070012, "Data type does not match, length of service parameter too"
     " high"},
    {0x06070013, "Data type does not match, length of service parameter too"
     " low"},
    {0x06090011, "Subindex does not exist"},
    {0x06090030, "Value range of parameter exceeded"},
    {0x06090031, "Value of parameter written too high"},
    {0x06090032, "Value of parameter written too low"},
    {0x06090036, "Maximum value is less than minimum value"},
    {0x08000000, "General error"},
    {0x08000020, "Data cannot be transferred or stored to the application"},
    {0x08000021, "Data cannot be transferred or stored to the application"
     " because of local control"},
    {0x08000022, "Data cannot be transferred or stored to the application"
     " because of the present device state"},
    {0x08000023, "Object dictionary dynamic generation fails or no object"
     " dictionary is present"},
    {}
};

/*****************************************************************************/

/**
   Outputs an Sdo abort message.
*/

void ec_canopen_abort_msg(uint32_t abort_code)
{
    const ec_code_msg_t *abort_msg;

    for (abort_msg = sdo_abort_messages; abort_msg->code; abort_msg++) {
        if (abort_msg->code == abort_code) {
            EC_ERR("Sdo abort message 0x%08X: \"%s\".\n",
                   abort_msg->code, abort_msg->message);
            return;
        }
    }

    EC_ERR("Unknown Sdo abort code 0x%08X.\n", abort_code);
}

/*****************************************************************************/

/**
   Constructor.
*/

void ec_fsm_coe_init(ec_fsm_coe_t *fsm, /**< finite state machine */
                     ec_datagram_t *datagram /**< datagram */
                     )
{
    fsm->state = NULL;
    fsm->datagram = datagram;
}

/*****************************************************************************/

/**
   Destructor.
*/

void ec_fsm_coe_clear(ec_fsm_coe_t *fsm /**< finite state machine */)
{
}

/*****************************************************************************/

/**
   Starts reading a slaves' Sdo dictionary.
*/

void ec_fsm_coe_dictionary(ec_fsm_coe_t *fsm, /**< finite state machine */
                           ec_slave_t *slave /**< EtherCAT slave */
                           )
{
    fsm->slave = slave;
    fsm->state = ec_fsm_coe_dict_start;
}

/*****************************************************************************/

/**
   Starts to transfer an Sdo to/from a slave.
*/

void ec_fsm_coe_transfer(
        ec_fsm_coe_t *fsm, /**< State machine. */
        ec_slave_t *slave, /**< EtherCAT slave. */
        ec_sdo_request_t *request /**< Sdo request. */
        )
{
    fsm->slave = slave;
    fsm->request = request;
    if (request->dir == EC_DIR_OUTPUT)
        fsm->state = ec_fsm_coe_down_start;
    else
        fsm->state = ec_fsm_coe_up_start;
}

/*****************************************************************************/

/**
   Executes the current state of the state machine.
   \return false, if state machine has terminated
*/

int ec_fsm_coe_exec(ec_fsm_coe_t *fsm /**< finite state machine */)
{
    fsm->state(fsm);

    return fsm->state != ec_fsm_coe_end && fsm->state != ec_fsm_coe_error;
}

/*****************************************************************************/

/**
   Returns, if the state machine terminated with success.
   \return non-zero if successful.
*/

int ec_fsm_coe_success(ec_fsm_coe_t *fsm /**< Finite state machine */)
{
    return fsm->state == ec_fsm_coe_end;
}

/*****************************************************************************/

/**
 */
int ec_fsm_coe_check_emergency(
        ec_fsm_coe_t *fsm, /**< Finite state machine */
        const uint8_t *data, /**< CoE mailbox data. */
        size_t size /**< CoE mailbox data size. */
        )
{
    if (size < 2 || ((EC_READ_U16(data) >> 12) & 0x0F) != 0x01)
        return 0;

    if (size < 10) {
        EC_WARN("Received incomplete CoE Emergency request from slave %u:\n",
                fsm->slave->ring_position);
        ec_print_data(data, size);
        return 1;
    }
    
    EC_INFO("CoE Emergency Request received from slave %u:\n",
            fsm->slave->ring_position);
    EC_INFO("Error code 0x%04X, Error register 0x%02X, data:\n",
            EC_READ_U16(data + 2), EC_READ_U8(data + 4));
    ec_print_data(data + 5, 5);
    return 1;
}

/******************************************************************************
 *  CoE dictionary state machine
 *****************************************************************************/

/**
   CoE state: DICT START.
*/

void ec_fsm_coe_dict_start(ec_fsm_coe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    uint8_t *data;

    if (!(slave->sii.mailbox_protocols & EC_MBOX_COE)) {
        EC_ERR("Slave %u does not support CoE!\n", slave->ring_position);
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (!(data = ec_slave_mbox_prepare_send(slave, datagram, 0x03, 8))) {
        fsm->state = ec_fsm_coe_error;
        return;
    }

    EC_WRITE_U16(data, 0x8 << 12); // Sdo information
    EC_WRITE_U8 (data + 2, 0x01); // Get OD List Request
    EC_WRITE_U8 (data + 3, 0x00);
    EC_WRITE_U16(data + 4, 0x0000);
    EC_WRITE_U16(data + 6, 0x0001); // deliver all Sdos!

    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_coe_dict_request;
}

/*****************************************************************************/

/**
   CoE state: DICT REQUEST.
   \todo Timeout behavior
*/

void ec_fsm_coe_dict_request(ec_fsm_coe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return; // FIXME: request again?

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Failed to receive CoE dictionary request datagram for"
               " slave %u (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Reception of CoE dictionary request failed on slave %u: ",
                slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    fsm->cycles_start = datagram->cycles_sent;

    ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_coe_dict_check;
}

/*****************************************************************************/

/**
   CoE state: DICT CHECK.
*/

void ec_fsm_coe_dict_check(ec_fsm_coe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Failed to receive CoE mailbox check datagram for slave %u"
                " (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Reception of CoE mailbox check datagram failed on slave %u: ",
               slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    if (!ec_slave_mbox_check(datagram)) {
        if (datagram->cycles_received
            - fsm->cycles_start >= (cycles_t) 100 * cpu_khz) {
            fsm->state = ec_fsm_coe_error;
            EC_ERR("Timeout while checking Sdo dictionary on slave %u.\n",
                   slave->ring_position);
            return;
        }

        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        return;
    }

    // Fetch response
    ec_slave_mbox_prepare_fetch(slave, datagram); // can not fail.
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_coe_dict_response;
}

/*****************************************************************************/

/**
   CoE state: DICT RESPONSE.
   \todo Timeout behavior
*/

void ec_fsm_coe_dict_response(ec_fsm_coe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    uint8_t *data, mbox_prot;
    size_t rec_size;
    unsigned int sdo_count, i;
    uint16_t sdo_index, fragments_left;
    ec_sdo_t *sdo;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return; // FIXME: request again?

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Failed to receive CoE dictionary response datagram for"
               " slave %u (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Reception of CoE dictionary response failed on slave %u: ",
                slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    if (!(data = ec_slave_mbox_fetch(slave, datagram,
				     &mbox_prot, &rec_size))) {
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (mbox_prot != 0x03) { // CoE
        EC_ERR("Received mailbox protocol 0x%02X as response.\n", mbox_prot);
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (ec_fsm_coe_check_emergency(fsm, data, rec_size)) {
        // check for CoE response again
        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_coe_dict_check;
        return;
    }

    if (EC_READ_U16(data) >> 12 == 0x8 && // Sdo information
        (EC_READ_U8(data + 2) & 0x7F) == 0x07) { // error response
        EC_ERR("Sdo information error response at slave %u!\n",
               slave->ring_position);
        ec_canopen_abort_msg(EC_READ_U32(data + 6));
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (EC_READ_U16(data) >> 12 != 0x8 || // Sdo information
        (EC_READ_U8 (data + 2) & 0x7F) != 0x02) { // Get OD List response
        EC_ERR("Invalid Sdo list response at slave %u!\n",
               slave->ring_position);
        ec_print_data(data, rec_size);
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (rec_size < 8) {
        EC_ERR("Invalid data size!\n");
        ec_print_data(data, rec_size);
        fsm->state = ec_fsm_coe_error;
        return;
    }

    sdo_count = (rec_size - 8) / 2;

    for (i = 0; i < sdo_count; i++) {
        sdo_index = EC_READ_U16(data + 8 + i * 2);
        if (!sdo_index) {
            if (slave->master->debug_level)
                EC_WARN("Sdo dictionary of slave %u contains index 0x0000.\n",
                        slave->ring_position);
            continue;
        }

        if (!(sdo = (ec_sdo_t *) kmalloc(sizeof(ec_sdo_t), GFP_ATOMIC))) {
            EC_ERR("Failed to allocate memory for Sdo!\n");
            fsm->state = ec_fsm_coe_error;
            return;
        }

        if (ec_sdo_init(sdo, sdo_index, slave)) {
            EC_ERR("Failed to init Sdo!\n");
            fsm->state = ec_fsm_coe_error;
            return;
        }

        list_add_tail(&sdo->list, &slave->sdo_dictionary);
    }

    fragments_left = EC_READ_U16(data + 4);
    if (slave->master->debug_level && fragments_left) {
        EC_DBG("Sdo list fragments left: %u\n", fragments_left);
    }

    if (EC_READ_U8(data + 2) & 0x80 || fragments_left) { // more messages waiting. check again.
        fsm->cycles_start = datagram->cycles_sent;
        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_coe_dict_check;
        return;
    }

    if (list_empty(&slave->sdo_dictionary)) {
        // no Sdos in dictionary. finished.
        fsm->state = ec_fsm_coe_end; // success
        return;
    }

    // fetch Sdo descriptions
    fsm->sdo = list_entry(slave->sdo_dictionary.next, ec_sdo_t, list);

    if (!(data = ec_slave_mbox_prepare_send(slave, datagram, 0x03, 8))) {
        fsm->state = ec_fsm_coe_error;
        return;
    }

    EC_WRITE_U16(data, 0x8 << 12); // Sdo information
    EC_WRITE_U8 (data + 2, 0x03); // Get object description request
    EC_WRITE_U8 (data + 3, 0x00);
    EC_WRITE_U16(data + 4, 0x0000);
    EC_WRITE_U16(data + 6, fsm->sdo->index); // Sdo index

    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_coe_dict_desc_request;
}

/*****************************************************************************/

/**
   CoE state: DICT DESC REQUEST.
   \todo Timeout behavior
*/

void ec_fsm_coe_dict_desc_request(ec_fsm_coe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return; // FIXME: check for response first?

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Failed to receive CoE Sdo description request datagram for"
               " slave %u (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Reception of CoE Sdo description"
                " request failed on slave %u: ", slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    fsm->cycles_start = datagram->cycles_sent;

    ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_coe_dict_desc_check;
}

/*****************************************************************************/

/**
   CoE state: DICT DESC CHECK.
*/

void ec_fsm_coe_dict_desc_check(ec_fsm_coe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Failed to receive CoE mailbox check datagram from slave %u"
                " (datagram state %i).\n",
                slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Reception of CoE mailbox check"
                " datagram failed on slave %u: ", slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    if (!ec_slave_mbox_check(datagram)) {
        if (datagram->cycles_received
            - fsm->cycles_start >= (cycles_t) 100 * cpu_khz) {
            fsm->state = ec_fsm_coe_error;
            EC_ERR("Timeout while checking Sdo description on slave %u.\n",
                   slave->ring_position);
            return;
        }

        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        return;
    }

    // Fetch response
    ec_slave_mbox_prepare_fetch(slave, datagram); // can not fail.
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_coe_dict_desc_response;
}

/*****************************************************************************/

/**
   CoE state: DICT DESC RESPONSE.
   \todo Timeout behavior
*/

void ec_fsm_coe_dict_desc_response(ec_fsm_coe_t *fsm
                                   /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    ec_sdo_t *sdo = fsm->sdo;
    uint8_t *data, mbox_prot;
    size_t rec_size, name_size;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return; // FIXME: request again?

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Failed to receive CoE Sdo description response datagram from"
               " slave %u (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Reception of CoE Sdo description"
                " response failed on slave %u: ", slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    if (!(data = ec_slave_mbox_fetch(slave, datagram,
				     &mbox_prot, &rec_size))) {
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (mbox_prot != 0x03) { // CoE
        EC_ERR("Received mailbox protocol 0x%02X as response.\n", mbox_prot);
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (ec_fsm_coe_check_emergency(fsm, data, rec_size)) {
        // check for CoE response again
        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_coe_dict_desc_check;
        return;
    }

    if (EC_READ_U16(data) >> 12 == 0x8 && // Sdo information
        (EC_READ_U8 (data + 2) & 0x7F) == 0x07) { // error response
        EC_ERR("Sdo information error response at slave %u while"
               " fetching Sdo 0x%04X!\n", slave->ring_position,
               sdo->index);
        ec_canopen_abort_msg(EC_READ_U32(data + 6));
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (EC_READ_U16(data) >> 12 != 0x8 || // Sdo information
        (EC_READ_U8 (data + 2) & 0x7F) != 0x04 || // Object desc. response
        EC_READ_U16(data + 6) != sdo->index) { // Sdo index
        EC_ERR("Invalid object description response at slave %u while"
               " fetching Sdo 0x%04X!\n", slave->ring_position,
               sdo->index);
        ec_print_data(data, rec_size);
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (rec_size < 12) {
        EC_ERR("Invalid data size!\n");
        ec_print_data(data, rec_size);
        fsm->state = ec_fsm_coe_error;
        return;
    }

    sdo->max_subindex = EC_READ_U8(data + 10);
    sdo->object_code = EC_READ_U8(data + 11);

    name_size = rec_size - 12;
    if (name_size) {
        if (!(sdo->name = kmalloc(name_size + 1, GFP_ATOMIC))) {
            EC_ERR("Failed to allocate Sdo name!\n");
            fsm->state = ec_fsm_coe_error;
            return;
        }

        memcpy(sdo->name, data + 12, name_size);
        sdo->name[name_size] = 0;
    }

    if (EC_READ_U8(data + 2) & 0x80) {
        EC_ERR("Fragment follows (not implemented)!\n");
        fsm->state = ec_fsm_coe_error;
        return;
    }

    // start fetching entries

    fsm->subindex = 0;

    if (!(data = ec_slave_mbox_prepare_send(slave, datagram, 0x03, 10))) {
        fsm->state = ec_fsm_coe_error;
        return;
    }

    EC_WRITE_U16(data, 0x8 << 12); // Sdo information
    EC_WRITE_U8 (data + 2, 0x05); // Get entry description request
    EC_WRITE_U8 (data + 3, 0x00);
    EC_WRITE_U16(data + 4, 0x0000);
    EC_WRITE_U16(data + 6, sdo->index); // Sdo index
    EC_WRITE_U8 (data + 8, fsm->subindex); // Sdo subindex
    EC_WRITE_U8 (data + 9, 0x00); // value info (no values)

    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_coe_dict_entry_request;
}

/*****************************************************************************/

/**
   CoE state: DICT ENTRY REQUEST.
   \todo Timeout behavior
*/

void ec_fsm_coe_dict_entry_request(ec_fsm_coe_t *fsm
                                   /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return; // FIXME: check for response first?

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Failed to receive CoE Sdo entry request datagram for"
               " slave %u (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Reception of CoE Sdo entry request failed on slave %u: ",
                slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    fsm->cycles_start = datagram->cycles_sent;

    ec_slave_mbox_prepare_check(slave, datagram); // can not fail
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_coe_dict_entry_check;
}

/*****************************************************************************/

/**
   CoE state: DICT ENTRY CHECK.
*/

void ec_fsm_coe_dict_entry_check(ec_fsm_coe_t *fsm
                                 /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Failed to receive CoE mailbox check datagram from slave %u"
                " (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Reception of CoE mailbox check"
                " datagram failed on slave %u: ", slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    if (!ec_slave_mbox_check(datagram)) {
        if (datagram->cycles_received
            - fsm->cycles_start >= (cycles_t) 100 * cpu_khz) {
            fsm->state = ec_fsm_coe_error;
            EC_ERR("Timeout while checking Sdo entry on slave %u.\n",
                   slave->ring_position);
            return;
        }

        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        return;
    }

    // Fetch response
    ec_slave_mbox_prepare_fetch(slave, datagram); // can not fail.
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_coe_dict_entry_response;
}

/*****************************************************************************/

/**
   CoE state: DICT ENTRY RESPONSE.
   \todo Timeout behavior
*/

void ec_fsm_coe_dict_entry_response(ec_fsm_coe_t *fsm
                                    /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    ec_sdo_t *sdo = fsm->sdo;
    uint8_t *data, mbox_prot;
    size_t rec_size, data_size;
    ec_sdo_entry_t *entry;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return; // FIXME: request again?

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Failed to receive CoE Sdo description response datagram from"
               " slave %u (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Reception of CoE Sdo description"
                " response failed on slave %u: ", slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    if (!(data = ec_slave_mbox_fetch(slave, datagram,
				     &mbox_prot, &rec_size))) {
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (mbox_prot != 0x03) { // CoE
        EC_ERR("Received mailbox protocol 0x%02X as response.\n", mbox_prot);
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (ec_fsm_coe_check_emergency(fsm, data, rec_size)) {
        // check for CoE response again
        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_coe_dict_entry_check;
        return;
    }

    if (EC_READ_U16(data) >> 12 == 0x8 && // Sdo information
        (EC_READ_U8 (data + 2) & 0x7F) == 0x07) { // error response
        EC_ERR("Sdo information error response at slave %u while"
               " fetching Sdo entry 0x%04X:%02X!\n", slave->ring_position,
               sdo->index, fsm->subindex);
        ec_canopen_abort_msg(EC_READ_U32(data + 6));
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (EC_READ_U16(data) >> 12 != 0x8 || // Sdo information
        (EC_READ_U8(data + 2) & 0x7F) != 0x06 || // Entry desc. response
        EC_READ_U16(data + 6) != sdo->index || // Sdo index
        EC_READ_U8(data + 8) != fsm->subindex) { // Sdo subindex
        EC_ERR("Invalid entry description response at slave %u while"
               " fetching Sdo entry 0x%04X:%02X!\n", slave->ring_position,
               sdo->index, fsm->subindex);
        ec_print_data(data, rec_size);
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (rec_size < 16) {
        EC_ERR("Invalid data size!\n");
        ec_print_data(data, rec_size);
        fsm->state = ec_fsm_coe_error;
        return;
    }

    data_size = rec_size - 16;

    if (!(entry = (ec_sdo_entry_t *)
          kmalloc(sizeof(ec_sdo_entry_t), GFP_ATOMIC))) {
        EC_ERR("Failed to allocate entry!\n");
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (ec_sdo_entry_init(entry, fsm->subindex, sdo)) {
        EC_ERR("Failed to init entry!\n");
        fsm->state = ec_fsm_coe_error;
        return;
    }

    entry->data_type = EC_READ_U16(data + 10);
    entry->bit_length = EC_READ_U16(data + 12);

    if (data_size) {
        uint8_t *desc;
        if (!(desc = kmalloc(data_size + 1, GFP_ATOMIC))) {
            EC_ERR("Failed to allocate Sdo entry name!\n");
            fsm->state = ec_fsm_coe_error;
            return;
        }
        memcpy(desc, data + 16, data_size);
        desc[data_size] = 0;
        entry->description = desc;
    }

    list_add_tail(&entry->list, &sdo->entries);

    if (fsm->subindex < sdo->max_subindex) {
        fsm->subindex++;

        if (!(data = ec_slave_mbox_prepare_send(slave, datagram, 0x03, 10))) {
            fsm->state = ec_fsm_coe_error;
            return;
        }

        EC_WRITE_U16(data, 0x8 << 12); // Sdo information
        EC_WRITE_U8 (data + 2, 0x05); // Get entry description request
        EC_WRITE_U8 (data + 3, 0x00);
        EC_WRITE_U16(data + 4, 0x0000);
        EC_WRITE_U16(data + 6, sdo->index); // Sdo index
        EC_WRITE_U8 (data + 8, fsm->subindex); // Sdo subindex
        EC_WRITE_U8 (data + 9, 0x00); // value info (no values)

        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_coe_dict_entry_request;
        return;
    }

    // another Sdo description to fetch?
    if (fsm->sdo->list.next != &slave->sdo_dictionary) {
        fsm->sdo = list_entry(fsm->sdo->list.next, ec_sdo_t, list);

        if (!(data = ec_slave_mbox_prepare_send(slave, datagram, 0x03, 8))) {
            fsm->state = ec_fsm_coe_error;
            return;
        }

        EC_WRITE_U16(data, 0x8 << 12); // Sdo information
        EC_WRITE_U8 (data + 2, 0x03); // Get object description request
        EC_WRITE_U8 (data + 3, 0x00);
        EC_WRITE_U16(data + 4, 0x0000);
        EC_WRITE_U16(data + 6, fsm->sdo->index); // Sdo index

        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_coe_dict_desc_request;
        return;
    }

    fsm->state = ec_fsm_coe_end;
}

/******************************************************************************
 *  CoE state machine
 *****************************************************************************/

/**
   CoE state: DOWN START.
*/

void ec_fsm_coe_down_start(ec_fsm_coe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    ec_sdo_request_t *request = fsm->request;
    uint8_t *data;

    if (fsm->slave->master->debug_level) {
        EC_DBG("Downloading Sdo 0x%04X:%02X to slave %u.\n",
               request->index, request->subindex, slave->ring_position);
        ec_print_data(request->data, request->data_size);
    }

    if (!(slave->sii.mailbox_protocols & EC_MBOX_COE)) {
        EC_ERR("Slave %u does not support CoE!\n", slave->ring_position);
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (slave->sii.rx_mailbox_size < 6 + 10 + request->data_size) {
        EC_ERR("Sdo fragmenting not supported yet!\n");
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (!(data = ec_slave_mbox_prepare_send(slave, datagram, 0x03,
                                            request->data_size + 10))) {
        fsm->state = ec_fsm_coe_error;
        return;
    }

    EC_WRITE_U16(data, 0x2 << 12); // Sdo request
    EC_WRITE_U8 (data + 2, (0x1 // size specified
                            | 0x1 << 5)); // Download request
    EC_WRITE_U16(data + 3, request->index);
    EC_WRITE_U8 (data + 5, request->subindex);
    EC_WRITE_U32(data + 6, request->data_size);
    memcpy(data + 10, request->data, request->data_size);

    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_coe_down_request;
}

/*****************************************************************************/

/**
   CoE state: DOWN REQUEST.
   \todo Timeout behavior
*/

void ec_fsm_coe_down_request(ec_fsm_coe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return; // FIXME: check for response first?

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Failed to receive CoE download request datagram for"
               " slave %u (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Reception of CoE download request failed on slave %u: ",
                slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    fsm->cycles_start = datagram->cycles_sent;

    ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_coe_down_check;
}

/*****************************************************************************/

/**
   CoE state: DOWN CHECK.
*/

void ec_fsm_coe_down_check(ec_fsm_coe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Failed to receive CoE mailbox check datagram for slave %u"
                " (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Reception of CoE mailbox check"
                " datagram failed on slave %u: ", slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    if (!ec_slave_mbox_check(datagram)) {
        if (datagram->cycles_received
            - fsm->cycles_start >= (cycles_t) 100 * cpu_khz) {
            fsm->state = ec_fsm_coe_error;
            EC_ERR("Timeout while checking Sdo configuration on slave %u.\n",
                   slave->ring_position);
            return;
        }

        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        return;
    }

    // Fetch response
    ec_slave_mbox_prepare_fetch(slave, datagram); // can not fail.
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_coe_down_response;
}

/*****************************************************************************/

/**
   CoE state: DOWN RESPONSE.
   \todo Timeout behavior
*/

void ec_fsm_coe_down_response(ec_fsm_coe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    uint8_t *data, mbox_prot;
    size_t rec_size;
    ec_sdo_request_t *request = fsm->request;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return; // FIXME: request again?

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Failed to receive CoE download response datagram from"
               " slave %u (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Reception of CoE download response failed on slave %u: ",
                slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    if (!(data = ec_slave_mbox_fetch(slave, datagram,
				     &mbox_prot, &rec_size))) {
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (mbox_prot != 0x03) { // CoE
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Received mailbox protocol 0x%02X as response.\n", mbox_prot);
        return;
    }

    if (ec_fsm_coe_check_emergency(fsm, data, rec_size)) {
        // check for CoE response again
        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_coe_down_check;
        return;
    }

    if (rec_size < 6) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Received data is too small (%u bytes):\n", rec_size);
        ec_print_data(data, rec_size);
        return;
    }

    if (EC_READ_U16(data) >> 12 == 0x2 && // Sdo request
        EC_READ_U8 (data + 2) >> 5 == 0x4) { // abort Sdo transfer request
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Sdo download 0x%04X:%02X (%u bytes) aborted on slave %u.\n",
               request->index, request->subindex, request->data_size,
               slave->ring_position);
        if (rec_size < 10) {
            EC_ERR("Incomplete Abort command:\n");
            ec_print_data(data, rec_size);
        }
        else
            ec_canopen_abort_msg(EC_READ_U32(data + 6));
        return;
    }

    if (EC_READ_U16(data) >> 12 != 0x3 || // Sdo response
        EC_READ_U8 (data + 2) >> 5 != 0x3 || // Download response
        EC_READ_U16(data + 3) != request->index || // index
        EC_READ_U8 (data + 5) != request->subindex) { // subindex
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Sdo download 0x%04X:%02X (%u bytes) failed:\n",
               request->index, request->subindex, request->data_size);
        EC_ERR("Invalid Sdo download response at slave %u!\n",
               slave->ring_position);
        ec_print_data(data, rec_size);
        return;
    }

    fsm->state = ec_fsm_coe_end; // success
}

/*****************************************************************************/

/**
   CoE state: UP START.
*/

void ec_fsm_coe_up_start(ec_fsm_coe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = slave->master;
    ec_sdo_request_t *request = fsm->request;
    uint8_t *data;

    if (master->debug_level)
        EC_DBG("Uploading Sdo 0x%04X:%02X from slave %u.\n",
               request->index, request->subindex, slave->ring_position);

    if (!(slave->sii.mailbox_protocols & EC_MBOX_COE)) {
        EC_ERR("Slave %u does not support CoE!\n", slave->ring_position);
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (!(data = ec_slave_mbox_prepare_send(slave, datagram, 0x03, 10))) {
        fsm->state = ec_fsm_coe_error;
        return;
    }

    EC_WRITE_U16(data, 0x2 << 12); // Sdo request
    EC_WRITE_U8 (data + 2, 0x2 << 5); // initiate upload request
    EC_WRITE_U16(data + 3, request->index);
    EC_WRITE_U8 (data + 5, request->subindex);
    memset(data + 6, 0x00, 4);

    if (master->debug_level) {
        EC_DBG("Upload request:\n");
        ec_print_data(data, 10);
    }

    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_coe_up_request;
}

/*****************************************************************************/

/**
   CoE state: UP REQUEST.
   \todo Timeout behavior
*/

void ec_fsm_coe_up_request(ec_fsm_coe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return; // FIXME: check for response first?

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Failed to receive CoE upload request for slave %u"
                " (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Reception of CoE upload request failed on slave %u: ",
                slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    fsm->cycles_start = datagram->cycles_sent;

    ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_coe_up_check;
}

/*****************************************************************************/

/**
   CoE state: UP CHECK.
*/

void ec_fsm_coe_up_check(ec_fsm_coe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Failed to receive CoE mailbox check datagram from slave %u"
                " (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Reception of CoE mailbox check"
                " datagram failed on slave %u: ", slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    if (!ec_slave_mbox_check(datagram)) {
        if (datagram->cycles_received
            - fsm->cycles_start >= (cycles_t) 100 * cpu_khz) {
            fsm->state = ec_fsm_coe_error;
            EC_ERR("Timeout while checking Sdo upload on slave %u.\n",
                   slave->ring_position);
            return;
        }

        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        return;
    }

    // Fetch response
    ec_slave_mbox_prepare_fetch(slave, datagram); // can not fail.
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_coe_up_response;
}

/*****************************************************************************/

/**
   CoE state: UP RESPONSE.
   \todo Timeout behavior
*/

void ec_fsm_coe_up_response(ec_fsm_coe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = slave->master;
    uint8_t *data, mbox_prot;
    size_t rec_size, data_size;
    ec_sdo_request_t *request = fsm->request;
    uint32_t complete_size;
    unsigned int expedited, size_specified;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return; // FIXME: request again?

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Failed to receive CoE upload response datagram for"
               " slave %u (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Reception of CoE upload response failed on slave %u: ",
                slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    if (!(data = ec_slave_mbox_fetch(slave, datagram,
				     &mbox_prot, &rec_size))) {
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (master->debug_level) {
        EC_DBG("Upload response:\n");
        ec_print_data(data, rec_size);
    }

    if (mbox_prot != 0x03) { // CoE
        fsm->state = ec_fsm_coe_error;
        EC_WARN("Received mailbox protocol 0x%02X as response.\n", mbox_prot);
        return;
    }

    if (ec_fsm_coe_check_emergency(fsm, data, rec_size)) {
        // check for CoE response again
        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_coe_up_check;
        return;
    }

    if (rec_size < 3) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Received currupted Sdo upload response (%u bytes)!\n", rec_size);
        ec_print_data(data, rec_size);
        return;
    }

    if (EC_READ_U16(data) >> 12 == 0x2 && // Sdo request
        EC_READ_U8 (data + 2) >> 5 == 0x4) { // abort Sdo transfer request
        EC_ERR("Sdo upload 0x%04X:%02X aborted on slave %u.\n",
               request->index, request->subindex, slave->ring_position);
        if (rec_size >= 10)
            ec_canopen_abort_msg(EC_READ_U32(data + 6));
        else
            EC_ERR("No abort message.\n");
        fsm->state = ec_fsm_coe_error;
        return;
    }

    // normal or expedited?
    expedited = EC_READ_U8(data + 2) & 0x02;

    if (expedited) {
        if (rec_size < 7) {
            fsm->state = ec_fsm_coe_error;
            EC_ERR("Received currupted Sdo expedited upload"
                    " response (only %u bytes)!\n", rec_size);
            ec_print_data(data, rec_size);
            return;
        }

        if (EC_READ_U16(data) >> 12 != 0x3 || // Sdo response
                EC_READ_U8 (data + 2) >> 5 != 0x2 || // upload response
                EC_READ_U16(data + 3) != request->index || // index
                EC_READ_U8 (data + 5) != request->subindex) { // subindex
            fsm->state = ec_fsm_coe_error;
            EC_ERR("Sdo upload 0x%04X:%02X failed:\n",
                    request->index, request->subindex);
            EC_ERR("Invalid Sdo upload expedited response at slave %u!\n",
                    slave->ring_position);
            ec_print_data(data, rec_size);
            return;
        }

        size_specified = EC_READ_U8(data + 2) & 0x01;
        if (size_specified) {
            complete_size = 4 - ((EC_READ_U8(data + 2) & 0x0C) >> 2);
        } else {
            complete_size = 4;
        }

        if (rec_size < 6 + complete_size) {
            fsm->state = ec_fsm_coe_error;
            EC_ERR("Received currupted Sdo expedited upload"
                    " response (only %u bytes)!\n", rec_size);
            ec_print_data(data, rec_size);
            return;
        }

        if (ec_sdo_request_copy_data(request, data + 6, complete_size)) {
            fsm->state = ec_fsm_coe_error;
            return;
        }
    } else { // normal
        if (rec_size < 10) {
            fsm->state = ec_fsm_coe_error;
            EC_ERR("Received currupted Sdo normal upload"
                    " response (only %u bytes)!\n", rec_size);
            ec_print_data(data, rec_size);
            return;
        }

        if (EC_READ_U16(data) >> 12 != 0x3 || // Sdo response
                EC_READ_U8 (data + 2) >> 5 != 0x2 || // upload response
                EC_READ_U16(data + 3) != request->index || // index
                EC_READ_U8 (data + 5) != request->subindex) { // subindex
            fsm->state = ec_fsm_coe_error;
            EC_ERR("Sdo upload 0x%04X:%02X failed:\n",
                    request->index, request->subindex);
            EC_ERR("Invalid Sdo normal upload response at slave %u!\n",
                    slave->ring_position);
            ec_print_data(data, rec_size);
            return;
        }

        data_size = rec_size - 10;
        complete_size = EC_READ_U32(data + 6);

        if (!complete_size) {
            fsm->state = ec_fsm_coe_error;
            EC_ERR("No complete size supplied!\n");
            ec_print_data(data, rec_size);
            return;
        }

        if (ec_sdo_request_alloc(request, complete_size)) {
            fsm->state = ec_fsm_coe_error;
            return;
        }

        if (ec_sdo_request_copy_data(request, data + 10, data_size)) {
            fsm->state = ec_fsm_coe_error;
            return;
        }

        fsm->toggle = 0;

        if (data_size < complete_size) {
            EC_WARN("Sdo data incomplete (%i / %i).\n",
                    data_size, complete_size);

            if (!(data = ec_slave_mbox_prepare_send(slave, datagram,
                                                    0x03, 3))) {
                fsm->state = ec_fsm_coe_error;
                return;
            }

            EC_WRITE_U16(data, 0x2 << 12); // Sdo request
            EC_WRITE_U8 (data + 2, (fsm->toggle << 4 // toggle
                                    | 0x3 << 5)); // upload segment request

            if (master->debug_level) {
                EC_DBG("Upload segment request:\n");
                ec_print_data(data, 3);
            }

            fsm->retries = EC_FSM_RETRIES;
            fsm->state = ec_fsm_coe_up_seg_request;
            return;
        }
    }

    if (master->debug_level) {
        EC_DBG("Uploaded data:\n");
        ec_print_data(request->data, request->data_size);
    }

    fsm->state = ec_fsm_coe_end; // success
}

/*****************************************************************************/

/**
   CoE state: UP REQUEST.
   \todo Timeout behavior
*/

void ec_fsm_coe_up_seg_request(ec_fsm_coe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return; // FIXME: check for response first?

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Failed to receive CoE upload segment request datagram for"
               " slave %u (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Reception of CoE upload segment"
                " request failed on slave %u: ", slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    fsm->cycles_start = datagram->cycles_sent;

    ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_coe_up_seg_check;
}

/*****************************************************************************/

/**
   CoE state: UP CHECK.
*/

void ec_fsm_coe_up_seg_check(ec_fsm_coe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Failed to receive CoE mailbox check datagram for slave %u"
                " (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Reception of CoE mailbox check"
                " datagram failed on slave %u: ", slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    if (!ec_slave_mbox_check(datagram)) {
        if (datagram->cycles_received
            - fsm->cycles_start >= (cycles_t) 100 * cpu_khz) {
            fsm->state = ec_fsm_coe_error;
            EC_ERR("Timeout while checking Sdo upload segment on slave %u.\n",
                   slave->ring_position);
            return;
        }

        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        return;
    }

    // Fetch response
    ec_slave_mbox_prepare_fetch(slave, datagram); // can not fail.
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_coe_up_seg_response;
}

/*****************************************************************************/

/**
   CoE state: UP RESPONSE.
   \todo Timeout behavior
   \todo Check for \a data_size exceeding \a complete_size.
*/

void ec_fsm_coe_up_seg_response(ec_fsm_coe_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = slave->master;
    uint8_t *data, mbox_prot;
    size_t rec_size, data_size;
    ec_sdo_request_t *request = fsm->request;
    uint32_t seg_size;
    unsigned int last_segment;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return; // FIXME: request again?

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Failed to receive CoE upload segment response datagram for"
               " slave %u (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->state = ec_fsm_coe_error;
        EC_ERR("Reception of CoE upload segment"
                " response failed on slave %u: ", slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    if (!(data = ec_slave_mbox_fetch(slave, datagram,
				     &mbox_prot, &rec_size))) {
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (master->debug_level) {
        EC_DBG("Upload segment response:\n");
        ec_print_data(data, rec_size);
    }

    if (mbox_prot != 0x03) { // CoE
        EC_ERR("Received mailbox protocol 0x%02X as response.\n", mbox_prot);
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (ec_fsm_coe_check_emergency(fsm, data, rec_size)) {
        // check for CoE response again
        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_coe_up_seg_check;
        return;
    }

    if (rec_size < 10) {
        EC_ERR("Received currupted Sdo upload segment response!\n");
        ec_print_data(data, rec_size);
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (EC_READ_U16(data) >> 12 == 0x2 && // Sdo request
        EC_READ_U8 (data + 2) >> 5 == 0x4) { // abort Sdo transfer request
        EC_ERR("Sdo upload 0x%04X:%02X aborted on slave %u.\n",
               request->index, request->subindex, slave->ring_position);
        ec_canopen_abort_msg(EC_READ_U32(data + 6));
        fsm->state = ec_fsm_coe_error;
        return;
    }

    if (EC_READ_U16(data) >> 12 != 0x3 || // Sdo response
        EC_READ_U8 (data + 2) >> 5 != 0x0) { // upload segment response
        EC_ERR("Sdo upload 0x%04X:%02X failed:\n", request->index, request->subindex);
        EC_ERR("Invalid Sdo upload segment response at slave %u!\n",
               slave->ring_position);
        ec_print_data(data, rec_size);
        fsm->state = ec_fsm_coe_error;
        return;
    }

    last_segment = EC_READ_U8(data + 2) & 0x01;
    seg_size = (EC_READ_U8(data + 2) & 0xE) >> 1;
    data_size = rec_size - 10;

    if (data_size != seg_size) {
        EC_WARN("Sdo segment data invalid (%i / %i)"
                " - Fragmenting not implemented.\n",
                data_size, seg_size);
    }

    memcpy(request->data + request->data_size, data + 10, data_size);
    request->data_size += data_size;

    if (!last_segment) {
        fsm->toggle = !fsm->toggle;

        if (!(data = ec_slave_mbox_prepare_send(slave, datagram, 0x03, 3))) {
            fsm->state = ec_fsm_coe_error;
            return;
        }

        EC_WRITE_U16(data, 0x2 << 12); // Sdo request
        EC_WRITE_U8 (data + 2, (fsm->toggle << 4 // toggle
                                | 0x3 << 5)); // upload segment request

        if (master->debug_level) {
            EC_DBG("Upload segment request:\n");
            ec_print_data(data, 3);
        }

        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_coe_up_seg_request;
        return;
    }

    if (master->debug_level) {
        EC_DBG("Uploaded data:\n");
        ec_print_data(request->data, request->data_size);
    }

    fsm->state = ec_fsm_coe_end; // success
}

/*****************************************************************************/

/**
   State: ERROR.
*/

void ec_fsm_coe_error(ec_fsm_coe_t *fsm /**< finite state machine */)
{
}

/*****************************************************************************/

/**
   State: END.
*/

void ec_fsm_coe_end(ec_fsm_coe_t *fsm /**< finite state machine */)
{
}

/*****************************************************************************/
