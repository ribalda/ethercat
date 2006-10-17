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

void ec_fsm_sii_start_reading(ec_fsm_t *);
void ec_fsm_sii_read_check(ec_fsm_t *);
void ec_fsm_sii_read_fetch(ec_fsm_t *);
void ec_fsm_sii_start_writing(ec_fsm_t *);
void ec_fsm_sii_write_check(ec_fsm_t *);
void ec_fsm_sii_write_check2(ec_fsm_t *);

void ec_fsm_change_start(ec_fsm_t *);
void ec_fsm_change_check(ec_fsm_t *);
void ec_fsm_change_status(ec_fsm_t *);
void ec_fsm_change_code(ec_fsm_t *);
void ec_fsm_change_ack(ec_fsm_t *);
void ec_fsm_change_check_ack(ec_fsm_t *);

void ec_fsm_coe_down_start(ec_fsm_t *);
void ec_fsm_coe_down_request(ec_fsm_t *);
void ec_fsm_coe_down_check(ec_fsm_t *);
void ec_fsm_coe_down_response(ec_fsm_t *);

void ec_fsm_end(ec_fsm_t *);
void ec_fsm_error(ec_fsm_t *);

void ec_canopen_abort_msg(uint32_t);

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

    return 0;
}

/*****************************************************************************/

/**
   Destructor.
*/

void ec_fsm_clear(ec_fsm_t *fsm /**< finite state machine */)
{
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
*/

void ec_fsm_execute(ec_fsm_t *fsm /**< finite state machine */)
{
    fsm->master_state(fsm);
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
   Returns the running state of the master startup state machine.
   \return non-zero if not terminated yet.
*/

int ec_fsm_startup_running(ec_fsm_t *fsm /**< Finite state machine */)
{
    return fsm->master_state != ec_fsm_end &&
        fsm->master_state != ec_fsm_error;
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
   Returns the running state of the master configuration state machine.
   \return non-zero if not terminated yet.
*/

int ec_fsm_configuration_running(ec_fsm_t *fsm /**< Finite state machine */)
{
    return fsm->master_state != ec_fsm_end &&
        fsm->master_state != ec_fsm_error;
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
    fsm->change_new = EC_SLAVE_STATE_INIT;
    fsm->change_state = ec_fsm_change_start;
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
        fsm->change_new = EC_SLAVE_STATE_INIT;
        fsm->change_state = ec_fsm_change_start;
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

    // check if any slaves are not in the state, they're supposed to be
    list_for_each_entry(slave, &master->slaves, list) {
        if (slave->error_flag ||
            !slave->online ||
            slave->requested_state == EC_SLAVE_STATE_UNKNOWN ||
            slave->current_state == slave->requested_state) continue;

        ec_state_string(slave->current_state, old_state);
        ec_state_string(slave->requested_state, new_state);
        EC_INFO("Changing state of slave %i from %s to %s.\n",
                slave->ring_position, old_state, new_state);

        fsm->slave = slave;
        fsm->slave_state = ec_fsm_slaveconf_init;
        fsm->change_new = EC_SLAVE_STATE_INIT;
        fsm->change_state = ec_fsm_change_start;
        fsm->master_state = ec_fsm_master_configure_slave;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    // Check, if EoE processing has to be started
    ec_master_eoe_start(master);

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
            memcpy(fsm->sii_value, slave->new_eeprom_data, 2);
            fsm->sii_mode = 1;
            fsm->sii_state = ec_fsm_sii_start_writing;
            fsm->slave = slave;
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
            fsm->sii_offset = 0x0008; // vendor ID
            fsm->sii_mode = 0;
            fsm->sii_state = ec_fsm_sii_start_reading;
            fsm->sii_state(fsm); // execute immediately
            return;
        }
    }

    ec_fsm_master_action_process_states(fsm);
}

/*****************************************************************************/

/**
   Master state: STATES.
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

    fsm->sii_state(fsm); // execute SII state machine

    if (fsm->sii_state == ec_fsm_error) {
        fsm->slave->error_flag = 1;
        EC_ERR("Failed to validate vendor ID of slave %i.\n",
               slave->ring_position);
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    if (fsm->sii_state != ec_fsm_end) return;

    if (EC_READ_U32(fsm->sii_value) != slave->sii_vendor_id) {
        EC_ERR("Slave %i: invalid vendor ID!\n", slave->ring_position);
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    // vendor ID is ok. check product code.
    fsm->master_state = ec_fsm_master_validate_product;
    fsm->sii_offset = 0x000A; // product code
    fsm->sii_mode = 0;
    fsm->sii_state = ec_fsm_sii_start_reading;
    fsm->sii_state(fsm); // execute immediately
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

    fsm->sii_state(fsm); // execute SII state machine

    if (fsm->sii_state == ec_fsm_error) {
        fsm->slave->error_flag = 1;
        EC_ERR("Failed to validate product code of slave %i.\n",
               slave->ring_position);
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    if (fsm->sii_state != ec_fsm_end) return;

    if (EC_READ_U32(fsm->sii_value) != slave->sii_product_code) {
        EC_ERR("Slave %i: invalid product code!\n", slave->ring_position);
        EC_ERR("expected 0x%08X, got 0x%08X.\n", slave->sii_product_code,
               EC_READ_U32(fsm->sii_value));
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
    fsm->sii_offset = 0x0008; // vendor ID
    fsm->sii_mode = 0;
    fsm->sii_state = ec_fsm_sii_start_reading;
    fsm->sii_state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   Master state: ADDRESS.
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
   Master state: SCAN.
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

    // determine initial states.
    list_for_each_entry(slave, &master->slaves, list) {
        if (ec_slave_is_coupler(slave)) {
            slave->requested_state = EC_SLAVE_STATE_OP;
        }
        else {
            if (master->mode == EC_MASTER_MODE_OPERATION)
                slave->requested_state = EC_SLAVE_STATE_PREOP;
            else
                slave->requested_state = EC_SLAVE_STATE_INIT;
        }
    }

    fsm->master_state = ec_fsm_master_start;
    fsm->master_state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   Master state: CONF.
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
   Master state: EEPROM.
*/

void ec_fsm_master_write_eeprom(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;

    fsm->sii_state(fsm); // execute SII state machine

    if (fsm->sii_state == ec_fsm_error) {
        fsm->slave->error_flag = 1;
        EC_ERR("Failed to write EEPROM contents to slave %i.\n",
               slave->ring_position);
        kfree(slave->new_eeprom_data);
        slave->new_eeprom_data = NULL;
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    if (fsm->sii_state != ec_fsm_end) return;

    fsm->sii_offset++;
    if (fsm->sii_offset < slave->new_eeprom_size) {
        memcpy(fsm->sii_value, slave->new_eeprom_data + fsm->sii_offset, 2);
        fsm->sii_state = ec_fsm_sii_start_writing;
        fsm->sii_state(fsm); // execute immediately
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
    return;
}

/******************************************************************************
 *  slave scan state machine
 *****************************************************************************/

/**
   Slave state: START_READING.
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
   Slave state: ADDRESS.
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
   Slave state: STATE.
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
        slave->current_state &= 0x0F;
    }

    // read base data
    ec_datagram_nprd(datagram, fsm->slave->station_address, 0x0000, 6);
    ec_master_queue_datagram(fsm->master, datagram);
    fsm->slave_state = ec_fsm_slavescan_base;
}

/*****************************************************************************/

/**
   Slave state: BASE.
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
   Slave state: DATALINK.
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
    fsm->sii_mode = 1;
    fsm->sii_state = ec_fsm_sii_start_reading;
    fsm->slave_state = ec_fsm_slavescan_eeprom_size;
    fsm->slave_state(fsm); // execute state immediately
}

/*****************************************************************************/

/**
   Slave state: EEPROM_SIZE.
*/

void ec_fsm_slavescan_eeprom_size(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;
    uint16_t cat_type, cat_size;

    // execute SII state machine
    fsm->sii_state(fsm);

    if (fsm->sii_state == ec_fsm_error) {
        fsm->slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        EC_ERR("Failed to read EEPROM size of slave %i.\n",
               slave->ring_position);
        return;
    }

    if (fsm->sii_state != ec_fsm_end) return;

    cat_type = EC_READ_U16(fsm->sii_value);
    cat_size = EC_READ_U16(fsm->sii_value + 2);

    if (cat_type != 0xFFFF) { // not the last category
        fsm->sii_offset += cat_size + 2;
        fsm->sii_state = ec_fsm_sii_start_reading;
        fsm->sii_state(fsm); // execute state immediately
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

    fsm->sii_offset = 0x0000;
    fsm->sii_mode = 1;
    fsm->sii_state = ec_fsm_sii_start_reading;
    fsm->slave_state = ec_fsm_slavescan_eeprom_data;
    fsm->slave_state(fsm); // execute state immediately
}

/*****************************************************************************/

/**
   Slave state: EEPROM_DATA.
*/

void ec_fsm_slavescan_eeprom_data(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;
    uint16_t *cat_word, cat_type, cat_size;

    // execute SII state machine
    fsm->sii_state(fsm);

    if (fsm->sii_state == ec_fsm_error) {
        fsm->slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        EC_ERR("Failed to fetch EEPROM contents of slave %i.\n",
               slave->ring_position);
        return;
    }

    if (fsm->sii_state != ec_fsm_end) return;

    // 2 words fetched

    if (fsm->sii_offset + 2 <= slave->eeprom_size / 2) { // 2 words fit
        memcpy(slave->eeprom_data + fsm->sii_offset * 2, fsm->sii_value, 4);
    }
    else { // copy the last word
        memcpy(slave->eeprom_data + fsm->sii_offset * 2, fsm->sii_value, 2);
    }

    if (fsm->sii_offset + 2 < slave->eeprom_size / 2) {
        // fetch the next 2 words
        fsm->sii_offset += 2;
        fsm->sii_state = ec_fsm_sii_start_reading;
        fsm->sii_state(fsm); // execute state immediately
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
   Slave state: INIT.
*/

void ec_fsm_slaveconf_init(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = &fsm->datagram;
    const ec_sii_sync_t *sync;

    fsm->change_state(fsm); // execute state change state machine

    if (fsm->change_state == ec_fsm_error) {
        slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        return;
    }

    if (fsm->change_state != ec_fsm_end) return;

    // slave is now in INIT
    if (slave->current_state == slave->requested_state) {
        fsm->slave_state = ec_fsm_end; // successful
        return;
    }

    // check and reset CRC fault counters
    //ec_slave_check_crc(slave);
    // TODO: Implement state machine for CRC checking.

    if (!slave->base_sync_count) { // no sync managers
        fsm->slave_state = ec_fsm_slaveconf_preop;
        fsm->change_new = EC_SLAVE_STATE_PREOP;
        fsm->change_state = ec_fsm_change_start;
        fsm->change_state(fsm); // execute immediately
        return;
    }

    // configure sync managers
    ec_datagram_npwr(datagram, slave->station_address, 0x0800,
                     EC_SYNC_SIZE * slave->base_sync_count);
    memset(datagram->data, 0x00, EC_SYNC_SIZE * slave->base_sync_count);

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

    ec_master_queue_datagram(fsm->master, datagram);
    fsm->slave_state = ec_fsm_slaveconf_sync;
}

/*****************************************************************************/

/**
   Slave state: SYNC.
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
    fsm->change_new = EC_SLAVE_STATE_PREOP;
    fsm->change_state = ec_fsm_change_start;
    fsm->change_state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   Slave state: PREOP.
*/

void ec_fsm_slaveconf_preop(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = fsm->master;
    ec_datagram_t *datagram = &fsm->datagram;
    unsigned int j;

    fsm->change_state(fsm); // execute state change state machine

    if (fsm->change_state == ec_fsm_error) {
        slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        return;
    }

    if (fsm->change_state != ec_fsm_end) return;

    // slave is now in PREOP
    if (slave->current_state == slave->requested_state) {
        fsm->slave_state = ec_fsm_end; // successful
        return;
    }

    if (!slave->base_fmmu_count) { // skip FMMU configuration
        if (list_empty(&slave->sdo_confs)) { // skip SDO configuration
            fsm->slave_state = ec_fsm_slaveconf_saveop;
            fsm->change_new = EC_SLAVE_STATE_SAVEOP;
            fsm->change_state = ec_fsm_change_start;
            fsm->change_state(fsm); // execute immediately
            return;
        }
        fsm->slave_state = ec_fsm_slaveconf_sdoconf;
        fsm->coe_sdodata = list_entry(slave->sdo_confs.next, ec_sdo_data_t, list);
        fsm->coe_state = ec_fsm_coe_down_start;
        fsm->coe_state(fsm); // execute immediately
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
   Slave state: FMMU.
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
        fsm->change_new = EC_SLAVE_STATE_SAVEOP;
        fsm->change_state = ec_fsm_change_start;
        fsm->change_state(fsm); // execute immediately
        return;
    }

    fsm->slave_state = ec_fsm_slaveconf_sdoconf;
    fsm->coe_sdodata = list_entry(slave->sdo_confs.next, ec_sdo_data_t, list);
    fsm->coe_state = ec_fsm_coe_down_start;
    fsm->coe_state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   Slave state: SDOCONF.
*/

void ec_fsm_slaveconf_sdoconf(ec_fsm_t *fsm /**< finite state machine */)
{
    fsm->coe_state(fsm); // execute CoE state machine

    if (fsm->coe_state == ec_fsm_error) {
        fsm->slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        return;
    }

    if (fsm->coe_state != ec_fsm_end) return;

    // Another SDO to configure?
    if (fsm->coe_sdodata->list.next != &fsm->slave->sdo_confs) {
        fsm->coe_sdodata = list_entry(fsm->coe_sdodata->list.next,
                                      ec_sdo_data_t, list);
        fsm->coe_state = ec_fsm_coe_down_start;
        fsm->coe_state(fsm); // execute immediately
        return;
    }

    // All SDOs are now configured.

    // set state to SAVEOP
    fsm->slave_state = ec_fsm_slaveconf_saveop;
    fsm->change_new = EC_SLAVE_STATE_SAVEOP;
    fsm->change_state = ec_fsm_change_start;
    fsm->change_state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   Slave state: SAVEOP.
*/

void ec_fsm_slaveconf_saveop(ec_fsm_t *fsm /**< finite state machine */)
{
    fsm->change_state(fsm); // execute state change state machine

    if (fsm->change_state == ec_fsm_error) {
        fsm->slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        return;
    }

    if (fsm->change_state != ec_fsm_end) return;

    // slave is now in SAVEOP
    if (fsm->slave->current_state == fsm->slave->requested_state) {
        fsm->slave_state = ec_fsm_end; // successful
        return;
    }

    // set state to OP
    fsm->slave_state = ec_fsm_slaveconf_op;
    fsm->change_new = EC_SLAVE_STATE_OP;
    fsm->change_state = ec_fsm_change_start;
    fsm->change_state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   Slave state: OP
*/

void ec_fsm_slaveconf_op(ec_fsm_t *fsm /**< finite state machine */)
{
    fsm->change_state(fsm); // execute state change state machine

    if (fsm->change_state == ec_fsm_error) {
        fsm->slave->error_flag = 1;
        fsm->slave_state = ec_fsm_error;
        return;
    }

    if (fsm->change_state != ec_fsm_end) return;

    // slave is now in OP
    fsm->slave_state = ec_fsm_end; // successful
}

/******************************************************************************
 *  SII state machine
 *****************************************************************************/

/**
   SII state: START_READING.
   Starts reading the slave information interface.
*/

void ec_fsm_sii_start_reading(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;

    // initiate read operation
    if (fsm->sii_mode) {
        ec_datagram_npwr(datagram, fsm->slave->station_address, 0x502, 4);
    }
    else {
        ec_datagram_apwr(datagram, fsm->slave->ring_position, 0x502, 4);
    }

    EC_WRITE_U8 (datagram->data,     0x00); // read-only access
    EC_WRITE_U8 (datagram->data + 1, 0x01); // request read operation
    EC_WRITE_U16(datagram->data + 2, fsm->sii_offset);
    ec_master_queue_datagram(fsm->master, datagram);
    fsm->sii_state = ec_fsm_sii_read_check;
}

/*****************************************************************************/

/**
   SII state: READ_CHECK.
   Checks, if the SII-read-datagram has been sent and issues a fetch datagram.
*/

void ec_fsm_sii_read_check(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;

    if (datagram->state != EC_DATAGRAM_RECEIVED
        || datagram->working_counter != 1) {
        EC_ERR("SII: Reception of read datagram failed.\n");
        fsm->sii_state = ec_fsm_error;
        return;
    }

    fsm->sii_start = datagram->cycles_sent;
    fsm->sii_check_once_more = 1;

    // issue check/fetch datagram
    if (fsm->sii_mode) {
        ec_datagram_nprd(datagram, fsm->slave->station_address, 0x502, 10);
    }
    else {
        ec_datagram_aprd(datagram, fsm->slave->ring_position, 0x502, 10);
    }

    ec_master_queue_datagram(fsm->master, datagram);
    fsm->sii_state = ec_fsm_sii_read_fetch;
}

/*****************************************************************************/

/**
   SII state: READ_FETCH.
   Fetches the result of an SII-read datagram.
*/

void ec_fsm_sii_read_fetch(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;

    if (datagram->state != EC_DATAGRAM_RECEIVED
        || datagram->working_counter != 1) {
        EC_ERR("SII: Reception of check/fetch datagram failed.\n");
        fsm->sii_state = ec_fsm_error;
        return;
    }

    // check "busy bit"
    if (EC_READ_U8(datagram->data + 1) & 0x81) {
        // still busy... timeout?
        if (datagram->cycles_received
            - fsm->sii_start >= (cycles_t) 10 * cpu_khz) {
            if (!fsm->sii_check_once_more) {
                EC_ERR("SII: Read timeout.\n");
                fsm->sii_state = ec_fsm_error;
#if 0
                EC_DBG("SII busy: %02X %02X %02X %02X\n",
                       EC_READ_U8(datagram->data + 0),
                       EC_READ_U8(datagram->data + 1),
                       EC_READ_U8(datagram->data + 2),
                       EC_READ_U8(datagram->data + 3));
#endif
                return;
            }
            fsm->sii_check_once_more = 0;
        }

        // issue check/fetch datagram again
        if (fsm->sii_mode) {
            ec_datagram_nprd(datagram, fsm->slave->station_address, 0x502, 10);
        }
        else {
            ec_datagram_aprd(datagram, fsm->slave->ring_position, 0x502, 10);
        }
        ec_master_queue_datagram(fsm->master, datagram);
        return;
    }

#if 0
    EC_DBG("SII rec: %02X %02X %02X %02X - %02X %02X %02X %02X\n",
           EC_READ_U8(datagram->data + 0), EC_READ_U8(datagram->data + 1),
           EC_READ_U8(datagram->data + 2), EC_READ_U8(datagram->data + 3),
           EC_READ_U8(datagram->data + 6), EC_READ_U8(datagram->data + 7),
           EC_READ_U8(datagram->data + 8), EC_READ_U8(datagram->data + 9));
#endif

    // SII value received.
    memcpy(fsm->sii_value, datagram->data + 6, 4);
    fsm->sii_state = ec_fsm_end;
}

/*****************************************************************************/

/**
   SII state: START_WRITING.
   Starts reading the slave information interface.
*/

void ec_fsm_sii_start_writing(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;

    // initiate write operation
    ec_datagram_npwr(datagram, fsm->slave->station_address, 0x502, 8);
    EC_WRITE_U8 (datagram->data,     0x01); // enable write access
    EC_WRITE_U8 (datagram->data + 1, 0x02); // request write operation
    EC_WRITE_U32(datagram->data + 2, fsm->sii_offset);
    memcpy(datagram->data + 6, fsm->sii_value, 2);
    ec_master_queue_datagram(fsm->master, datagram);
    fsm->sii_state = ec_fsm_sii_write_check;
}

/*****************************************************************************/

/**
   SII state: WRITE_CHECK.
*/

void ec_fsm_sii_write_check(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;

    if (datagram->state != EC_DATAGRAM_RECEIVED
        || datagram->working_counter != 1) {
        EC_ERR("SII: Reception of write datagram failed.\n");
        fsm->sii_state = ec_fsm_error;
        return;
    }

    fsm->sii_start = datagram->cycles_sent;
    fsm->sii_check_once_more = 1;

    // issue check/fetch datagram
    ec_datagram_nprd(datagram, fsm->slave->station_address, 0x502, 2);
    ec_master_queue_datagram(fsm->master, datagram);
    fsm->sii_state = ec_fsm_sii_write_check2;
}

/*****************************************************************************/

/**
   SII state: WRITE_CHECK2.
*/

void ec_fsm_sii_write_check2(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;

    if (datagram->state != EC_DATAGRAM_RECEIVED
        || datagram->working_counter != 1) {
        EC_ERR("SII: Reception of write check datagram failed.\n");
        fsm->sii_state = ec_fsm_error;
        return;
    }

    if (EC_READ_U8(datagram->data + 1) & 0x82) {
        // still busy... timeout?
        if (datagram->cycles_received
            - fsm->sii_start >= (cycles_t) 10 * cpu_khz) {
            if (!fsm->sii_check_once_more) {
                EC_ERR("SII: Write timeout.\n");
                fsm->sii_state = ec_fsm_error;
                return;
            }
            fsm->sii_check_once_more = 0;
        }

        // issue check/fetch datagram again
        ec_master_queue_datagram(fsm->master, datagram);
        return;
    }

    if (EC_READ_U8(datagram->data + 1) & 0x40) {
        EC_ERR("SII: Write operation failed!\n");
        fsm->sii_state = ec_fsm_error;
        return;
    }

    // success
    fsm->sii_state = ec_fsm_end;
}

/******************************************************************************
 *  state change state machine
 *****************************************************************************/

/**
   Change state: START.
*/

void ec_fsm_change_start(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    fsm->change_take_time = 1;

    // write new state to slave
    ec_datagram_npwr(datagram, slave->station_address, 0x0120, 2);
    EC_WRITE_U16(datagram->data, fsm->change_new);
    ec_master_queue_datagram(fsm->master, datagram);
    fsm->change_state = ec_fsm_change_check;
}

/*****************************************************************************/

/**
   Change state: CHECK.
*/

void ec_fsm_change_check(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->change_state = ec_fsm_error;
        EC_ERR("Failed to send state datagram to slave %i!\n",
               fsm->slave->ring_position);
        return;
    }

    if (fsm->change_take_time) {
        fsm->change_take_time = 0;
        fsm->change_jiffies = datagram->jiffies_sent;
    }

    if (datagram->working_counter != 1) {
        if (datagram->jiffies_received - fsm->change_jiffies >= 3 * HZ) {
            fsm->change_state = ec_fsm_error;
            EC_ERR("Failed to set state 0x%02X on slave %i: Slave did not"
                   " respond.\n", fsm->change_new, fsm->slave->ring_position);
            return;
        }

        // repeat writing new state to slave
        ec_datagram_npwr(datagram, slave->station_address, 0x0120, 2);
        EC_WRITE_U16(datagram->data, fsm->change_new);
        ec_master_queue_datagram(fsm->master, datagram);
        return;
    }

    fsm->change_take_time = 1;

    // read AL status from slave
    ec_datagram_nprd(datagram, slave->station_address, 0x0130, 2);
    ec_master_queue_datagram(fsm->master, datagram);
    fsm->change_state = ec_fsm_change_status;
}

/*****************************************************************************/

/**
   Change state: STATUS.
*/

void ec_fsm_change_status(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state != EC_DATAGRAM_RECEIVED
        || datagram->working_counter != 1) {
        fsm->change_state = ec_fsm_error;
        EC_ERR("Failed to check state 0x%02X on slave %i.\n",
               fsm->change_new, slave->ring_position);
        return;
    }

    if (fsm->change_take_time) {
        fsm->change_take_time = 0;
        fsm->change_jiffies = datagram->jiffies_sent;
    }

    slave->current_state = EC_READ_U8(datagram->data);

    if (slave->current_state == fsm->change_new) {
        // state has been set successfully
        fsm->change_state = ec_fsm_end;
        return;
    }

    if (slave->current_state & EC_SLAVE_STATE_ACK_ERR) {
        // state change error
        fsm->change_new = slave->current_state & 0x0F;
        EC_ERR("Failed to set state 0x%02X - Slave %i refused state change"
               " (code 0x%02X)!\n", fsm->change_new, slave->ring_position,
               slave->current_state);
        // fetch AL status error code
        ec_datagram_nprd(datagram, slave->station_address, 0x0134, 2);
        ec_master_queue_datagram(fsm->master, datagram);
        fsm->change_state = ec_fsm_change_code;
        return;
    }

    if (datagram->jiffies_received
        - fsm->change_jiffies >= 100 * HZ / 1000) { // 100ms
        // timeout while checking
        fsm->change_state = ec_fsm_error;
        EC_ERR("Timeout while setting state 0x%02X on slave %i.\n",
               fsm->change_new, slave->ring_position);
        return;
    }

    // still old state: check again
    ec_datagram_nprd(datagram, slave->station_address, 0x0130, 2);
    ec_master_queue_datagram(fsm->master, datagram);
}

/*****************************************************************************/

/**
   Application layer status messages.
*/

const ec_code_msg_t al_status_messages[] = {
    {0x0001, "Unspecified error"},
    {0x0011, "Invalud requested state change"},
    {0x0012, "Unknown requested state"},
    {0x0013, "Bootstrap not supported"},
    {0x0014, "No valid firmware"},
    {0x0015, "Invalid mailbox configuration"},
    {0x0016, "Invalid mailbox configuration"},
    {0x0017, "Invalid sync manager configuration"},
    {0x0018, "No valid inputs available"},
    {0x0019, "No valid outputs"},
    {0x001A, "Synchronisation error"},
    {0x001B, "Sync manager watchdog"},
    {0x001C, "Invalid sync manager types"},
    {0x001D, "Invalid output configuration"},
    {0x001E, "Invalid input configuration"},
    {0x001F, "Invalid watchdog configuration"},
    {0x0020, "Slave needs cold start"},
    {0x0021, "Slave needs INIT"},
    {0x0022, "Slave needs PREOP"},
    {0x0023, "Slave needs SAVEOP"},
    {0x0030, "Invalid DC SYNCH configuration"},
    {0x0031, "Invalid DC latch configuration"},
    {0x0032, "PLL error"},
    {0x0033, "Invalid DC IO error"},
    {0x0034, "Invalid DC timeout error"},
    {0x0042, "MBOX EOE"},
    {0x0043, "MBOX COE"},
    {0x0044, "MBOX FOE"},
    {0x0045, "MBOX SOE"},
    {0x004F, "MBOX VOE"},
    {}
};

/*****************************************************************************/

/**
   Change state: CODE.
*/

void ec_fsm_change_code(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    uint32_t code;
    const ec_code_msg_t *al_msg;

    if (datagram->state != EC_DATAGRAM_RECEIVED
        || datagram->working_counter != 1) {
        fsm->change_state = ec_fsm_error;
        EC_ERR("Reception of AL status code datagram failed.\n");
        return;
    }

    if ((code = EC_READ_U16(datagram->data))) {
        for (al_msg = al_status_messages; al_msg->code; al_msg++) {
            if (al_msg->code != code) continue;
            EC_ERR("AL status message 0x%04X: \"%s\".\n",
                   al_msg->code, al_msg->message);
            break;
        }
        if (!al_msg->code)
            EC_ERR("Unknown AL status code 0x%04X.\n", code);
    }

    // acknowledge "old" slave state
    ec_datagram_npwr(datagram, slave->station_address, 0x0120, 2);
    EC_WRITE_U16(datagram->data, slave->current_state);
    ec_master_queue_datagram(fsm->master, datagram);
    fsm->change_state = ec_fsm_change_ack;
}

/*****************************************************************************/

/**
   Change state: ACK.
*/

void ec_fsm_change_ack(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state != EC_DATAGRAM_RECEIVED
        || datagram->working_counter != 1) {
        fsm->change_state = ec_fsm_error;
        EC_ERR("Reception of state ack datagram failed.\n");
        return;
    }

    fsm->change_take_time = 1;

    // read new AL status
    ec_datagram_nprd(datagram, slave->station_address, 0x0130, 2);
    ec_master_queue_datagram(fsm->master, datagram);
    fsm->change_state = ec_fsm_change_check_ack;
}

/*****************************************************************************/

/**
   Change state: CHECK ACK.
*/

void ec_fsm_change_check_ack(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    ec_slave_state_t ack_state;

    if (datagram->state != EC_DATAGRAM_RECEIVED
        || datagram->working_counter != 1) {
        fsm->change_state = ec_fsm_error;
        EC_ERR("Reception of state ack check datagram failed.\n");
        return;
    }

    if (fsm->change_take_time) {
        fsm->change_take_time = 0;
        fsm->change_jiffies = datagram->jiffies_sent;
    }

    ack_state = EC_READ_U8(datagram->data);

    if (ack_state == slave->current_state) {
        fsm->change_state = ec_fsm_error;
        EC_INFO("Acknowleged state 0x%02X on slave %i.\n",
                slave->current_state, slave->ring_position);
        return;
    }

    if (datagram->jiffies_received
        - fsm->change_jiffies >= 100 * HZ / 1000) { // 100ms
        // timeout while checking
        slave->current_state = EC_SLAVE_STATE_UNKNOWN;
        fsm->change_state = ec_fsm_error;
        EC_ERR("Timeout while acknowleging state 0x%02X on slave %i.\n",
               fsm->change_new, slave->ring_position);
        return;
    }

    // reread new AL status
    ec_datagram_nprd(datagram, slave->station_address, 0x0130, 2);
    ec_master_queue_datagram(fsm->master, datagram);
}

/******************************************************************************
 *  CoE state machine
 *****************************************************************************/

/**
   CoE state: DOWN_START.
*/

void ec_fsm_coe_down_start(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    ec_sdo_data_t *sdodata = fsm->coe_sdodata;
    uint8_t *data;

    EC_INFO("Downloading SDO 0x%04X:%i to slave %i.\n",
            sdodata->index, sdodata->subindex, slave->ring_position);

    if (slave->sii_rx_mailbox_size < 6 + 10 + sdodata->size) {
        EC_ERR("SDO fragmenting not supported yet!\n");
        fsm->coe_state = ec_fsm_error;
        return;
    }

    if (!(data = ec_slave_mbox_prepare_send(slave, datagram, 0x03,
                                            sdodata->size + 10))) {
        fsm->coe_state = ec_fsm_error;
        return;
    }

    EC_WRITE_U16(data, 0x2 << 12); // SDO request
    EC_WRITE_U8 (data + 2, (0x1 // size specified
                            | 0x1 << 5)); // Download request
    EC_WRITE_U16(data + 3, sdodata->index);
    EC_WRITE_U8 (data + 5, sdodata->subindex);
    EC_WRITE_U32(data + 6, sdodata->size);
    memcpy(data + 10, sdodata->data, sdodata->size);

    ec_master_queue_datagram(fsm->master, datagram);
    fsm->coe_state = ec_fsm_coe_down_request;
}

/*****************************************************************************/

/**
   CoE state: DOWN_REQUEST.
*/

void ec_fsm_coe_down_request(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state != EC_DATAGRAM_RECEIVED
        || datagram->working_counter != 1) {
        fsm->coe_state = ec_fsm_error;
        EC_ERR("Reception of CoE download request failed.\n");
        return;
    }

    fsm->coe_start = datagram->cycles_sent;

    ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
    ec_master_queue_datagram(fsm->master, datagram);
    fsm->coe_state = ec_fsm_coe_down_check;
}

/*****************************************************************************/

/**
   CoE state: DOWN_CHECK.
*/

void ec_fsm_coe_down_check(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state != EC_DATAGRAM_RECEIVED
        || datagram->working_counter != 1) {
        fsm->coe_state = ec_fsm_error;
        EC_ERR("Reception of CoE mailbox check datagram failed.\n");
        return;
    }

    if (!ec_slave_mbox_check(datagram)) {
        if (datagram->cycles_received
            - fsm->coe_start >= (cycles_t) 100 * cpu_khz) {
            fsm->coe_state = ec_fsm_error;
            EC_ERR("Timeout while checking SDO configuration on slave %i.\n",
                   slave->ring_position);
            return;
        }

        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        ec_master_queue_datagram(fsm->master, datagram);
        return;
    }

    // Fetch response
    ec_slave_mbox_prepare_fetch(slave, datagram); // can not fail.
    ec_master_queue_datagram(fsm->master, datagram);
    fsm->coe_state = ec_fsm_coe_down_response;
}

/*****************************************************************************/

/**
   CoE state: DOWN_RESPONSE.
*/

void ec_fsm_coe_down_response(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_datagram_t *datagram = &fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    uint8_t *data, mbox_prot;
    size_t rec_size;
    ec_sdo_data_t *sdodata = fsm->coe_sdodata;

    if (datagram->state != EC_DATAGRAM_RECEIVED
        || datagram->working_counter != 1) {
        fsm->coe_state = ec_fsm_error;
        EC_ERR("Reception of CoE download response failed.\n");
        return;
    }

    if (!(data = ec_slave_mbox_fetch(slave, datagram,
				     &mbox_prot, &rec_size))) {
        fsm->coe_state = ec_fsm_error;
        return;
    }

    if (mbox_prot != 0x03) { // CoE
        EC_WARN("Received mailbox protocol 0x%02X as response.\n", mbox_prot);
        fsm->coe_state = ec_fsm_error;
	return;
    }

    if (rec_size < 6) {
        fsm->coe_state = ec_fsm_error;
        EC_ERR("Received data is too small (%i bytes):\n", rec_size);
        ec_print_data(data, rec_size);
        return;
    }

    if (EC_READ_U16(data) >> 12 == 0x2 && // SDO request
        EC_READ_U8 (data + 2) >> 5 == 0x4) { // abort SDO transfer request
        fsm->coe_state = ec_fsm_error;
        EC_ERR("SDO download 0x%04X:%X (%i bytes) aborted on slave %i.\n",
               sdodata->index, sdodata->subindex, sdodata->size,
               slave->ring_position);
        if (rec_size < 10) {
            EC_ERR("Incomplete Abort command:\n");
            ec_print_data(data, rec_size);
        }
        else
            ec_canopen_abort_msg(EC_READ_U32(data + 6));
        return;
    }

    if (EC_READ_U16(data) >> 12 != 0x3 || // SDO response
        EC_READ_U8 (data + 2) >> 5 != 0x3 || // Download response
        EC_READ_U16(data + 3) != sdodata->index || // index
        EC_READ_U8 (data + 5) != sdodata->subindex) { // subindex
        fsm->coe_state = ec_fsm_error;
        EC_ERR("SDO download 0x%04X:%X (%i bytes) failed:\n",
               sdodata->index, sdodata->subindex, sdodata->size);
        EC_ERR("Invalid SDO download response at slave %i!\n",
               slave->ring_position);
        ec_print_data(data, rec_size);
        return;
    }

    fsm->coe_state = ec_fsm_end; // success
}

/*****************************************************************************/

/**
   SDO abort messages.
   The "abort SDO transfer request" supplies an abort code,
   which can be translated to clear text. This table does
   the mapping of the codes and messages.
*/

const ec_code_msg_t sdo_abort_messages[] = {
    {0x05030000, "Toggle bit not changed"},
    {0x05040000, "SDO protocol timeout"},
    {0x05040001, "Client/Server command specifier not valid or unknown"},
    {0x05040005, "Out of memory"},
    {0x06010000, "Unsupported access to an object"},
    {0x06010001, "Attempt to read a write-only object"},
    {0x06010002, "Attempt to write a read-only object"},
    {0x06020000, "This object does not exist in the object directory"},
    {0x06040041, "The object cannot be mapped into the PDO"},
    {0x06040042, "The number and length of the objects to be mapped would"
     " exceed the PDO length"},
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
   Outputs an SDO abort message.
*/

void ec_canopen_abort_msg(uint32_t abort_code)
{
    const ec_code_msg_t *abort_msg;

    for (abort_msg = sdo_abort_messages; abort_msg->code; abort_msg++) {
        if (abort_msg->code == abort_code) {
            EC_ERR("SDO abort message 0x%08X: \"%s\".\n",
                   abort_msg->code, abort_msg->message);
            return;
        }
    }

    EC_ERR("Unknown SDO abort code 0x%08X.\n", abort_code);
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
