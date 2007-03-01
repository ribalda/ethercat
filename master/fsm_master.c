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
   EtherCAT finite state machines.
*/

/*****************************************************************************/

#include "globals.h"
#include "master.h"
#include "mailbox.h"
#include "fsm_master.h"

/*****************************************************************************/

void ec_fsm_master_state_start(ec_fsm_master_t *);
void ec_fsm_master_state_broadcast(ec_fsm_master_t *);
void ec_fsm_master_state_read_states(ec_fsm_master_t *);
void ec_fsm_master_state_acknowledge(ec_fsm_master_t *);
void ec_fsm_master_state_validate_vendor(ec_fsm_master_t *);
void ec_fsm_master_state_validate_product(ec_fsm_master_t *);
void ec_fsm_master_state_rewrite_addresses(ec_fsm_master_t *);
void ec_fsm_master_state_configure_slave(ec_fsm_master_t *);
void ec_fsm_master_state_scan_slaves(ec_fsm_master_t *);
void ec_fsm_master_state_write_eeprom(ec_fsm_master_t *);
void ec_fsm_master_state_sdodict(ec_fsm_master_t *);
void ec_fsm_master_state_sdo_request(ec_fsm_master_t *);
void ec_fsm_master_state_end(ec_fsm_master_t *);
void ec_fsm_master_state_error(ec_fsm_master_t *);

/*****************************************************************************/

/**
   Constructor.
*/

void ec_fsm_master_init(ec_fsm_master_t *fsm, /**< master state machine */
        ec_master_t *master, /**< EtherCAT master */
        ec_datagram_t *datagram /**< datagram object to use */
        )
{
    fsm->master = master;
    fsm->datagram = datagram;
    fsm->state = ec_fsm_master_state_start;
    fsm->slaves_responding = 0;
    fsm->topology_change_pending = 0;
    fsm->slave_states = EC_SLAVE_STATE_UNKNOWN;
    fsm->validate = 0;

    // init sub-state-machines
    ec_fsm_slave_init(&fsm->fsm_slave, fsm->datagram);
    ec_fsm_sii_init(&fsm->fsm_sii, fsm->datagram);
    ec_fsm_change_init(&fsm->fsm_change, fsm->datagram);
    ec_fsm_coe_init(&fsm->fsm_coe, fsm->datagram);
}

/*****************************************************************************/

/**
   Destructor.
*/

void ec_fsm_master_clear(ec_fsm_master_t *fsm /**< master state machine */)
{
    // clear sub-state machines
    ec_fsm_slave_clear(&fsm->fsm_slave);
    ec_fsm_sii_clear(&fsm->fsm_sii);
    ec_fsm_change_clear(&fsm->fsm_change);
    ec_fsm_coe_clear(&fsm->fsm_coe);
}

/*****************************************************************************/

/**
   Executes the current state of the state machine.
   If the state machine's datagram is not sent or received yet, the execution
   of the state machine is delayed to the next cycle.
   \return false, if state machine has terminated
*/

int ec_fsm_master_exec(ec_fsm_master_t *fsm /**< master state machine */)
{
    if (fsm->datagram->state == EC_DATAGRAM_SENT
        || fsm->datagram->state == EC_DATAGRAM_QUEUED) {
        // datagram was not sent or received yet.
        return ec_fsm_master_running(fsm);
    }

    fsm->state(fsm);
    return ec_fsm_master_running(fsm);
}

/*****************************************************************************/

/**
   \return false, if state machine has terminated
*/

int ec_fsm_master_running(ec_fsm_master_t *fsm /**< master state machine */)
{
    return fsm->state != ec_fsm_master_state_end
        && fsm->state != ec_fsm_master_state_error;
}

/*****************************************************************************/

/**
   \return true, if the master state machine terminated gracefully
*/

int ec_fsm_master_success(ec_fsm_master_t *fsm /**< master state machine */)
{
    return fsm->state == ec_fsm_master_state_end;
}

/******************************************************************************
 *  operation/idle state machine
 *****************************************************************************/

/**
   Master state: START.
   Starts with getting slave count and slave states.
*/

void ec_fsm_master_state_start(ec_fsm_master_t *fsm)
{
    ec_datagram_brd(fsm->datagram, 0x0130, 2);
    ec_master_queue_datagram(fsm->master, fsm->datagram);
    fsm->state = ec_fsm_master_state_broadcast;
}

/*****************************************************************************/

/**
   Master state: BROADCAST.
   Processes the broadcast read slave count and slaves states.
*/

void ec_fsm_master_state_broadcast(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    unsigned int i;
    ec_slave_t *slave;
    ec_master_t *master = fsm->master;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT) {
        // always retry
        ec_master_queue_datagram(fsm->master, fsm->datagram);
        return;
    }

    if (datagram->state != EC_DATAGRAM_RECEIVED) { // EC_DATAGRAM_ERROR
        // link is down
        fsm->slaves_responding = 0;
        list_for_each_entry(slave, &master->slaves, list) {
            slave->online = 0;
        }
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    // bus topology change?
    if (datagram->working_counter != fsm->slaves_responding) {
        fsm->topology_change_pending = 1;
        fsm->slaves_responding = datagram->working_counter;

        EC_INFO("%i slave%s responding.\n",
                fsm->slaves_responding,
                fsm->slaves_responding == 1 ? "" : "s");

        if (master->mode == EC_MASTER_MODE_OPERATION) {
            if (fsm->slaves_responding == master->slave_count) {
                fsm->validate = 1; // start validation later
            }
            else {
                EC_WARN("Invalid slave count. Bus in tainted state.\n");
            }
        }
    }

    // slave states changed?
    if (EC_READ_U8(datagram->data) != fsm->slave_states) {
        char states[EC_STATE_STRING_SIZE];
        fsm->slave_states = EC_READ_U8(datagram->data);
        ec_state_string(fsm->slave_states, states);
        EC_INFO("Slave states: %s.\n", states);
    }

    // topology change in idle mode: clear all slaves and scan the bus
    if (fsm->topology_change_pending &&
            master->mode == EC_MASTER_MODE_IDLE) {
        fsm->topology_change_pending = 0;

        ec_master_eoe_stop(master);
        ec_master_destroy_slaves(master);

        master->slave_count = datagram->working_counter;

        if (!master->slave_count) {
            // no slaves present -> finish state machine.
            fsm->state = ec_fsm_master_state_end;
            return;
        }

        // init slaves
        for (i = 0; i < master->slave_count; i++) {
            if (!(slave = (ec_slave_t *) kmalloc(sizeof(ec_slave_t),
                                                 GFP_ATOMIC))) {
                EC_ERR("Failed to allocate slave %i!\n", i);
                ec_master_destroy_slaves(master);
                fsm->state = ec_fsm_master_state_error;
                return;
            }

            if (ec_slave_init(slave, master, i, i + 1)) {
                // freeing of "slave" already done
                ec_master_destroy_slaves(master);
                fsm->state = ec_fsm_master_state_error;
                return;
            }

            list_add_tail(&slave->list, &master->slaves);
        }

        EC_INFO("Scanning bus.\n");

        // begin scanning of slaves
        fsm->slave = list_entry(master->slaves.next, ec_slave_t, list);
        ec_fsm_slave_start_scan(&fsm->fsm_slave, fsm->slave);
        ec_fsm_slave_exec(&fsm->fsm_slave); // execute immediately
        fsm->state = ec_fsm_master_state_scan_slaves;
        return;
    }

    // fetch state from each slave
    fsm->slave = list_entry(master->slaves.next, ec_slave_t, list);
    ec_datagram_nprd(fsm->datagram, fsm->slave->station_address, 0x0130, 2);
    ec_master_queue_datagram(master, fsm->datagram);
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_master_state_read_states;
}

/*****************************************************************************/

/**
 * Check for pending EEPROM write requests and process one.
 * \return non-zero, if an EEPROM write request is processed.
 */

int ec_fsm_master_action_process_eeprom(
        ec_fsm_master_t *fsm /**< master state machine */
        )
{
    ec_master_t *master = fsm->master;
    ec_eeprom_write_request_t *request;
    ec_slave_t *slave;

    down(&master->eeprom_sem);
    list_for_each_entry(request, &master->eeprom_requests, list) {
        list_del_init(&request->list); // dequeue
        up(&master->eeprom_sem);

        slave = request->slave;
        if (!slave->online || slave->error_flag) {
            EC_ERR("Discarding EEPROM data, slave %i not ready.\n",
                    slave->ring_position);
            request->state = EC_EEPROM_REQ_ERROR;
            wake_up_interruptible(&master->eeprom_queue);
            down(&master->eeprom_sem);
            continue;
        }

        // found pending EEPROM write operation. execute it!
        EC_INFO("Writing EEPROM of slave %i...\n", slave->ring_position);
        fsm->eeprom_request = request;
        fsm->eeprom_index = 0;
        ec_fsm_sii_write(&fsm->fsm_sii, request->slave, request->offset,
                request->words, EC_FSM_SII_NODE);
        fsm->state = ec_fsm_master_state_write_eeprom;
        fsm->state(fsm); // execute immediately
        return 1;
    }

    up(&master->eeprom_sem);
    return 0;
}

/*****************************************************************************/

/**
   Master action: PROC_STATES.
   Processes the slave states.
*/

void ec_fsm_master_action_process_states(ec_fsm_master_t *fsm
                                         /**< master state machine */
                                         )
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave;
    char old_state[EC_STATE_STRING_SIZE], new_state[EC_STATE_STRING_SIZE];

    // check if any slaves are not in the state, they're supposed to be
    list_for_each_entry(slave, &master->slaves, list) {
        if (slave->error_flag
            || !slave->online
            || slave->requested_state == EC_SLAVE_STATE_UNKNOWN
            || (slave->current_state == slave->requested_state
                && slave->self_configured)) continue;

        if (master->debug_level) {
            ec_state_string(slave->current_state, old_state);
            if (slave->current_state != slave->requested_state) {
                ec_state_string(slave->requested_state, new_state);
                EC_DBG("Changing state of slave %i (%s -> %s).\n",
                       slave->ring_position, old_state, new_state);
            }
            else if (!slave->self_configured) {
                EC_DBG("Reconfiguring slave %i (%s).\n",
                       slave->ring_position, old_state);
            }
        }

        fsm->slave = slave;
        ec_fsm_slave_start_conf(&fsm->fsm_slave, slave);
        ec_fsm_slave_exec(&fsm->fsm_slave); // execute immediately
        fsm->state = ec_fsm_master_state_configure_slave;
        return;
    }

    // Check, if EoE processing has to be started
    ec_master_eoe_start(master);

    if (master->mode == EC_MASTER_MODE_IDLE) {

        // Check for a pending SDO request
        if (master->sdo_seq_master != master->sdo_seq_user) {
            if (master->debug_level)
                EC_DBG("Processing SDO request...\n");
            slave = master->sdo_request->sdo->slave;
            if (slave->current_state == EC_SLAVE_STATE_INIT
                || !slave->online) {
                EC_ERR("Failed to process SDO request, slave %i not ready.\n",
                       slave->ring_position);
                master->sdo_request->return_code = -1;
                master->sdo_seq_master++;
            }
            else {
                // start uploading SDO
                fsm->slave = slave;
                fsm->state = ec_fsm_master_state_sdo_request;
                fsm->sdo_request = master->sdo_request;
                ec_fsm_coe_upload(&fsm->fsm_coe, slave, fsm->sdo_request);
                ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
                return;
            }
        }

        // check, if slaves have an SDO dictionary to read out.
        list_for_each_entry(slave, &master->slaves, list) {
            if (!(slave->sii_mailbox_protocols & EC_MBOX_COE)
                || slave->sdo_dictionary_fetched
                || slave->current_state == EC_SLAVE_STATE_INIT
                || jiffies - slave->jiffies_preop < EC_WAIT_SDO_DICT * HZ
                || !slave->online
                || slave->error_flag) continue;

            if (master->debug_level) {
                EC_DBG("Fetching SDO dictionary from slave %i.\n",
                       slave->ring_position);
            }

            slave->sdo_dictionary_fetched = 1;

            // start fetching SDO dictionary
            fsm->slave = slave;
            fsm->state = ec_fsm_master_state_sdodict;
            ec_fsm_coe_dictionary(&fsm->fsm_coe, slave);
            ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
            return;
        }

        // check for pending EEPROM write operations.
        if (ec_fsm_master_action_process_eeprom(fsm))
            return; // EEPROM write request found
    }

    fsm->state = ec_fsm_master_state_end;
}

/*****************************************************************************/

/**
   Master action: Get state of next slave.
*/

void ec_fsm_master_action_next_slave_state(ec_fsm_master_t *fsm
                                           /**< master state machine */)
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave = fsm->slave;

    // is there another slave to query?
    if (slave->list.next != &master->slaves) {
        // process next slave
        fsm->slave = list_entry(fsm->slave->list.next, ec_slave_t, list);
        ec_datagram_nprd(fsm->datagram, fsm->slave->station_address,
                         0x0130, 2);
        ec_master_queue_datagram(master, fsm->datagram);
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_master_state_read_states;
        return;
    }

    // all slave states read

    // check, if a bus validation has to be done
    if (fsm->validate) {
        fsm->validate = 0;
        list_for_each_entry(slave, &master->slaves, list) {
            if (slave->online) continue;

            // At least one slave is offline. validate!
            EC_INFO("Validating bus.\n");
            fsm->slave = list_entry(master->slaves.next, ec_slave_t, list);
            fsm->state = ec_fsm_master_state_validate_vendor;
            ec_fsm_sii_read(&fsm->fsm_sii, slave, 0x0008, EC_FSM_SII_POSITION);
            ec_fsm_sii_exec(&fsm->fsm_sii); // execute immediately
            return;
        }
    }

    ec_fsm_master_action_process_states(fsm);
}

/*****************************************************************************/

/**
   Master state: READ STATES.
   Fetches the AL- and online state of a slave.
*/

void ec_fsm_master_state_read_states(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = fsm->datagram;
    uint8_t new_state;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--) {
        ec_master_queue_datagram(fsm->master, fsm->datagram);
        return;
    }

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        EC_ERR("Failed to receive AL state datagram for slave %i"
                " (datagram state %i)\n", slave->ring_position, datagram->state);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    // did the slave not respond to its station address?
    if (datagram->working_counter != 1) {
        if (slave->online) {
            slave->online = 0;
            if (slave->master->debug_level)
                EC_DBG("Slave %i: offline.\n", slave->ring_position);
        }
        ec_fsm_master_action_next_slave_state(fsm);
        return;
    }

    // slave responded
    new_state = EC_READ_U8(datagram->data);
    if (!slave->online) { // slave was offline before
        slave->online = 1;
        slave->error_flag = 0; // clear error flag
        slave->current_state = new_state;
        if (slave->master->debug_level) {
            char cur_state[EC_STATE_STRING_SIZE];
            ec_state_string(slave->current_state, cur_state);
            EC_DBG("Slave %i: online (%s).\n",
                   slave->ring_position, cur_state);
        }
    }
    else if (new_state != slave->current_state) {
        if (slave->master->debug_level) {
            char old_state[EC_STATE_STRING_SIZE],
                cur_state[EC_STATE_STRING_SIZE];
            ec_state_string(slave->current_state, old_state);
            ec_state_string(new_state, cur_state);
            EC_DBG("Slave %i: %s -> %s.\n",
                   slave->ring_position, old_state, cur_state);
        }
        slave->current_state = new_state;
    }

    // check, if new slave state has to be acknowledged
    if (slave->current_state & EC_SLAVE_STATE_ACK_ERR && !slave->error_flag) {
        ec_fsm_change_ack(&fsm->fsm_change, slave);
        ec_fsm_change_exec(&fsm->fsm_change);
        fsm->state = ec_fsm_master_state_acknowledge;
        return;
    }

    ec_fsm_master_action_next_slave_state(fsm);
}

/*****************************************************************************/

/**
   Master state: ACKNOWLEDGE
*/

void ec_fsm_master_state_acknowledge(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_change_exec(&fsm->fsm_change)) return;

    if (!ec_fsm_change_success(&fsm->fsm_change)) {
        fsm->slave->error_flag = 1;
        EC_ERR("Failed to acknowledge state change on slave %i.\n",
               slave->ring_position);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    ec_fsm_master_action_next_slave_state(fsm);
}

/*****************************************************************************/

/**
   Master state: VALIDATE_VENDOR.
   Validates the vendor ID of a slave.
*/

void ec_fsm_master_state_validate_vendor(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_sii_exec(&fsm->fsm_sii)) return;

    if (!ec_fsm_sii_success(&fsm->fsm_sii)) {
        fsm->slave->error_flag = 1;
        EC_ERR("Failed to validate vendor ID of slave %i.\n",
               slave->ring_position);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    if (EC_READ_U32(fsm->fsm_sii.value) != slave->sii_vendor_id) {
        EC_ERR("Slave %i has an invalid vendor ID!\n", slave->ring_position);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    // vendor ID is ok. check product code.
    fsm->state = ec_fsm_master_state_validate_product;
    ec_fsm_sii_read(&fsm->fsm_sii, slave, 0x000A, EC_FSM_SII_POSITION);
    ec_fsm_sii_exec(&fsm->fsm_sii); // execute immediately
}

/*****************************************************************************/

/**
   Master action: ADDRESS.
   Looks for slave, that have lost their configuration and writes
   their station address, so that they can be reconfigured later.
*/

void ec_fsm_master_action_addresses(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;

    while (fsm->slave->online) {
        if (fsm->slave->list.next == &fsm->master->slaves) { // last slave?
            fsm->state = ec_fsm_master_state_start;
            fsm->state(fsm); // execute immediately
            return;
        }
        // check next slave
        fsm->slave = list_entry(fsm->slave->list.next, ec_slave_t, list);
    }

    if (fsm->master->debug_level)
        EC_DBG("Reinitializing slave %i.\n", fsm->slave->ring_position);

    // write station address
    ec_datagram_apwr(datagram, fsm->slave->ring_position, 0x0010, 2);
    EC_WRITE_U16(datagram->data, fsm->slave->station_address);
    ec_master_queue_datagram(fsm->master, datagram);
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_master_state_rewrite_addresses;
}

/*****************************************************************************/

/**
   Master state: VALIDATE_PRODUCT.
   Validates the product ID of a slave.
*/

void ec_fsm_master_state_validate_product(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_sii_exec(&fsm->fsm_sii)) return;

    if (!ec_fsm_sii_success(&fsm->fsm_sii)) {
        fsm->slave->error_flag = 1;
        EC_ERR("Failed to validate product code of slave %i.\n",
               slave->ring_position);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    if (EC_READ_U32(fsm->fsm_sii.value) != slave->sii_product_code) {
        EC_ERR("Slave %i: invalid product code!\n", slave->ring_position);
        EC_ERR("expected 0x%08X, got 0x%08X.\n", slave->sii_product_code,
               EC_READ_U32(fsm->fsm_sii.value));
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    // have all states been validated?
    if (slave->list.next == &fsm->master->slaves) {
        fsm->slave = list_entry(fsm->master->slaves.next, ec_slave_t, list);
        // start writing addresses to offline slaves
        ec_fsm_master_action_addresses(fsm);
        return;
    }

    // validate next slave
    fsm->slave = list_entry(fsm->slave->list.next, ec_slave_t, list);
    fsm->state = ec_fsm_master_state_validate_vendor;
    ec_fsm_sii_read(&fsm->fsm_sii, slave, 0x0008, EC_FSM_SII_POSITION);
    ec_fsm_sii_exec(&fsm->fsm_sii); // execute immediately
}

/*****************************************************************************/

/**
   Master state: REWRITE ADDRESS.
   Checks, if the new station address has been written to the slave.
*/

void ec_fsm_master_state_rewrite_addresses(ec_fsm_master_t *fsm
                                     /**< master state machine */
                                     )
{
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = fsm->datagram;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--) {
        ec_master_queue_datagram(fsm->master, fsm->datagram);
        return;
    }

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        EC_ERR("Failed to receive address datagram for slave %i"
                " (datagram state %i).\n",
                slave->ring_position, datagram->state);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    if (datagram->working_counter != 1) {
        EC_ERR("Failed to write station address - slave %i did not respond.\n",
               slave->ring_position);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    if (fsm->slave->list.next == &fsm->master->slaves) { // last slave?
        fsm->state = ec_fsm_master_state_start;
        fsm->state(fsm); // execute immediately
        return;
    }

    // check next slave
    fsm->slave = list_entry(fsm->slave->list.next, ec_slave_t, list);
    // Write new station address to slave
    ec_fsm_master_action_addresses(fsm);
}

/*****************************************************************************/

/**
   Master state: SCAN SLAVES.
   Executes the sub-statemachine for the scanning of a slave.
*/

void ec_fsm_master_state_scan_slaves(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave;

    if (ec_fsm_slave_exec(&fsm->fsm_slave)) // execute slave state machine
        return;

    // another slave to fetch?
    if (fsm->slave->list.next != &master->slaves) {
        fsm->slave = list_entry(fsm->slave->list.next, ec_slave_t, list);
        ec_fsm_slave_start_scan(&fsm->fsm_slave, fsm->slave);
        ec_fsm_slave_exec(&fsm->fsm_slave); // execute immediately
        return;
    }

    EC_INFO("Bus scanning completed.\n");

    ec_master_calc_addressing(master);

    // set initial states of all slaves to PREOP to make mailbox
    // communication possible
    list_for_each_entry(slave, &master->slaves, list) {
        ec_slave_request_state(slave, EC_SLAVE_STATE_PREOP);
    }

    fsm->state = ec_fsm_master_state_end;
}

/*****************************************************************************/

/**
   Master state: CONFIGURE SLAVES.
   Starts configuring a slave.
*/

void ec_fsm_master_state_configure_slave(ec_fsm_master_t *fsm
                                   /**< master state machine */
                                   )
{
    if (ec_fsm_slave_exec(&fsm->fsm_slave)) // execute slave's state machine
        return;

    ec_fsm_master_action_process_states(fsm);
}

/*****************************************************************************/

/**
   Master state: WRITE EEPROM.
*/

void ec_fsm_master_state_write_eeprom(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_master_t *master = fsm->master;
    ec_eeprom_write_request_t *request = fsm->eeprom_request;
    ec_slave_t *slave = request->slave;

    if (ec_fsm_sii_exec(&fsm->fsm_sii)) return;

    if (!ec_fsm_sii_success(&fsm->fsm_sii)) {
        slave->error_flag = 1;
        EC_ERR("Failed to write EEPROM contents to slave %i.\n",
               slave->ring_position);
        request->state = EC_EEPROM_REQ_ERROR;
        wake_up_interruptible(&master->eeprom_queue);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    fsm->eeprom_index++;
    if (fsm->eeprom_index < request->size) {
        ec_fsm_sii_write(&fsm->fsm_sii, slave,
                request->offset + fsm->eeprom_index,
                request->words + fsm->eeprom_index,
                EC_FSM_SII_NODE);
        ec_fsm_sii_exec(&fsm->fsm_sii); // execute immediately
        return;
    }

    // finished writing EEPROM
    EC_INFO("Finished writing EEPROM of slave %i.\n", slave->ring_position);
    request->state = EC_EEPROM_REQ_COMPLETED;
    wake_up_interruptible(&master->eeprom_queue);

    // TODO: Evaluate new EEPROM contents!

    // check for another EEPROM write request
    if (ec_fsm_master_action_process_eeprom(fsm))
        return; // processing another request

    // restart master state machine.
    fsm->state = ec_fsm_master_state_start;
    fsm->state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   Master state: SDODICT.
*/

void ec_fsm_master_state_sdodict(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = fsm->master;

    if (ec_fsm_coe_exec(&fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(&fsm->fsm_coe)) {
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    // SDO dictionary fetching finished

    if (master->debug_level) {
        unsigned int sdo_count, entry_count;
        ec_slave_sdo_dict_info(slave, &sdo_count, &entry_count);
        EC_DBG("Fetched %i SDOs and %i entries from slave %i.\n",
               sdo_count, entry_count, slave->ring_position);
    }

    // restart master state machine.
    fsm->state = ec_fsm_master_state_start;
    fsm->state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   Master state: SDO REQUEST.
*/

void ec_fsm_master_state_sdo_request(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_master_t *master = fsm->master;
    ec_sdo_request_t *request = fsm->sdo_request;

    if (ec_fsm_coe_exec(&fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(&fsm->fsm_coe)) {
        request->return_code = -1;
        master->sdo_seq_master++;
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    // SDO dictionary fetching finished

    request->return_code = 1;
    master->sdo_seq_master++;

    // restart master state machine.
    fsm->state = ec_fsm_master_state_start;
    fsm->state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   State: ERROR.
*/

void ec_fsm_master_state_error(
        ec_fsm_master_t *fsm /**< master state machine */
        )
{
    fsm->state = ec_fsm_master_state_start;
}

/*****************************************************************************/

/**
   State: END.
*/

void ec_fsm_master_state_end(ec_fsm_master_t *fsm /**< master state machine */)
{
    fsm->state = ec_fsm_master_state_start;
}

/*****************************************************************************/

