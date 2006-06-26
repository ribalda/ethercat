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

/*****************************************************************************/

void ec_fsm_master_start(ec_fsm_t *);
void ec_fsm_master_broadcast(ec_fsm_t *);
void ec_fsm_master_proc_states(ec_fsm_t *);
void ec_fsm_master_scan(ec_fsm_t *);
void ec_fsm_master_states(ec_fsm_t *);
void ec_fsm_master_validate_vendor(ec_fsm_t *);
void ec_fsm_master_validate_product(ec_fsm_t *);
void ec_fsm_master_reconfigure(ec_fsm_t *);
void ec_fsm_master_address(ec_fsm_t *);
void ec_fsm_master_conf(ec_fsm_t *);
void ec_fsm_master_eeprom(ec_fsm_t *);

void ec_fsm_slave_start_reading(ec_fsm_t *);
void ec_fsm_slave_read_status(ec_fsm_t *);
void ec_fsm_slave_read_base(ec_fsm_t *);
void ec_fsm_slave_read_dl(ec_fsm_t *);
void ec_fsm_slave_eeprom_size(ec_fsm_t *);
void ec_fsm_slave_fetch_eeprom(ec_fsm_t *);
void ec_fsm_slave_fetch_eeprom2(ec_fsm_t *);
void ec_fsm_slave_end(ec_fsm_t *);

void ec_fsm_slave_conf(ec_fsm_t *);
void ec_fsm_slave_sync(ec_fsm_t *);
void ec_fsm_slave_preop(ec_fsm_t *);
void ec_fsm_slave_fmmu(ec_fsm_t *);
void ec_fsm_slave_saveop(ec_fsm_t *);
void ec_fsm_slave_op(ec_fsm_t *);
void ec_fsm_slave_op2(ec_fsm_t *);

void ec_fsm_sii_start_reading(ec_fsm_t *);
void ec_fsm_sii_read_check(ec_fsm_t *);
void ec_fsm_sii_read_fetch(ec_fsm_t *);
void ec_fsm_sii_start_writing(ec_fsm_t *);
void ec_fsm_sii_write_check(ec_fsm_t *);
void ec_fsm_sii_write_check2(ec_fsm_t *);
void ec_fsm_sii_end(ec_fsm_t *);
void ec_fsm_sii_error(ec_fsm_t *);

void ec_fsm_change_start(ec_fsm_t *);
void ec_fsm_change_check(ec_fsm_t *);
void ec_fsm_change_status(ec_fsm_t *);
void ec_fsm_change_code(ec_fsm_t *);
void ec_fsm_change_ack(ec_fsm_t *);
void ec_fsm_change_ack2(ec_fsm_t *);
void ec_fsm_change_end(ec_fsm_t *);
void ec_fsm_change_error(ec_fsm_t *);

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

    ec_command_init(&fsm->command);
    if (ec_command_prealloc(&fsm->command, EC_MAX_DATA_SIZE)) {
        EC_ERR("FSM failed to allocate FSM command.\n");
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
    ec_command_clear(&fsm->command);
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

/******************************************************************************
 *  master state machine
 *****************************************************************************/

/**
   Master state: START.
   Starts with getting slave count and slave states.
*/

void ec_fsm_master_start(ec_fsm_t *fsm)
{
    ec_command_brd(&fsm->command, 0x0130, 2);
    ec_master_queue_command(fsm->master, &fsm->command);
    fsm->master_state = ec_fsm_master_broadcast;
}

/*****************************************************************************/

/**
   Master state: BROADCAST.
   Processes the broadcast read slave count and slaves states.
*/

void ec_fsm_master_broadcast(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;
    unsigned int topology_change, states_change, i;
    ec_slave_t *slave;
    ec_master_t *master = fsm->master;

    if (command->state != EC_CMD_RECEIVED) {
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

    topology_change = (command->working_counter !=
                       fsm->master_slaves_responding);
    states_change = (EC_READ_U8(command->data) != fsm->master_slave_states);

    fsm->master_slave_states = EC_READ_U8(command->data);
    fsm->master_slaves_responding = command->working_counter;

    if (topology_change) {
        EC_INFO("%i slave%s responding.\n",
                fsm->master_slaves_responding,
                fsm->master_slaves_responding == 1 ? "" : "s");

        if (master->mode == EC_MASTER_MODE_RUNNING) {
            if (fsm->master_slaves_responding == master->slave_count) {
                fsm->master_validation = 1; // start validation later
            }
            else {
                EC_WARN("Invalid slave count. Bus in tainted state.\n");
            }
        }
    }

    if (states_change) {
        EC_INFO("Slave states: ");
        ec_print_states(fsm->master_slave_states);
        printk(".\n");
    }

    // topology change in free-run mode: clear all slaves and scan the bus
    if (topology_change && master->mode == EC_MASTER_MODE_FREERUN) {
        EC_INFO("Scanning bus.\n");

        ec_master_eoe_stop(master);
        ec_master_clear_slaves(master);

        if (!fsm->master_slaves_responding) {
            // no slaves present -> finish state machine.
            fsm->master_state = ec_fsm_master_start;
            fsm->master_state(fsm); // execute immediately
            return;
        }

        // init slaves
        for (i = 0; i < fsm->master_slaves_responding; i++) {
            if (!(slave = (ec_slave_t *) kmalloc(sizeof(ec_slave_t),
                                                 GFP_ATOMIC))) {
                EC_ERR("Failed to allocate slave %i!\n", i);
                fsm->master_state = ec_fsm_master_start;
                fsm->master_state(fsm); // execute immediately
                return;
            }

            if (ec_slave_init(slave, master, i, i + 1)) {
                fsm->master_state = ec_fsm_master_start;
                fsm->master_state(fsm); // execute immediately
                return;
            }

            if (kobject_add(&slave->kobj)) {
                EC_ERR("Failed to add kobject.\n");
                kobject_put(&slave->kobj); // free
                fsm->master_state = ec_fsm_master_start;
                fsm->master_state(fsm); // execute immediately
                return;
            }

            list_add_tail(&slave->list, &master->slaves);
        }

        // begin scanning of slaves
        fsm->slave = list_entry(master->slaves.next, ec_slave_t, list);
        fsm->slave_state = ec_fsm_slave_start_reading;
        fsm->master_state = ec_fsm_master_scan;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    // fetch state from each slave
    fsm->slave = list_entry(master->slaves.next, ec_slave_t, list);
    ec_command_nprd(&fsm->command, fsm->slave->station_address, 0x0130, 2);
    ec_master_queue_command(master, &fsm->command);
    fsm->master_state = ec_fsm_master_states;
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

    // have all states been read?
    if (slave->list.next == &master->slaves) {

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

        fsm->master_state = ec_fsm_master_proc_states;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    // process next slave
    fsm->slave = list_entry(fsm->slave->list.next, ec_slave_t, list);
    ec_command_nprd(&fsm->command, fsm->slave->station_address, 0x0130, 2);
    ec_master_queue_command(master, &fsm->command);
    fsm->master_state = ec_fsm_master_states;
}

/*****************************************************************************/

/**
   Master state: STATES.
   Fetches the AL- and online state of a slave.
*/

void ec_fsm_master_states(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_command_t *command = &fsm->command;
    uint8_t new_state;

    if (command->state != EC_CMD_RECEIVED) {
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    // did the slave not respond to its station address?
    if (command->working_counter != 1) {
        if (slave->online) {
            slave->online = 0;
            EC_INFO("Slave %i: offline.\n", slave->ring_position);
        }
        ec_fsm_master_action_next_slave_state(fsm);
        return;
    }

    // slave responded
    new_state = EC_READ_U8(command->data);
    if (!slave->online) { // slave was offline before
        slave->online = 1;
        slave->state_error = 0;
        slave->current_state = new_state;
        EC_INFO("Slave %i: online (", slave->ring_position);
        ec_print_states(new_state);
        printk(").\n");
    }
    else if (new_state != slave->current_state) {
        EC_INFO("Slave %i: ", slave->ring_position);
        ec_print_states(slave->current_state);
        printk(" -> ");
        ec_print_states(new_state);
        printk(".\n");
        slave->current_state = new_state;
    }

    ec_fsm_master_action_next_slave_state(fsm);
}

/*****************************************************************************/

/**
   Master state: PROC_STATES.
   Processes the slave states.
*/

void ec_fsm_master_proc_states(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave;

    // check if any slaves are not in the state, they're supposed to be
    list_for_each_entry(slave, &master->slaves, list) {
        if (slave->state_error || !slave->online ||
            slave->requested_state == EC_SLAVE_STATE_UNKNOWN ||
            slave->current_state == slave->requested_state) continue;

        EC_INFO("Changing state of slave %i from ", slave->ring_position);
        ec_print_states(slave->current_state);
        printk(" to ");
        ec_print_states(slave->requested_state);
        printk(".\n");

        fsm->slave = slave;
        fsm->slave_state = ec_fsm_slave_conf;
        fsm->change_new = EC_SLAVE_STATE_INIT;
        fsm->change_state = ec_fsm_change_start;
        fsm->master_state = ec_fsm_master_conf;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    if (master->mode == EC_MASTER_MODE_FREERUN) {
        // nothing to configure. check for pending EEPROM write operations.
        list_for_each_entry(slave, &master->slaves, list) {
            if (!slave->new_eeprom_data) continue;

            // found pending EEPROM write operation. execute it!
            EC_INFO("Writing EEPROM of slave %i...\n", slave->ring_position);
            fsm->sii_offset = 0x0000;
            memcpy(fsm->sii_value, slave->new_eeprom_data, 2);
            fsm->sii_mode = 1;
            fsm->sii_state = ec_fsm_sii_start_writing;
            fsm->slave = slave;
            fsm->master_state = ec_fsm_master_eeprom;
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
   Master state: VALIDATE_VENDOR.
   Validates the vendor ID of a slave.
*/

void ec_fsm_master_validate_vendor(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;

    fsm->sii_state(fsm); // execute SII state machine

    if (fsm->sii_state == ec_fsm_sii_error) {
        EC_ERR("Failed to validate vendor ID of slave %i.\n",
               slave->ring_position);
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    if (fsm->sii_state != ec_fsm_sii_end) return;

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
   Master state: VALIDATE_PRODUCT.
   Validates the product ID of a slave.
*/

void ec_fsm_master_validate_product(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;

    fsm->sii_state(fsm); // execute SII state machine

    if (fsm->sii_state == ec_fsm_sii_error) {
        EC_ERR("Failed to validate product code of slave %i.\n",
               slave->ring_position);
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    if (fsm->sii_state != ec_fsm_sii_end) return;

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
        fsm->master_state = ec_fsm_master_reconfigure;
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
   Master state: RECONFIGURE.
   Looks for slave, that have lost their configuration and writes
   their station address, so that they can be reconfigured later.
*/

void ec_fsm_master_reconfigure(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;

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
    ec_command_apwr(command, fsm->slave->ring_position, 0x0010, 2);
    EC_WRITE_U16(command->data, fsm->slave->station_address);
    ec_master_queue_command(fsm->master, command);
    fsm->master_state = ec_fsm_master_address;
}

/*****************************************************************************/

/**
   Master state: ADDRESS.
   Checks, if the new station address has been written to the slave.
*/

void ec_fsm_master_address(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_command_t *command = &fsm->command;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
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
    fsm->master_state = ec_fsm_master_reconfigure;
    fsm->master_state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   Master state: SCAN.
   Executes the sub-statemachine for the scanning of a slave.
*/

void ec_fsm_master_scan(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave = fsm->slave;
    uint16_t coupler_index, coupler_subindex;
    uint16_t reverse_coupler_index, current_coupler_index;
    ec_slave_ident_t *ident;

    fsm->slave_state(fsm); // execute slave state machine

    if (fsm->slave_state != ec_fsm_slave_end) return;

    // have all slaves been fetched?
    if (slave->list.next == &master->slaves) {
        EC_INFO("Bus scanning completed.\n");

        // identify all slaves and calculate coupler addressing

        coupler_index = 0;
        reverse_coupler_index = 0xFFFF;
        current_coupler_index = 0x3FFF;
        coupler_subindex = 0;

        list_for_each_entry(slave, &master->slaves, list)
        {
            // search for identification in "database"
            ident = slave_idents;
            while (ident->type) {
                if (ident->vendor_id == slave->sii_vendor_id
                    && ident->product_code == slave->sii_product_code) {
                    slave->type = ident->type;
                    break;
                }
                ident++;
            }

            if (!slave->type) {
                EC_WARN("FSM: Unknown slave device (vendor 0x%08X,"
                        " code 0x%08X) at position %i.\n",
                        slave->sii_vendor_id, slave->sii_product_code,
                        slave->ring_position);
            }
            else {
                // if the slave is a bus coupler, change adressing base
                if (slave->type->special == EC_TYPE_BUS_COUPLER) {
                    if (slave->sii_alias)
                        current_coupler_index = reverse_coupler_index--;
                    else
                        current_coupler_index = coupler_index++;
                    coupler_subindex = 0;
                }
            }

            // determine initial state.
            if ((slave->type && slave->type->special == EC_TYPE_BUS_COUPLER)) {
                slave->requested_state = EC_SLAVE_STATE_OP;
            }
            else {
                if (master->mode == EC_MASTER_MODE_RUNNING)
                    slave->requested_state = EC_SLAVE_STATE_PREOP;
                else
                    slave->requested_state = EC_SLAVE_STATE_INIT;
            }
            slave->state_error = 0;

            // calculate coupler-based slave address
            slave->coupler_index = current_coupler_index;
            slave->coupler_subindex = coupler_subindex;
            coupler_subindex++;
        }

        if (master->mode == EC_MASTER_MODE_FREERUN) {
            // start EoE processing
            ec_master_eoe_start(master);
        }

        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    // process next slave
    fsm->slave = list_entry(fsm->slave->list.next, ec_slave_t, list);
    fsm->slave_state = ec_fsm_slave_start_reading;
    fsm->slave_state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   Master state: CONF.
   Starts configuring a slave.
*/

void ec_fsm_master_conf(ec_fsm_t *fsm /**< finite state machine */)
{
    fsm->slave_state(fsm); // execute slave's state machine
    if (fsm->slave_state != ec_fsm_slave_end) return;
    fsm->master_state = ec_fsm_master_proc_states;
    fsm->master_state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   Master state: EEPROM.
*/

void ec_fsm_master_eeprom(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;

    fsm->sii_state(fsm); // execute SII state machine

    if (fsm->sii_state == ec_fsm_sii_error) {
        EC_ERR("Failed to write EEPROM contents to slave %i.\n",
               slave->ring_position);
        kfree(slave->new_eeprom_data);
        slave->new_eeprom_data = NULL;
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    if (fsm->sii_state != ec_fsm_sii_end) return;

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

    // restart master state machine.
    fsm->master_state = ec_fsm_master_start;
    fsm->master_state(fsm); // execute immediately
    return;
}

/******************************************************************************
 *  slave state machine
 *****************************************************************************/

/**
   Slave state: START_READING.
   First state of the slave state machine. Writes the station address to the
   slave, according to its ring position.
*/

void ec_fsm_slave_start_reading(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;

    // write station address
    ec_command_apwr(command, fsm->slave->ring_position, 0x0010, 2);
    EC_WRITE_U16(command->data, fsm->slave->station_address);
    ec_master_queue_command(fsm->master, command);
    fsm->slave_state = ec_fsm_slave_read_status;
}

/*****************************************************************************/

/**
   Slave state: READ_STATUS.
*/

void ec_fsm_slave_read_status(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("FSM failed to write station address of slave %i.\n",
               fsm->slave->ring_position);
        fsm->slave_state = ec_fsm_slave_end;
        return;
    }

    // read AL status
    ec_command_nprd(command, fsm->slave->station_address, 0x0130, 2);
    ec_master_queue_command(fsm->master, command);
    fsm->slave_state = ec_fsm_slave_read_base;
}

/*****************************************************************************/

/**
   Slave state: READ_BASE.
*/

void ec_fsm_slave_read_base(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;
    ec_slave_t *slave = fsm->slave;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("FSM failed to read AL status of slave %i.\n",
               fsm->slave->ring_position);
        fsm->slave_state = ec_fsm_slave_end;
        return;
    }

    slave->current_state = EC_READ_U8(command->data);
    if (slave->current_state & EC_ACK) {
        EC_WARN("Slave %i has status error bit set (0x%02X)!\n",
                slave->ring_position, slave->current_state);
        slave->current_state &= 0x0F;
    }

    // read base data
    ec_command_nprd(command, fsm->slave->station_address, 0x0000, 6);
    ec_master_queue_command(fsm->master, command);
    fsm->slave_state = ec_fsm_slave_read_dl;
}

/*****************************************************************************/

/**
   Slave state: READ_DL.
*/

void ec_fsm_slave_read_dl(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;
    ec_slave_t *slave = fsm->slave;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("FSM failed to read base data of slave %i.\n",
               slave->ring_position);
        fsm->slave_state = ec_fsm_slave_end;
        return;
    }

    slave->base_type       = EC_READ_U8 (command->data);
    slave->base_revision   = EC_READ_U8 (command->data + 1);
    slave->base_build      = EC_READ_U16(command->data + 2);
    slave->base_fmmu_count = EC_READ_U8 (command->data + 4);
    slave->base_sync_count = EC_READ_U8 (command->data + 5);

    if (slave->base_fmmu_count > EC_MAX_FMMUS)
        slave->base_fmmu_count = EC_MAX_FMMUS;

    // read data link status
    ec_command_nprd(command, slave->station_address, 0x0110, 2);
    ec_master_queue_command(slave->master, command);
    fsm->slave_state = ec_fsm_slave_eeprom_size;
}

/*****************************************************************************/

/**
   Slave state: EEPROM_SIZE.
   Read the actual size of the EEPROM to allocate the EEPROM image.
*/

void ec_fsm_slave_eeprom_size(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;
    ec_slave_t *slave = fsm->slave;
    uint16_t dl_status;
    unsigned int i;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("FSM failed to read DL status of slave %i.\n",
               slave->ring_position);
        fsm->slave_state = ec_fsm_slave_end;
        return;
    }

    dl_status = EC_READ_U16(command->data);
    for (i = 0; i < 4; i++) {
        slave->dl_link[i] = dl_status & (1 << (4 + i)) ? 1 : 0;
        slave->dl_loop[i] = dl_status & (1 << (8 + i * 2)) ? 1 : 0;
        slave->dl_signal[i] = dl_status & (1 << (9 + i * 2)) ? 1 : 0;
    }

    // Start fetching EEPROM size

    fsm->sii_offset = 0x0040; // first category header
    fsm->sii_mode = 1;
    fsm->sii_state = ec_fsm_sii_start_reading;
    fsm->slave_state = ec_fsm_slave_fetch_eeprom;
    fsm->slave_state(fsm); // execute state immediately
}

/*****************************************************************************/

/**
   Slave state: FETCH_EEPROM.
*/

void ec_fsm_slave_fetch_eeprom(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;
    uint16_t cat_type, cat_size;

    // execute SII state machine
    fsm->sii_state(fsm);

    if (fsm->sii_state == ec_fsm_sii_error) {
        fsm->slave_state = ec_fsm_slave_end;
        EC_ERR("Failed to read EEPROM size of slave %i.\n",
               slave->ring_position);
        return;
    }

    if (fsm->sii_state != ec_fsm_sii_end) return;

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
        EC_ERR("Failed to allocate EEPROM data on slave %i.\n",
               slave->ring_position);
        fsm->slave_state = ec_fsm_slave_end;
        return;
    }

    // Start fetching EEPROM contents

    fsm->sii_offset = 0x0000;
    fsm->sii_mode = 1;
    fsm->sii_state = ec_fsm_sii_start_reading;
    fsm->slave_state = ec_fsm_slave_fetch_eeprom2;
    fsm->slave_state(fsm); // execute state immediately
}

/*****************************************************************************/

/**
   Slave state: FETCH_EEPROM2.
*/

void ec_fsm_slave_fetch_eeprom2(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;
    uint16_t *cat_word, cat_type, cat_size;

    // execute SII state machine
    fsm->sii_state(fsm);

    if (fsm->sii_state == ec_fsm_sii_error) {
        fsm->slave_state = ec_fsm_slave_end;
        EC_ERR("Failed to fetch EEPROM contents of slave %i.\n",
               slave->ring_position);
        return;
    }

    if (fsm->sii_state != ec_fsm_sii_end) return;

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
                if (ec_slave_fetch_general(slave, (uint8_t *) (cat_word + 2)))
                    goto end;
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

 end:
    fsm->slave_state = ec_fsm_slave_end;
}

/*****************************************************************************/

/**
   Slave state: CONF.
*/

void ec_fsm_slave_conf(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = fsm->master;
    ec_command_t *command = &fsm->command;

    fsm->change_state(fsm); // execute state change state machine

    if (fsm->change_state == ec_fsm_change_error) {
        fsm->slave_state = ec_fsm_slave_end;
        return;
    }

    if (fsm->change_state != ec_fsm_change_end) return;

    // slave is now in INIT
    if (slave->current_state == slave->requested_state) {
        fsm->slave_state = ec_fsm_slave_end;
        return;
    }

    // check for slave registration
    if (!slave->type) {
        EC_WARN("Slave %i has unknown type!\n", slave->ring_position);
    }

    // check and reset CRC fault counters
    //ec_slave_check_crc(slave);

    if (!slave->base_fmmu_count) { // no fmmus
        fsm->slave_state = ec_fsm_slave_sync;
        fsm->slave_state(fsm); // execute immediately
        return;
    }

    // reset FMMUs
    ec_command_npwr(command, slave->station_address, 0x0600,
                    EC_FMMU_SIZE * slave->base_fmmu_count);
    memset(command->data, 0x00, EC_FMMU_SIZE * slave->base_fmmu_count);
    ec_master_queue_command(master, command);
    fsm->slave_state = ec_fsm_slave_sync;
}

/*****************************************************************************/

/**
   Slave state: SYNC.
   Configure sync managers.
*/

void ec_fsm_slave_sync(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;
    ec_slave_t *slave = fsm->slave;
    unsigned int j;
    const ec_sync_t *sync;
    ec_eeprom_sync_t *eeprom_sync, mbox_sync;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("Failed to reset FMMUs of slave %i.\n",
               slave->ring_position);
        slave->state_error = 1;
        fsm->slave_state = ec_fsm_slave_end;
        return;
    }

    if (!slave->base_sync_count) { // no sync managers
        fsm->slave_state = ec_fsm_slave_preop;
        fsm->slave_state(fsm); // execute immediately
        return;
    }

    // configure sync managers
    ec_command_npwr(command, slave->station_address, 0x0800,
                    EC_SYNC_SIZE * slave->base_sync_count);
    memset(command->data, 0x00, EC_SYNC_SIZE * slave->base_sync_count);

    // known slave type, take type's SM information
    if (slave->type) {
        for (j = 0; slave->type->sync_managers[j] && j < EC_MAX_SYNC; j++) {
            sync = slave->type->sync_managers[j];
            ec_sync_config(sync, slave, command->data + EC_SYNC_SIZE * j);
        }
    }

    // unknown type, but slave has mailbox
    else if (slave->sii_mailbox_protocols)
    {
        // does it supply sync manager configurations in its EEPROM?
        if (!list_empty(&slave->eeprom_syncs)) {
            list_for_each_entry(eeprom_sync, &slave->eeprom_syncs, list) {
                if (eeprom_sync->index >= slave->base_sync_count) {
                    EC_ERR("Invalid sync manager configuration found!");
                    fsm->slave_state = ec_fsm_slave_end;
                    return;
                }
                ec_eeprom_sync_config(eeprom_sync,
                                      command->data + EC_SYNC_SIZE
                                      * eeprom_sync->index);
            }
        }

        // no sync manager information; guess mailbox settings
        else {
            mbox_sync.physical_start_address =
                slave->sii_rx_mailbox_offset;
            mbox_sync.length = slave->sii_rx_mailbox_size;
            mbox_sync.control_register = 0x26;
            mbox_sync.enable = 1;
            ec_eeprom_sync_config(&mbox_sync, command->data);

            mbox_sync.physical_start_address =
                slave->sii_tx_mailbox_offset;
            mbox_sync.length = slave->sii_tx_mailbox_size;
            mbox_sync.control_register = 0x22;
            mbox_sync.enable = 1;
            ec_eeprom_sync_config(&mbox_sync,
                                  command->data + EC_SYNC_SIZE);
        }

        EC_INFO("Mailbox configured for unknown slave %i\n",
                slave->ring_position);
    }

    ec_master_queue_command(fsm->master, command);
    fsm->slave_state = ec_fsm_slave_preop;
}

/*****************************************************************************/

/**
   Slave state: PREOP.
   Change slave state to PREOP.
*/

void ec_fsm_slave_preop(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;
    ec_slave_t *slave = fsm->slave;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("Failed to set sync managers on slave %i.\n",
               slave->ring_position);
        slave->state_error = 1;
        fsm->slave_state = ec_fsm_slave_end;
        return;
    }

    fsm->change_new = EC_SLAVE_STATE_PREOP;
    fsm->change_state = ec_fsm_change_start;
    fsm->slave_state = ec_fsm_slave_fmmu;
    fsm->change_state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   Slave state: FMMU.
   Configure FMMUs.
*/

void ec_fsm_slave_fmmu(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = fsm->master;
    ec_command_t *command = &fsm->command;
    unsigned int j;

    fsm->change_state(fsm); // execute state change state machine

    if (fsm->change_state == ec_fsm_change_error) {
        fsm->slave_state = ec_fsm_slave_end;
        return;
    }

    if (fsm->change_state != ec_fsm_change_end) return;

    // slave is now in PREOP
    if (slave->current_state == slave->requested_state) {
        fsm->slave_state = ec_fsm_slave_end;
        return;
    }

    // stop activation here for slaves without type
    if (!slave->type) {
        fsm->slave_state = ec_fsm_slave_end;
        return;
    }

    if (!slave->base_fmmu_count) {
        fsm->slave_state = ec_fsm_slave_saveop;
        fsm->slave_state(fsm); // execute immediately
        return;
    }

    // configure FMMUs
    ec_command_npwr(command, slave->station_address,
                    0x0600, EC_FMMU_SIZE * slave->base_fmmu_count);
    memset(command->data, 0x00, EC_FMMU_SIZE * slave->base_fmmu_count);
    for (j = 0; j < slave->fmmu_count; j++) {
        ec_fmmu_config(&slave->fmmus[j], slave,
                       command->data + EC_FMMU_SIZE * j);
    }

    ec_master_queue_command(master, command);
    fsm->slave_state = ec_fsm_slave_saveop;
}

/*****************************************************************************/

/**
   Slave state: SAVEOP.
   Set slave state to SAVEOP.
*/

void ec_fsm_slave_saveop(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;

    if (fsm->slave->base_fmmu_count && (command->state != EC_CMD_RECEIVED ||
                                        command->working_counter != 1)) {
        EC_ERR("FSM failed to set FMMUs on slave %i.\n",
               fsm->slave->ring_position);
        fsm->slave->state_error = 1;
        fsm->slave_state = ec_fsm_slave_end;
        return;
    }

    // set state to SAVEOP
    fsm->slave_state = ec_fsm_slave_op;
    fsm->change_new = EC_SLAVE_STATE_SAVEOP;
    fsm->change_state = ec_fsm_change_start;
    fsm->change_state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   Slave state: OP.
   Set slave state to OP.
*/

void ec_fsm_slave_op(ec_fsm_t *fsm /**< finite state machine */)
{
    fsm->change_state(fsm); // execute state change state machine

    if (fsm->change_state == ec_fsm_change_error) {
        fsm->slave_state = ec_fsm_slave_end;
        return;
    }

    if (fsm->change_state != ec_fsm_change_end) return;

    // slave is now in SAVEOP
    if (fsm->slave->current_state == fsm->slave->requested_state) {
        fsm->slave_state = ec_fsm_slave_end;
        return;
    }

    // set state to OP
    fsm->slave_state = ec_fsm_slave_op2;
    fsm->change_new = EC_SLAVE_STATE_OP;
    fsm->change_state = ec_fsm_change_start;
    fsm->change_state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   Slave state: OP2
   Executes the change state machine, until the OP state is set.
*/

void ec_fsm_slave_op2(ec_fsm_t *fsm /**< finite state machine */)
{
    fsm->change_state(fsm); // execute state change state machine

    if (fsm->change_state == ec_fsm_change_error) {
        fsm->slave_state = ec_fsm_slave_end;
        return;
    }

    if (fsm->change_state != ec_fsm_change_end) return;

    // slave is now in OP
    fsm->slave_state = ec_fsm_slave_end;
}

/*****************************************************************************/

/**
   Slave state: END.
   End state of the slave state machine.
*/

void ec_fsm_slave_end(ec_fsm_t *fsm /**< finite state machine */)
{
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
    ec_command_t *command = &fsm->command;

    // initiate read operation
    if (fsm->sii_mode) {
        ec_command_npwr(command, fsm->slave->station_address, 0x502, 4);
    }
    else {
        ec_command_apwr(command, fsm->slave->ring_position, 0x502, 4);
    }

    EC_WRITE_U8 (command->data,     0x00); // read-only access
    EC_WRITE_U8 (command->data + 1, 0x01); // request read operation
    EC_WRITE_U16(command->data + 2, fsm->sii_offset);
    ec_master_queue_command(fsm->master, command);
    fsm->sii_state = ec_fsm_sii_read_check;
}

/*****************************************************************************/

/**
   SII state: READ_CHECK.
   Checks, if the SII-read-command has been sent and issues a fetch command.
*/

void ec_fsm_sii_read_check(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("SII: Reception of read command failed.\n");
        fsm->sii_state = ec_fsm_sii_error;
        return;
    }

    fsm->sii_start = get_cycles();

    // issue check/fetch command
    if (fsm->sii_mode) {
        ec_command_nprd(command, fsm->slave->station_address, 0x502, 10);
    }
    else {
        ec_command_aprd(command, fsm->slave->ring_position, 0x502, 10);
    }

    ec_master_queue_command(fsm->master, command);
    fsm->sii_state = ec_fsm_sii_read_fetch;
}

/*****************************************************************************/

/**
   SII state: READ_FETCH.
   Fetches the result of an SII-read command.
*/

void ec_fsm_sii_read_fetch(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("SII: Reception of check/fetch command failed.\n");
        fsm->sii_state = ec_fsm_sii_error;
        return;
    }

    // check "busy bit"
    if (EC_READ_U8(command->data + 1) & 0x81) {
        // still busy... timeout?
        if (get_cycles() - fsm->sii_start >= (cycles_t) 10 * cpu_khz) {
            EC_ERR("SII: Timeout.\n");
            fsm->sii_state = ec_fsm_sii_error;
#if 0
            EC_DBG("SII busy: %02X %02X %02X %02X\n",
                   EC_READ_U8(command->data + 0),
                   EC_READ_U8(command->data + 1),
                   EC_READ_U8(command->data + 2),
                   EC_READ_U8(command->data + 3));
#endif
        }

        // issue check/fetch command again
        if (fsm->sii_mode) {
            ec_command_nprd(command, fsm->slave->station_address, 0x502, 10);
        }
        else {
            ec_command_aprd(command, fsm->slave->ring_position, 0x502, 10);
        }
        ec_master_queue_command(fsm->master, command);
        return;
    }

#if 0
    EC_DBG("SII rec: %02X %02X %02X %02X - %02X %02X %02X %02X\n",
           EC_READ_U8(command->data + 0), EC_READ_U8(command->data + 1),
           EC_READ_U8(command->data + 2), EC_READ_U8(command->data + 3),
           EC_READ_U8(command->data + 6), EC_READ_U8(command->data + 7),
           EC_READ_U8(command->data + 8), EC_READ_U8(command->data + 9));
#endif

    // SII value received.
    memcpy(fsm->sii_value, command->data + 6, 4);
    fsm->sii_state = ec_fsm_sii_end;
}

/*****************************************************************************/

/**
   SII state: START_WRITING.
   Starts reading the slave information interface.
*/

void ec_fsm_sii_start_writing(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;

    // initiate write operation
    ec_command_npwr(command, fsm->slave->station_address, 0x502, 8);
    EC_WRITE_U8 (command->data,     0x01); // enable write access
    EC_WRITE_U8 (command->data + 1, 0x02); // request write operation
    EC_WRITE_U32(command->data + 2, fsm->sii_offset);
    memcpy(command->data + 6, fsm->sii_value, 2);
    ec_master_queue_command(fsm->master, command);
    fsm->sii_state = ec_fsm_sii_write_check;
}

/*****************************************************************************/

/**
   SII state: WRITE_CHECK.
*/

void ec_fsm_sii_write_check(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("SII: Reception of write command failed.\n");
        fsm->sii_state = ec_fsm_sii_error;
        return;
    }

    fsm->sii_start = get_cycles();

    // issue check/fetch command
    ec_command_nprd(command, fsm->slave->station_address, 0x502, 2);
    ec_master_queue_command(fsm->master, command);
    fsm->sii_state = ec_fsm_sii_write_check2;
}

/*****************************************************************************/

/**
   SII state: WRITE_CHECK2.
*/

void ec_fsm_sii_write_check2(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("SII: Reception of write check command failed.\n");
        fsm->sii_state = ec_fsm_sii_error;
        return;
    }

    if (EC_READ_U8(command->data + 1) & 0x82) {
        // still busy... timeout?
        if (get_cycles() - fsm->sii_start >= (cycles_t) 10 * cpu_khz) {
            EC_ERR("SII: Write timeout.\n");
            fsm->sii_state = ec_fsm_sii_error;
        }

        // issue check/fetch command again
        ec_master_queue_command(fsm->master, command);
    }
    else if (EC_READ_U8(command->data + 1) & 0x40) {
        EC_ERR("SII: Write operation failed!\n");
        fsm->sii_state = ec_fsm_sii_error;
    }
    else { // success
        fsm->sii_state = ec_fsm_sii_end;
    }
}

/*****************************************************************************/

/**
   SII state: END.
   End state of the slave SII state machine.
*/

void ec_fsm_sii_end(ec_fsm_t *fsm /**< finite state machine */)
{
}

/*****************************************************************************/

/**
   SII state: ERROR.
   End state of the slave SII state machine.
*/

void ec_fsm_sii_error(ec_fsm_t *fsm /**< finite state machine */)
{
}

/******************************************************************************
 *  state change state machine
 *****************************************************************************/

/**
   Change state: START.
*/

void ec_fsm_change_start(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;
    ec_slave_t *slave = fsm->slave;

    // write new state to slave
    ec_command_npwr(command, slave->station_address, 0x0120, 2);
    EC_WRITE_U16(command->data, fsm->change_new);
    ec_master_queue_command(fsm->master, command);
    fsm->change_state = ec_fsm_change_check;
}

/*****************************************************************************/

/**
   Change state: CHECK.
*/

void ec_fsm_change_check(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;
    ec_slave_t *slave = fsm->slave;

    if (command->state != EC_CMD_RECEIVED) {
        EC_ERR("Failed to send state command to slave %i!\n",
               fsm->slave->ring_position);
        slave->state_error = 1;
        fsm->change_state = ec_fsm_change_error;
        return;
    }

    if (command->working_counter != 1) {
        EC_ERR("Failed to set state 0x%02X on slave %i: Slave did not"
               " respond.\n", fsm->change_new, fsm->slave->ring_position);
        slave->state_error = 1;
        fsm->change_state = ec_fsm_change_error;
        return;
    }

    fsm->change_start = get_cycles();

    // read AL status from slave
    ec_command_nprd(command, slave->station_address, 0x0130, 2);
    ec_master_queue_command(fsm->master, command);
    fsm->change_state = ec_fsm_change_status;
}

/*****************************************************************************/

/**
   Change state: STATUS.
*/

void ec_fsm_change_status(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;
    ec_slave_t *slave = fsm->slave;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("Failed to check state 0x%02X on slave %i.\n",
               fsm->change_new, slave->ring_position);
        slave->state_error = 1;
        fsm->change_state = ec_fsm_change_error;
        return;
    }

    slave->current_state = EC_READ_U8(command->data);

    if (slave->current_state == fsm->change_new) {
        // state has been set successfully
        fsm->change_state = ec_fsm_change_end;
        return;
    }

    if (slave->current_state & 0x10) {
        // state change error
        fsm->change_new = slave->current_state & 0x0F;
        EC_ERR("Failed to set state 0x%02X - Slave %i refused state change"
               " (code 0x%02X)!\n", fsm->change_new, slave->ring_position,
               slave->current_state);
        // fetch AL status error code
        ec_command_nprd(command, slave->station_address, 0x0134, 2);
        ec_master_queue_command(fsm->master, command);
        fsm->change_state = ec_fsm_change_code;
        return;
    }

    if (get_cycles() - fsm->change_start >= (cycles_t) 10 * cpu_khz) {
        // timeout while checking
        slave->state_error = 1;
        fsm->change_state = ec_fsm_change_error;
        EC_ERR("Timeout while setting state 0x%02X on slave %i.\n",
               fsm->change_new, slave->ring_position);
        return;
    }

    // still old state: check again
    ec_command_nprd(command, slave->station_address, 0x0130, 2);
    ec_master_queue_command(fsm->master, command);
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
    {0x0020, "Slave needs cold start"},
    {0x0021, "Slave needs INIT"},
    {0x0022, "Slave needs PREOP"},
    {0x0023, "Slave needs SAVEOP"},
    {}
};

/*****************************************************************************/

/**
   Change state: CODE.
*/

void ec_fsm_change_code(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;
    ec_slave_t *slave = fsm->slave;
    uint32_t code;
    const ec_code_msg_t *al_msg;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("Reception of AL status code command failed.\n");
        slave->state_error = 1;
        fsm->change_state = ec_fsm_change_error;
        return;
    }

    if ((code = EC_READ_U16(command->data))) {
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
    ec_command_npwr(command, slave->station_address, 0x0120, 2);
    EC_WRITE_U16(command->data, slave->current_state);
    ec_master_queue_command(fsm->master, command);
    fsm->change_state = ec_fsm_change_ack;
}

/*****************************************************************************/

/**
   Change state: ACK.
*/

void ec_fsm_change_ack(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;
    ec_slave_t *slave = fsm->slave;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("Reception of state ack command failed.\n");
        slave->state_error = 1;
        fsm->change_state = ec_fsm_change_error;
        return;
    }

    // read new AL status
    ec_command_nprd(command, slave->station_address, 0x0130, 2);
    ec_master_queue_command(fsm->master, command);
    fsm->change_state = ec_fsm_change_ack2;
}

/*****************************************************************************/

/**
   Change state: ACK.
   Acknowledge 2.
*/

void ec_fsm_change_ack2(ec_fsm_t *fsm /**< finite state machine */)
{
    ec_command_t *command = &fsm->command;
    ec_slave_t *slave = fsm->slave;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("Reception of state ack check command failed.\n");
        slave->state_error = 1;
        fsm->change_state = ec_fsm_change_error;
        return;
    }

    slave->current_state = EC_READ_U8(command->data);

    if (slave->current_state == fsm->change_new) {
        EC_INFO("Acknowleged state 0x%02X on slave %i.\n",
                slave->current_state, slave->ring_position);
        slave->state_error = 1;
        fsm->change_state = ec_fsm_change_error;
        return;
    }

    EC_WARN("Failed to acknowledge state 0x%02X on slave %i"
            " - Timeout!\n", fsm->change_new, slave->ring_position);
    slave->state_error = 1;
    fsm->change_state = ec_fsm_change_error;
}

/*****************************************************************************/

/**
   Change state: END.
*/

void ec_fsm_change_end(ec_fsm_t *fsm /**< finite state machine */)
{
}

/*****************************************************************************/

/**
   Change state: ERROR.
*/

void ec_fsm_change_error(ec_fsm_t *fsm /**< finite state machine */)
{
}

/*****************************************************************************/
