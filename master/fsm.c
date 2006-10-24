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
#include "fsm.h"
#include "master.h"
#include "mailbox.h"

/*****************************************************************************/

void ec_fsm_master_start(ec_fsm_t *);
void ec_fsm_master_broadcast(ec_fsm_t *);
void ec_fsm_master_read_states(ec_fsm_t *);
void ec_fsm_master_validate_vendor(ec_fsm_t *);
void ec_fsm_master_validate_product(ec_fsm_t *);
void ec_fsm_master_rewrite_addresses(ec_fsm_t *);
void ec_fsm_master_configure_slave(ec_fsm_t *);
void ec_fsm_master_scan_slaves(ec_fsm_t *);
void ec_fsm_master_write_eeprom(ec_fsm_t *);
void ec_fsm_master_sdodict(ec_fsm_t *);
void ec_fsm_master_sdo_request(ec_fsm_t *);

void ec_fsm_startup_start(ec_fsm_t *);
void ec_fsm_startup_broadcast(ec_fsm_t *);
void ec_fsm_startup_scan(ec_fsm_t *);

void ec_fsm_configuration_start(ec_fsm_t *);
void ec_fsm_configuration_conf(ec_fsm_t *);

void ec_fsm_slavescan_start(ec_fsm_t *);
void ec_fsm_slavescan_address(ec_fsm_t *);
void ec_fsm_slavescan_state(ec_fsm_t *);
void ec_fsm_slavescan_base(ec_fsm_t *);
void ec_fsm_slavescan_datalink(ec_fsm_t *);
void ec_fsm_slavescan_eeprom_size(ec_fsm_t *);
void ec_fsm_slavescan_eeprom_data(ec_fsm_t *);

void ec_fsm_slaveconf_init(ec_fsm_t *);
void ec_fsm_slaveconf_sync(ec_fsm_t *);
void ec_fsm_slaveconf_preop(ec_fsm_t *);
void ec_fsm_slaveconf_fmmu(ec_fsm_t *);
void ec_fsm_slaveconf_sdoconf(ec_fsm_t *);
void ec_fsm_slaveconf_saveop(ec_fsm_t *);
void ec_fsm_slaveconf_op(ec_fsm_t *);

void ec_fsm_end(ec_fsm_t *);
void ec_fsm_error(ec_fsm_t *);

/*****************************************************************************/

/**
   Constructor.
*/

int ec_fsm_init(ec_fsm_t *fsm, /**< finite state machine */
                ec_master_t *master /**< EtherCAT master */
                )
{
    fsm->master = master;
    fsm->master_state = ec_fsm_master_start;
    fsm->master_slaves_responding = 0;
    fsm->master_slave_states = EC_SLAVE_STATE_UNKNOWN;
    fsm->master_validation = 0;

    ec_datagram_init(&fsm->datagram);
    if (ec_datagram_prealloc(&fsm->datagram, EC_MAX_DATA_SIZE)) {
        EC_ERR("Failed to allocate FSM datagram.\n");
        return -1;
    }

    // init sub-state-machines
    ec_fsm_sii_init(&fsm->fsm_sii, &fsm->datagram);
    ec_fsm_change_init(&fsm->fsm_change, &fsm->datagram);
    ec_fsm_coe_init(&fsm->fsm_coe, &fsm->datagram);

    return 0;
}

/*****************************************************************************/

/**
   Destructor.
*/

void ec_fsm_clear(ec_fsm_t *fsm /**< finite state machine */)
{
    // clear sub-state machines
    ec_fsm_sii_clear(&fsm->fsm_sii);
    ec_fsm_change_clear(&fsm->fsm_change);
    ec_fsm_coe_clear(&fsm->fsm_coe);

    ec_datagram_clear(&fsm->datagram);
}

/*****************************************************************************/

/**
   Resets the state machine.
*/

void ec_fsm_reset(ec_fsm_t *fsm /**< finite state machine */)
{
    fsm->master_state = ec_fsm_master_start;
    fsm->master_slaves_responding = 0;
    fsm->master_slave_states = EC_SLAVE_STATE_UNKNOWN;
}

/*****************************************************************************/

/**
   Executes the current state of the state machine.
   \return false, if state machine has terminated
*/

int ec_fsm_exec(ec_fsm_t *fsm /**< finite state machine */)
{
    fsm->master_state(fsm);

    return fsm->master_state != ec_fsm_end &&
        fsm->master_state != ec_fsm_error;
}

/*****************************************************************************/

/**
   Initializes the master startup state machine.
*/

void ec_fsm_startup(ec_fsm_t *fsm)
{
    fsm->master_state = ec_fsm_startup_start;
}

/*****************************************************************************/

/**
   Returns, if the master startup state machine terminated with success.
   \return non-zero if successful.
*/

int ec_fsm_startup_success(ec_fsm_t *fsm /**< Finite state machine */)
{
    return fsm->master_state == ec_fsm_end;
}

/*****************************************************************************/

/**
   Initializes the master configuration state machine.
*/

void ec_fsm_configuration(ec_fsm_t *fsm)
{
    fsm->master_state = ec_fsm_configuration_start;
}

/*****************************************************************************/

/**
   Returns, if the master confuguration state machine terminated with success.
   \return non-zero if successful.
*/

int ec_fsm_configuration_success(ec_fsm_t *fsm /**< Finite state machine */)
{
    return fsm->master_state == ec_fsm_end;
}

/******************************************************************************
 *  master startup state machine
 *****************************************************************************/

/**
   Master state: START.
   Starts with getting slave count and slave states.
*/

void ec_fsm_startup_start(ec_fsm_t *fsm)
{
    ec_datagram_brd(&fsm->datagram, 0x0130, 2);
    ec_master_queue_datagram(fsm->master, &fsm->datagram);
    fsm->master_state = ec_fsm_startup_broadcast;
}

/*****************************************************************************/

/**
   Master state: BROADCAST.
   Processes the broadcast read slave count and slaves states.
*/

void ec_fsm_startup_broadcast(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;
    unsigned int i;
    ec_slave_t *slave;
    ec_master_t *master = fsm->master;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        EC_ERR("Failed to receive broadcast datagram.\n");
        fsm->master_state = ec_fsm_error;
        return;
    }

    EC_INFO("Scanning bus.\n");

    ec_master_clear_slaves(master);

    master->slave_count = datagram->working_counter;

    if (!master->slave_count) {
        // no slaves present -> finish state machine.
        fsm->master_state = ec_fsm_end;
        return;
    }

    // init slaves
    for (i = 0; i < master->slave_count; i++) {
        if (!(slave = (ec_slave_t *) kmalloc(sizeof(ec_slave_t),
                                             GFP_KERNEL))) {
            EC_ERR("Failed to allocate slave %i!\n", i);
            fsm->master_state = ec_fsm_error;
            return;
        }

        if (ec_slave_init(slave, master, i, i + 1)) {
            fsm->master_state = ec_fsm_error;
            return;
        }

        if (kobject_add(&slave->kobj)) {
            EC_ERR("Failed to add kobject.\n");
            kobject_put(&slave->kobj); // free
            fsm->master_state = ec_fsm_error;
            return;
        }

        list_add_tail(&slave->list, &master->slaves);
    }

    // begin scanning of slaves
    fsm->slave = list_entry(master->slaves.next, ec_slave_t, list);
    fsm->slave_state = ec_fsm_slavescan_start;
    fsm->master_state = ec_fsm_startup_scan;
    fsm->master_state(fsm); // execute immediately
    return;
}

/*****************************************************************************/

/**
   Master state: SCAN.
   Executes the sub-statemachine for the scanning of a slave.
*/

void ec_fsm_startup_scan(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave = fsm->slave;

    fsm->slave_state(fsm); // execute slave state machine

    if (fsm->slave_state == ec_fsm_error) {
        EC_ERR("Slave scanning failed.\n");
        fsm->master_state = ec_fsm_error;
        return;
    }

    if (fsm->slave_state != ec_fsm_end) return;

    // another slave to scan?
    if (slave->list.next != &master->slaves) {
        fsm->slave = list_entry(fsm->slave->list.next, ec_slave_t, list);
        fsm->slave_state = ec_fsm_slavescan_start;
        fsm->slave_state(fsm); // execute immediately
        return;
    }

    EC_INFO("Bus scanning completed.\n");

    ec_master_calc_addressing(master);

    fsm->master_state = ec_fsm_end;
}

/******************************************************************************
 *  master configuration state machine
 *****************************************************************************/

/**
   Master configuration state machine: START.
*/

void ec_fsm_configuration_start(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_master_t *master = fsm->master;

    if (list_empty(&master->slaves)) {
        fsm->master_state = ec_fsm_end;
        return;
    }

    // begin configuring slaves
    fsm->slave = list_entry(master->slaves.next, ec_slave_t, list);
    fsm->slave_state = ec_fsm_slaveconf_init;
    ec_fsm_change(&fsm->fsm_change, fsm->slave, EC_SLAVE_STATE_INIT);
    fsm->master_state = ec_fsm_configuration_conf;
    fsm->master_state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   Master state: CONF.
*/

void ec_fsm_configuration_conf(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave = fsm->slave;

    fsm->slave_state(fsm); // execute slave's state machine

    if (fsm->slave_state == ec_fsm_error) {
        fsm->master_state = ec_fsm_error;
        return;
    }

    if (fsm->slave_state != ec_fsm_end) return;

    // another slave to configure?
    if (slave->list.next != &master->slaves) {
        fsm->slave = list_entry(fsm->slave->list.next, ec_slave_t, list);
        fsm->slave_state = ec_fsm_slaveconf_init;
        ec_fsm_change(&fsm->fsm_change, fsm->slave, EC_SLAVE_STATE_INIT);
        fsm->master_state(fsm); // execute immediately
        return;
    }

    fsm->master_state = ec_fsm_end;
}

/******************************************************************************
 *  operation / idle state machine
 *****************************************************************************/

/**
   Master state: START.
   Starts with getting slave count and slave states.
*/

void ec_fsm_master_start(ec_fsm_t *fsm)
{
    ec_datagram_brd(&fsm->datagram, 0x0130, 2);
    ec_master_queue_datagram(fsm->master, &fsm->datagram);
    fsm->master_state = ec_fsm_master_broadcast;
}

/*****************************************************************************/

/**
   Master state: BROADCAST.
   Processes the broadcast read slave count and slaves states.
*/

void ec_fsm_master_broadcast(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;
    unsigned int topology_change, states_change, i;
    ec_slave_t *slave;
    ec_master_t *master = fsm->master;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        if (!master->device->link_state) {
            fsm->master_slaves_responding = 0;
            list_for_each_entry(slave, &master->slaves, list) {
                slave->online = 0;
            }
        }
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    topology_change = (datagram->working_counter !=
                       fsm->master_slaves_responding);
    states_change = (EC_READ_U8(datagram->data) != fsm->master_slave_states);

    fsm->master_slave_states = EC_READ_U8(datagram->data);
    fsm->master_slaves_responding = datagram->working_counter;

    if (topology_change) {
        EC_INFO("%i slave%s responding.\n",
                fsm->master_slaves_responding,
                fsm->master_slaves_responding == 1 ? "" : "s");

        if (master->mode == EC_MASTER_MODE_OPERATION) {
            if (fsm->master_slaves_responding == master->slave_count) {
                fsm->master_validation = 1; // start validation later
            }
            else {
                EC_WARN("Invalid slave count. Bus in tainted state.\n");
            }
        }
    }

    if (states_change) {
        char states[EC_STATE_STRING_SIZE];
        ec_state_string(fsm->master_slave_states, states);
        EC_INFO("Slave states: %s.\n", states);
    }

    // topology change in idle mode: clear all slaves and scan the bus
    if (topology_change && master->mode == EC_MASTER_MODE_IDLE) {
        EC_INFO("Scanning bus.\n");

        ec_master_eoe_stop(master);
        ec_master_clear_slaves(master);

        master->slave_count = datagram->working_counter;

        if (!master->slave_count) {
            // no slaves present -> finish state machine.
            fsm->master_state = ec_fsm_master_start;
            fsm->master_state(fsm); // execute immediately
            return;
        }

        // init slaves
        for (i = 0; i < master->slave_count; i++) {
            if (!(slave = (ec_slave_t *) kmalloc(sizeof(ec_slave_t),
                                                 GFP_ATOMIC))) {
                EC_ERR("Failed to allocate slave %i!\n", i);
                ec_master_clear_slaves(master);
                fsm->master_state = ec_fsm_master_start;
                fsm->master_state(fsm); // execute immediately
                return;
            }

            if (ec_slave_init(slave, master, i, i + 1)) {
                // freeing of "slave" already done
                ec_master_clear_slaves(master);
                fsm->master_state = ec_fsm_master_start;
                fsm->master_state(fsm); // execute immediately
                return;
            }

            if (kobject_add(&slave->kobj)) {
                EC_ERR("Failed to add kobject.\n");
                kobject_put(&slave->kobj); // free
                ec_master_clear_slaves(master);
                fsm->master_state = ec_fsm_master_start;
                fsm->master_state(fsm); // execute immediately
                return;
            }

            list_add_tail(&slave->list, &master->slaves);
        }

        // begin scanning of slaves
        fsm->slave = list_entry(master->slaves.next, ec_slave_t, list);
        fsm->slave_state = ec_fsm_slavescan_start;
        fsm->master_state = ec_fsm_master_scan_slaves;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    // fetch state from each slave
    fsm->slave = list_entry(master->slaves.next, ec_slave_t, list);
    ec_datagram_nprd(&fsm->datagram, fsm->slave->station_address, 0x0130, 2);
    ec_master_queue_datagram(master, &fsm->datagram);
    fsm->master_state = ec_fsm_master_read_states;
}

/*****************************************************************************/

/**
   Master action: PROC_STATES.
   Processes the slave states.
*/

void ec_fsm_master_action_process_states(ec_fsm_t *fsm
                                         /**< finite state machine */
                                         )
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave;
    char old_state[EC_STATE_STRING_SIZE], new_state[EC_STATE_STRING_SIZE];
    ec_sdo_request_t *request, *next_request;

    // check if any slaves are not in the state, they're supposed to be
    list_for_each_entry(slave, &master->slaves, list) {
        if (slave->error_flag
            || !slave->online
            || slave->requested_state == EC_SLAVE_STATE_UNKNOWN
            || (slave->current_state == slave->requested_state
                && (slave->configured
                    || slave->current_state == EC_SLAVE_STATE_INIT))) continue;

        if (!slave->configured
            && slave->current_state != EC_SLAVE_STATE_INIT) {
            ec_state_string(slave->current_state, old_state);
            EC_INFO("Reconfiguring slave %i (%s).\n",
                    slave->ring_position, old_state);
        }

        if (slave->current_state != slave->requested_state) {
            ec_state_string(slave->current_state, old_state);
            ec_state_string(slave->requested_state, new_state);
            EC_INFO("Changing state of slave %i from %s to %s.\n",
                    slave->ring_position, old_state, new_state);
        }

        fsm->slave = slave;
        fsm->slave_state = ec_fsm_slaveconf_init;
        ec_fsm_change(&fsm->fsm_change, fsm->slave, EC_SLAVE_STATE_INIT);
        fsm->master_state = ec_fsm_master_configure_slave;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    // Check, if EoE processing has to be started
    ec_master_eoe_start(master);

    // check, if there are pending SDO requests
    list_for_each_entry_safe(request, next_request,
                             &master->sdo_requests, queue) {
        // TODO: critical section!
        list_del_init(&request->queue);

        slave = request->sdo->slave;

        if (slave->current_state == EC_SLAVE_STATE_INIT
            || !slave->online
            || slave->error_flag) {
            request->return_code = -1;
            wake_up_interruptible(&master->sdo_wait_queue);
            continue;
        }

        // start uploading SDO
        fsm->slave = slave;
        fsm->master_state = ec_fsm_master_sdo_request;
        fsm->sdo_request = request;
        ec_fsm_coe_upload(&fsm->fsm_coe, slave, request);
        ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
        return;
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

        if (kobject_add(&slave->sdo_kobj)) {
            EC_ERR("Failed to add SDO kobj of slave %i.\n",
                   slave->ring_position);
            slave->error_flag = 1;
            fsm->master_state = ec_fsm_master_start;
            fsm->master_state(fsm); // execute immediately
            return;
        }

        slave->sdo_dictionary_fetched = 1;

        // start fetching SDO dictionary
        fsm->slave = slave;
        fsm->master_state = ec_fsm_master_sdodict;
        ec_fsm_coe_dictionary(&fsm->fsm_coe, slave);
        ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
        return;
    }

    if (master->mode == EC_MASTER_MODE_IDLE) {
        // nothing to configure. check for pending EEPROM write operations.
        list_for_each_entry(slave, &master->slaves, list) {
            if (!slave->new_eeprom_data) continue;

            if (!slave->online || slave->error_flag) {
                kfree(slave->new_eeprom_data);
                slave->new_eeprom_data = NULL;
                EC_ERR("Discarding EEPROM data, slave %i not ready.\n",
                       slave->ring_position);
                continue;
            }

            // found pending EEPROM write operation. execute it!
            EC_INFO("Writing EEPROM of slave %i...\n", slave->ring_position);
            fsm->sii_offset = 0x0000;
            ec_fsm_sii_write(&fsm->fsm_sii, slave, fsm->sii_offset,
                             slave->new_eeprom_data, EC_FSM_SII_NODE);
            fsm->master_state = ec_fsm_master_write_eeprom;
            fsm->master_state(fsm); // execute immediately
            return;
        }
    }

    // nothing to do. restart master state machine.
    fsm->master_state = ec_fsm_master_start;
    fsm->master_state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   Master action: Get state of next slave.
*/

void ec_fsm_master_action_next_slave_state(ec_fsm_t *fsm
                                           /**< finite state machine */)
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave = fsm->slave;

    // is there another slave to query?
    if (slave->list.next != &master->slaves) {
        // process next slave
        fsm->slave = list_entry(fsm->slave->list.next, ec_slave_t, list);
        ec_datagram_nprd(&fsm->datagram, fsm->slave->station_address,
                         0x0130, 2);
        ec_master_queue_datagram(master, &fsm->datagram);
        fsm->master_state = ec_fsm_master_read_states;
        return;
    }

    // all slave states read

    // check, if a bus validation has to be done
    if (fsm->master_validation) {
        fsm->master_validation = 0;
        list_for_each_entry(slave, &master->slaves, list) {
            if (slave->online) continue;

            // At least one slave is offline. validate!
            EC_INFO("Validating bus.\n");
            fsm->slave = list_entry(master->slaves.next, ec_slave_t, list);
            fsm->master_state = ec_fsm_master_validate_vendor;
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

void ec_fsm_master_read_states(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = &fsm->datagram;
    uint8_t new_state;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    // did the slave not respond to its station address?
    if (datagram->working_counter != 1) {
        if (slave->online) {
            slave->online = 0;
            EC_INFO("Slave %i: offline.\n", slave->ring_position);
        }
        ec_fsm_master_action_next_slave_state(fsm);
        return;
    }

    // slave responded
    new_state = EC_READ_U8(datagram->data);
    if (!slave->online) { // slave was offline before
        char cur_state[EC_STATE_STRING_SIZE];
        slave->online = 1;
        slave->error_flag = 0; // clear error flag
        slave->current_state = new_state;
        ec_state_string(slave->current_state, cur_state);
        EC_INFO("Slave %i: online (%s).\n", slave->ring_position, cur_state);
    }
    else if (new_state != slave->current_state) {
        char old_state[EC_STATE_STRING_SIZE], cur_state[EC_STATE_STRING_SIZE];
        ec_state_string(slave->current_state, old_state);
        ec_state_string(new_state, cur_state);
        EC_INFO("Slave %i: %s -> %s.\n",
                slave->ring_position, old_state, cur_state);
        slave->current_state = new_state;
    }

    ec_fsm_master_action_next_slave_state(fsm);
}

/*****************************************************************************/

/**
   Master state: VALIDATE_VENDOR.
   Validates the vendor ID of a slave.
*/

void ec_fsm_master_validate_vendor(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_sii_exec(&fsm->fsm_sii)) return;

    if (!ec_fsm_sii_success(&fsm->fsm_sii)) {
        fsm->slave->error_flag = 1;
        EC_ERR("Failed to validate vendor ID of slave %i.\n",
               slave->ring_position);
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    if (EC_READ_U32(fsm->fsm_sii.value) != slave->sii_vendor_id) {
        EC_ERR("Slave %i: invalid vendor ID!\n", slave->ring_position);
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    // vendor ID is ok. check product code.
    fsm->master_state = ec_fsm_master_validate_product;
    ec_fsm_sii_read(&fsm->fsm_sii, slave, 0x000A, EC_FSM_SII_POSITION);
    ec_fsm_sii_exec(&fsm->fsm_sii); // execute immediately
}

/*****************************************************************************/

/**
   Master action: ADDRESS.
   Looks for slave, that have lost their configuration and writes
   their station address, so that they can be reconfigured later.
*/

void ec_fsm_master_action_addresses(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;

    while (fsm->slave->online) {
        if (fsm->slave->list.next == &fsm->master->slaves) { // last slave?
            fsm->master_state = ec_fsm_master_start;
            fsm->master_state(fsm); // execute immediately
            return;
        }
        // check next slave
        fsm->slave = list_entry(fsm->slave->list.next, ec_slave_t, list);
    }

    EC_INFO("Reinitializing slave %i.\n", fsm->slave->ring_position);

    // write station address
    ec_datagram_apwr(datagram, fsm->slave->ring_position, 0x0010, 2);
    EC_WRITE_U16(datagram->data, fsm->slave->station_address);
    ec_master_queue_datagram(fsm->master, datagram);
    fsm->master_state = ec_fsm_master_rewrite_addresses;
}

/*****************************************************************************/

/**
   Master state: VALIDATE_PRODUCT.
   Validates the product ID of a slave.
*/

void ec_fsm_master_validate_product(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_sii_exec(&fsm->fsm_sii)) return;

    if (!ec_fsm_sii_success(&fsm->fsm_sii)) {
        fsm->slave->error_flag = 1;
        EC_ERR("Failed to validate product code of slave %i.\n",
               slave->ring_position);
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    if (EC_READ_U32(fsm->fsm_sii.value) != slave->sii_product_code) {
        EC_ERR("Slave %i: invalid product code!\n", slave->ring_position);
        EC_ERR("expected 0x%08X, got 0x%08X.\n", slave->sii_product_code,
               EC_READ_U32(fsm->fsm_sii.value));
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
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
    fsm->master_state = ec_fsm_master_validate_vendor;
    ec_fsm_sii_read(&fsm->fsm_sii, slave, 0x0008, EC_FSM_SII_POSITION);
    ec_fsm_sii_exec(&fsm->fsm_sii); // execute immediately
}

/*****************************************************************************/

/**
   Master state: REWRITE ADDRESS.
   Checks, if the new station address has been written to the slave.
*/

void ec_fsm_master_rewrite_addresses(ec_fsm_t *fsm
                                     /**< finite state machine */
                                     )
{
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = &fsm->datagram;

    if (datagram->state != EC_DATAGRAM_RECEIVED
        || datagram->working_counter != 1) {
        EC_ERR("Failed to write station address on slave %i.\n",
               slave->ring_position);
    }

    if (fsm->slave->list.next == &fsm->master->slaves) { // last slave?
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
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

void ec_fsm_master_scan_slaves(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave = fsm->slave;

    fsm->slave_state(fsm); // execute slave state machine

    if (fsm->slave_state != ec_fsm_end
        && fsm->slave_state != ec_fsm_error) return;

    // another slave to fetch?
    if (slave->list.next != &master->slaves) {
        fsm->slave = list_entry(fsm->slave->list.next, ec_slave_t, list);
        fsm->slave_state = ec_fsm_slavescan_start;
        fsm->slave_state(fsm); // execute immediately
        return;
    }

    EC_INFO("Bus scanning completed.\n");

    ec_master_calc_addressing(master);

    // set initial states of all slaves to PREOP to make mailbox
    // communication possible
    list_for_each_entry(slave, &master->slaves, list) {
        slave->requested_state = EC_SLAVE_STATE_PREOP;
    }

    fsm->master_state = ec_fsm_master_start;
    fsm->master_state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   Master state: CONFIGURE SLAVES.
   Starts configuring a slave.
*/

void ec_fsm_master_configure_slave(ec_fsm_t *fsm
                                   /**< finite state machine */
                                   )
{
    fsm->slave_state(fsm); // execute slave's state machine

    if (fsm->slave_state != ec_fsm_end
        && fsm->slave_state != ec_fsm_error) return;

    ec_fsm_master_action_process_states(fsm);
}

/*****************************************************************************/

/**
   Master state: WRITE EEPROM.
*/

void ec_fsm_master_write_eeprom(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_sii_exec(&fsm->fsm_sii)) return;

    if (!ec_fsm_sii_success(&fsm->fsm_sii)) {
        fsm->slave->error_flag = 1;
        EC_ERR("Failed to write EEPROM contents to slave %i.\n",
               slave->ring_position);
        kfree(slave->new_eeprom_data);
        slave->new_eeprom_data = NULL;
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    fsm->sii_offset++;
    if (fsm->sii_offset < slave->new_eeprom_size) {
        ec_fsm_sii_write(&fsm->fsm_sii, slave, fsm->sii_offset,
                         slave->new_eeprom_data + fsm->sii_offset,
                         EC_FSM_SII_NODE);
        ec_fsm_sii_exec(&fsm->fsm_sii); // execute immediately
        return;
    }

    // finished writing EEPROM
    EC_INFO("Finished writing EEPROM of slave %i.\n", slave->ring_position);
    kfree(slave->new_eeprom_data);
    slave->new_eeprom_data = NULL;

    // TODO: Evaluate new EEPROM contents!

    // restart master state machine.
    fsm->master_state = ec_fsm_master_start;
    fsm->master_state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   Master state: SDODICT.
*/

void ec_fsm_master_sdodict(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = fsm->master;

    if (ec_fsm_coe_exec(&fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(&fsm->fsm_coe)) {
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
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
    fsm->master_state = ec_fsm_master_start;
    fsm->master_state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   Master state: SDO REQUEST.
*/

void ec_fsm_master_sdo_request(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_master_t *master = fsm->master;
    ec_sdo_request_t *request = fsm->sdo_request;

    if (ec_fsm_coe_exec(&fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(&fsm->fsm_coe)) {
        request->return_code = -1;
        wake_up_interruptible(&master->sdo_wait_queue);
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    // SDO dictionary fetching finished

    request->return_code = 1;
    wake_up_interruptible(&master->sdo_wait_queue);

    // restart master state machine.
    fsm->master_state = ec_fsm_master_start;
    fsm->master_state(fsm); // execute immediately
}

/******************************************************************************
 *  slave scan state machine
 *****************************************************************************/

/**
   Slave scan state: START.
   First state of the slave state machine. Writes the station address to the
   slave, according to its ring position.
*/

void ec_fsm_slavescan_start(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;

    // write station address
    ec_datagram_apwr(datagram, fsm->slave->ring_position, 0x0010, 2);
    EC_WRITE_U16(datagram->data, fsm->slave->station_address);
    ec_master_queue_datagram(fsm->master, datagram);
    fsm->slave_state = ec_fsm_slavescan_address;
}

/*****************************************************************************/

/**
   Slave scan state: ADDRESS.
*/

void ec_fsm_slavescan_address(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;

    if (datagram->state != EC_DATAGRAM_RECEIVED
        || datagram->working_counter != 1) {
        fsm->slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        EC_ERR("Failed to write station address of slave %i.\n",
               fsm->slave->ring_position);
        return;
    }

    // Read AL state
    ec_datagram_nprd(datagram, fsm->slave->station_address, 0x0130, 2);
    ec_master_queue_datagram(fsm->master, datagram);
    fsm->slave_state = ec_fsm_slavescan_state;
}

/*****************************************************************************/

/**
   Slave scan state: STATE.
*/

void ec_fsm_slavescan_state(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state != EC_DATAGRAM_RECEIVED
        || datagram->working_counter != 1) {
        fsm->slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        EC_ERR("Failed to read AL state of slave %i.\n",
               fsm->slave->ring_position);
        return;
    }

    slave->current_state = EC_READ_U8(datagram->data);
    if (slave->current_state & EC_SLAVE_STATE_ACK_ERR) {
        EC_WARN("Slave %i has state error bit set (0x%02X)!\n",
                slave->ring_position, slave->current_state);
    }

    // read base data
    ec_datagram_nprd(datagram, fsm->slave->station_address, 0x0000, 6);
    ec_master_queue_datagram(fsm->master, datagram);
    fsm->slave_state = ec_fsm_slavescan_base;
}

/*****************************************************************************/

/**
   Slave scan state: BASE.
*/

void ec_fsm_slavescan_base(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state != EC_DATAGRAM_RECEIVED
        || datagram->working_counter != 1) {
        fsm->slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        EC_ERR("Failed to read base data of slave %i.\n",
               slave->ring_position);
        return;
    }

    slave->base_type       = EC_READ_U8 (datagram->data);
    slave->base_revision   = EC_READ_U8 (datagram->data + 1);
    slave->base_build      = EC_READ_U16(datagram->data + 2);
    slave->base_fmmu_count = EC_READ_U8 (datagram->data + 4);
    slave->base_sync_count = EC_READ_U8 (datagram->data + 5);

    if (slave->base_fmmu_count > EC_MAX_FMMUS)
        slave->base_fmmu_count = EC_MAX_FMMUS;

    // read data link status
    ec_datagram_nprd(datagram, slave->station_address, 0x0110, 2);
    ec_master_queue_datagram(slave->master, datagram);
    fsm->slave_state = ec_fsm_slavescan_datalink;
}

/*****************************************************************************/

/**
   Slave scan state: DATALINK.
*/

void ec_fsm_slavescan_datalink(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    uint16_t dl_status;
    unsigned int i;

    if (datagram->state != EC_DATAGRAM_RECEIVED
        || datagram->working_counter != 1) {
        fsm->slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        EC_ERR("Failed to read DL status of slave %i.\n",
               slave->ring_position);
        return;
    }

    dl_status = EC_READ_U16(datagram->data);
    for (i = 0; i < 4; i++) {
        slave->dl_link[i] = dl_status & (1 << (4 + i)) ? 1 : 0;
        slave->dl_loop[i] = dl_status & (1 << (8 + i * 2)) ? 1 : 0;
        slave->dl_signal[i] = dl_status & (1 << (9 + i * 2)) ? 1 : 0;
    }

    // Start fetching EEPROM size

    fsm->sii_offset = 0x0040; // first category header
    ec_fsm_sii_read(&fsm->fsm_sii, slave, fsm->sii_offset, EC_FSM_SII_NODE);
    fsm->slave_state = ec_fsm_slavescan_eeprom_size;
    fsm->slave_state(fsm); // execute state immediately
}

/*****************************************************************************/

/**
   Slave scan state: EEPROM SIZE.
*/

void ec_fsm_slavescan_eeprom_size(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;
    uint16_t cat_type, cat_size;

    if (ec_fsm_sii_exec(&fsm->fsm_sii)) return;

    if (!ec_fsm_sii_success(&fsm->fsm_sii)) {
        fsm->slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        EC_ERR("Failed to read EEPROM size of slave %i.\n",
               slave->ring_position);
        return;
    }

    cat_type = EC_READ_U16(fsm->fsm_sii.value);
    cat_size = EC_READ_U16(fsm->fsm_sii.value + 2);

    if (cat_type != 0xFFFF) { // not the last category
        fsm->sii_offset += cat_size + 2;
        ec_fsm_sii_read(&fsm->fsm_sii, slave, fsm->sii_offset,
                        EC_FSM_SII_NODE);
        ec_fsm_sii_exec(&fsm->fsm_sii); // execute state immediately
        return;
    }

    slave->eeprom_size = (fsm->sii_offset + 1) * 2;

    if (slave->eeprom_data) {
        EC_INFO("Freeing old EEPROM data on slave %i...\n",
                slave->ring_position);
        kfree(slave->eeprom_data);
    }

    if (!(slave->eeprom_data =
          (uint8_t *) kmalloc(slave->eeprom_size, GFP_ATOMIC))) {
        fsm->slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        EC_ERR("Failed to allocate EEPROM data on slave %i.\n",
               slave->ring_position);
        return;
    }

    // Start fetching EEPROM contents

    fsm->slave_state = ec_fsm_slavescan_eeprom_data;
    fsm->sii_offset = 0x0000;
    ec_fsm_sii_read(&fsm->fsm_sii, slave, fsm->sii_offset, EC_FSM_SII_NODE);
    ec_fsm_sii_exec(&fsm->fsm_sii); // execute state immediately
}

/*****************************************************************************/

/**
   Slave scan state: EEPROM DATA.
*/

void ec_fsm_slavescan_eeprom_data(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;
    uint16_t *cat_word, cat_type, cat_size;

    if (ec_fsm_sii_exec(&fsm->fsm_sii)) return;

    if (!ec_fsm_sii_success(&fsm->fsm_sii)) {
        fsm->slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        EC_ERR("Failed to fetch EEPROM contents of slave %i.\n",
               slave->ring_position);
        return;
    }

    // 2 words fetched

    if (fsm->sii_offset + 2 <= slave->eeprom_size / 2) { // 2 words fit
        memcpy(slave->eeprom_data + fsm->sii_offset * 2,
               fsm->fsm_sii.value, 4);
    }
    else { // copy the last word
        memcpy(slave->eeprom_data + fsm->sii_offset * 2,
               fsm->fsm_sii.value, 2);
    }

    if (fsm->sii_offset + 2 < slave->eeprom_size / 2) {
        // fetch the next 2 words
        fsm->sii_offset += 2;
        ec_fsm_sii_read(&fsm->fsm_sii, slave, fsm->sii_offset,
                        EC_FSM_SII_NODE);
        ec_fsm_sii_exec(&fsm->fsm_sii); // execute state immediately
        return;
    }

    // Evaluate EEPROM contents

    slave->sii_alias =
        EC_READ_U16(slave->eeprom_data + 2 * 0x0004);
    slave->sii_vendor_id =
        EC_READ_U32(slave->eeprom_data + 2 * 0x0008);
    slave->sii_product_code =
        EC_READ_U32(slave->eeprom_data + 2 * 0x000A);
    slave->sii_revision_number =
        EC_READ_U32(slave->eeprom_data + 2 * 0x000C);
    slave->sii_serial_number =
        EC_READ_U32(slave->eeprom_data + 2 * 0x000E);
    slave->sii_rx_mailbox_offset =
        EC_READ_U16(slave->eeprom_data + 2 * 0x0018);
    slave->sii_rx_mailbox_size =
        EC_READ_U16(slave->eeprom_data + 2 * 0x0019);
    slave->sii_tx_mailbox_offset =
        EC_READ_U16(slave->eeprom_data + 2 * 0x001A);
    slave->sii_tx_mailbox_size =
        EC_READ_U16(slave->eeprom_data + 2 * 0x001B);
    slave->sii_mailbox_protocols =
        EC_READ_U16(slave->eeprom_data + 2 * 0x001C);

    // evaluate category data
    cat_word = (uint16_t *) slave->eeprom_data + 0x0040;
    while (EC_READ_U16(cat_word) != 0xFFFF) {
        cat_type = EC_READ_U16(cat_word) & 0x7FFF;
        cat_size = EC_READ_U16(cat_word + 1);

        switch (cat_type) {
            case 0x000A:
                if (ec_slave_fetch_strings(slave, (uint8_t *) (cat_word + 2)))
                    goto end;
                break;
            case 0x001E:
                ec_slave_fetch_general(slave, (uint8_t *) (cat_word + 2));
                break;
            case 0x0028:
                break;
            case 0x0029:
                if (ec_slave_fetch_sync(slave, (uint8_t *) (cat_word + 2),
                                        cat_size))
                    goto end;
                break;
            case 0x0032:
                if (ec_slave_fetch_pdo(slave, (uint8_t *) (cat_word + 2),
                                       cat_size, EC_TX_PDO))
                    goto end;
                break;
            case 0x0033:
                if (ec_slave_fetch_pdo(slave, (uint8_t *) (cat_word + 2),
                                       cat_size, EC_RX_PDO))
                    goto end;
                break;
            default:
                EC_WARN("Unknown category type 0x%04X in slave %i.\n",
                        cat_type, slave->ring_position);
        }

        cat_word += cat_size + 2;
    }

    fsm->slave_state = ec_fsm_end;
    return;

end:
    EC_ERR("Failed to analyze category data.\n");
    fsm->slave->error_flag = 1;
    fsm->slave_state = ec_fsm_error;
}

/******************************************************************************
 *  slave configuration state machine
 *****************************************************************************/

/**
   Slave configuration state: INIT.
*/

void ec_fsm_slaveconf_init(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = &fsm->datagram;
    const ec_sii_sync_t *sync;
    ec_sii_sync_t mbox_sync;

    if (ec_fsm_change_exec(&fsm->fsm_change)) return;

    if (!ec_fsm_change_success(&fsm->fsm_change)) {
        slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        return;
    }

    slave->configured = 1;

    if (master->debug_level) {
        EC_DBG("Slave %i is now in INIT.\n", slave->ring_position);
    }

    // slave is now in INIT
    if (slave->current_state == slave->requested_state) {
        fsm->slave_state = ec_fsm_end; // successful
        if (master->debug_level) {
            EC_DBG("Finished configuration of slave %i.\n",
                   slave->ring_position);
        }
        return;
    }

    // check and reset CRC fault counters
    //ec_slave_check_crc(slave);
    // TODO: Implement state machine for CRC checking.

    if (!slave->base_sync_count) { // no sync managers
        fsm->slave_state = ec_fsm_slaveconf_preop;
        ec_fsm_change(&fsm->fsm_change, slave, EC_SLAVE_STATE_PREOP);
        ec_fsm_change_exec(&fsm->fsm_change); // execute immediately
        return;
    }

    if (master->debug_level) {
        EC_DBG("Configuring sync managers of slave %i.\n",
               slave->ring_position);
    }

    // configure sync managers
    ec_datagram_npwr(datagram, slave->station_address, 0x0800,
                     EC_SYNC_SIZE * slave->base_sync_count);
    memset(datagram->data, 0x00, EC_SYNC_SIZE * slave->base_sync_count);

    if (list_empty(&slave->sii_syncs)) {
        if (slave->sii_rx_mailbox_offset && slave->sii_tx_mailbox_offset) {
            if (slave->master->debug_level)
                EC_DBG("Guessing sync manager settings for slave %i.\n",
                       slave->ring_position);
            mbox_sync.index = 0;
            mbox_sync.physical_start_address = slave->sii_tx_mailbox_offset;
            mbox_sync.length = slave->sii_tx_mailbox_size;
            mbox_sync.control_register = 0x26;
            mbox_sync.enable = 0x01;
            mbox_sync.est_length = 0;
            ec_sync_config(&mbox_sync, slave,
                           datagram->data + EC_SYNC_SIZE * mbox_sync.index);
            mbox_sync.index = 1;
            mbox_sync.physical_start_address = slave->sii_rx_mailbox_offset;
            mbox_sync.length = slave->sii_rx_mailbox_size;
            mbox_sync.control_register = 0x22;
            mbox_sync.enable = 0x01;
            mbox_sync.est_length = 0;
            ec_sync_config(&mbox_sync, slave,
                           datagram->data + EC_SYNC_SIZE * mbox_sync.index);
        }
    }
    else {
        list_for_each_entry(sync, &slave->sii_syncs, list) {
            if (sync->index >= slave->base_sync_count) {
                EC_ERR("Invalid sync manager configuration found!");
                fsm->slave->error_flag = 1;
                fsm->slave_state = ec_fsm_error;
                return;
            }
            ec_sync_config(sync, slave,
                           datagram->data + EC_SYNC_SIZE * sync->index);
        }
    }

    ec_master_queue_datagram(fsm->master, datagram);
    fsm->slave_state = ec_fsm_slaveconf_sync;
}

/*****************************************************************************/

/**
   Slave configuration state: SYNC.
*/

void ec_fsm_slaveconf_sync(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state != EC_DATAGRAM_RECEIVED
        || datagram->working_counter != 1) {
        slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        EC_ERR("Failed to set sync managers on slave %i.\n",
               slave->ring_position);
        return;
    }

    fsm->slave_state = ec_fsm_slaveconf_preop;
    ec_fsm_change(&fsm->fsm_change, slave, EC_SLAVE_STATE_PREOP);
    ec_fsm_change_exec(&fsm->fsm_change); // execute immediately
}

/*****************************************************************************/

/**
   Slave configuration state: PREOP.
*/

void ec_fsm_slaveconf_preop(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = fsm->master;
    ec_datagram_t *datagram = &fsm->datagram;
    unsigned int j;

    if (ec_fsm_change_exec(&fsm->fsm_change)) return;

    if (!ec_fsm_change_success(&fsm->fsm_change)) {
        slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        return;
    }

    // slave is now in PREOP
    slave->jiffies_preop = fsm->datagram.jiffies_received;

    if (master->debug_level) {
        EC_DBG("Slave %i is now in PREOP.\n", slave->ring_position);
    }

    if (slave->current_state == slave->requested_state) {
        fsm->slave_state = ec_fsm_end; // successful
        if (master->debug_level) {
            EC_DBG("Finished configuration of slave %i.\n",
                   slave->ring_position);
        }
        return;
    }

    if (!slave->base_fmmu_count) { // skip FMMU configuration
        if (list_empty(&slave->sdo_confs)) { // skip SDO configuration
            fsm->slave_state = ec_fsm_slaveconf_saveop;
            ec_fsm_change(&fsm->fsm_change, slave, EC_SLAVE_STATE_SAVEOP);
            ec_fsm_change_exec(&fsm->fsm_change); // execute immediately
            return;
        }

        // start SDO configuration
        fsm->slave_state = ec_fsm_slaveconf_sdoconf;
        fsm->sdodata = list_entry(slave->sdo_confs.next, ec_sdo_data_t, list);
        ec_fsm_coe_download(&fsm->fsm_coe, slave, fsm->sdodata);
        ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
        return;
    }

    // configure FMMUs
    ec_datagram_npwr(datagram, slave->station_address,
                     0x0600, EC_FMMU_SIZE * slave->base_fmmu_count);
    memset(datagram->data, 0x00, EC_FMMU_SIZE * slave->base_fmmu_count);
    for (j = 0; j < slave->fmmu_count; j++) {
        ec_fmmu_config(&slave->fmmus[j], slave,
                       datagram->data + EC_FMMU_SIZE * j);
    }

    ec_master_queue_datagram(master, datagram);
    fsm->slave_state = ec_fsm_slaveconf_fmmu;
}

/*****************************************************************************/

/**
   Slave configuration state: FMMU.
*/

void ec_fsm_slaveconf_fmmu(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state != EC_DATAGRAM_RECEIVED
        || datagram->working_counter != 1) {
        fsm->slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        EC_ERR("Failed to set FMMUs on slave %i.\n",
               fsm->slave->ring_position);
        return;
    }

    // No CoE configuration to be applied? Jump to SAVEOP state.
    if (list_empty(&slave->sdo_confs)) { // skip SDO configuration
        // set state to SAVEOP
        fsm->slave_state = ec_fsm_slaveconf_saveop;
        ec_fsm_change(&fsm->fsm_change, slave, EC_SLAVE_STATE_SAVEOP);
        ec_fsm_change_exec(&fsm->fsm_change); // execute immediately
        return;
    }

    // start SDO configuration
    fsm->slave_state = ec_fsm_slaveconf_sdoconf;
    fsm->sdodata = list_entry(slave->sdo_confs.next, ec_sdo_data_t, list);
    ec_fsm_coe_download(&fsm->fsm_coe, slave, fsm->sdodata);
    ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
}

/*****************************************************************************/

/**
   Slave configuration state: SDOCONF.
*/

void ec_fsm_slaveconf_sdoconf(ec_fsm_t *fsm /**< finite state machine */)
{
    if (ec_fsm_coe_exec(&fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(&fsm->fsm_coe)) {
        fsm->slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        return;
    }

    // Another SDO to configure?
    if (fsm->sdodata->list.next != &fsm->slave->sdo_confs) {
        fsm->sdodata = list_entry(fsm->sdodata->list.next,
                                  ec_sdo_data_t, list);
        ec_fsm_coe_download(&fsm->fsm_coe, fsm->slave, fsm->sdodata);
        ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
        return;
    }

    // All SDOs are now configured.

    // set state to SAVEOP
    fsm->slave_state = ec_fsm_slaveconf_saveop;
    ec_fsm_change(&fsm->fsm_change, fsm->slave, EC_SLAVE_STATE_SAVEOP);
    ec_fsm_change_exec(&fsm->fsm_change); // execute immediately
}

/*****************************************************************************/

/**
   Slave configuration state: SAVEOP.
*/

void ec_fsm_slaveconf_saveop(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_change_exec(&fsm->fsm_change)) return;

    if (!ec_fsm_change_success(&fsm->fsm_change)) {
        fsm->slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        return;
    }

    // slave is now in SAVEOP

    if (master->debug_level) {
        EC_DBG("Slave %i is now in SAVEOP.\n", slave->ring_position);
    }

    if (fsm->slave->current_state == fsm->slave->requested_state) {
        fsm->slave_state = ec_fsm_end; // successful
        if (master->debug_level) {
            EC_DBG("Finished configuration of slave %i.\n",
                   slave->ring_position);
        }
        return;
    }

    // set state to OP
    fsm->slave_state = ec_fsm_slaveconf_op;
    ec_fsm_change(&fsm->fsm_change, slave, EC_SLAVE_STATE_OP);
    ec_fsm_change_exec(&fsm->fsm_change); // execute immediately
}

/*****************************************************************************/

/**
   Slave configuration state: OP
*/

void ec_fsm_slaveconf_op(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_change_exec(&fsm->fsm_change)) return;

    if (!ec_fsm_change_success(&fsm->fsm_change)) {
        slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        return;
    }

    // slave is now in OP

    if (master->debug_level) {
        EC_DBG("Slave %i is now in OP.\n", slave->ring_position);
        EC_DBG("Finished configuration of slave %i.\n", slave->ring_position);
    }

    fsm->slave_state = ec_fsm_end; // successful
}

/******************************************************************************
 *  Common state functions
 *****************************************************************************/

/**
   State: ERROR.
*/

void ec_fsm_error(ec_fsm_t *fsm /**< finite state machine */)
{
}

/*****************************************************************************/

/**
   State: END.
*/

void ec_fsm_end(ec_fsm_t *fsm /**< finite state machine */)
{
}

/*****************************************************************************/
