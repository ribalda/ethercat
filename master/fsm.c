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

#define EC_CAT_MEM 0x100

/*****************************************************************************/

const ec_code_msg_t al_status_messages[];

/*****************************************************************************/

void ec_fsm_master_start(ec_fsm_t *);
void ec_fsm_master_wait(ec_fsm_t *);
void ec_fsm_master_scan(ec_fsm_t *);
void ec_fsm_master_conf(ec_fsm_t *);

void ec_fsm_slave_start_reading(ec_fsm_t *);
void ec_fsm_slave_read_status(ec_fsm_t *);
void ec_fsm_slave_read_base(ec_fsm_t *);
void ec_fsm_slave_read_dl(ec_fsm_t *);
void ec_fsm_slave_prepare_sii(ec_fsm_t *);
void ec_fsm_slave_read_sii(ec_fsm_t *);
void ec_fsm_slave_category_header(ec_fsm_t *);
void ec_fsm_slave_category_data(ec_fsm_t *);
void ec_fsm_slave_conf(ec_fsm_t *);
void ec_fsm_slave_end(ec_fsm_t *);

void ec_fsm_slave_conf(ec_fsm_t *);
void ec_fsm_slave_sync(ec_fsm_t *);
void ec_fsm_slave_preop(ec_fsm_t *);
void ec_fsm_slave_fmmu(ec_fsm_t *);
void ec_fsm_slave_saveop(ec_fsm_t *);
void ec_fsm_slave_op(ec_fsm_t *);
void ec_fsm_slave_op2(ec_fsm_t *);

void ec_fsm_sii_start_reading(ec_fsm_t *);
void ec_fsm_sii_check(ec_fsm_t *);
void ec_fsm_sii_fetch(ec_fsm_t *);
void ec_fsm_sii_finished(ec_fsm_t *);
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

int ec_fsm_init(ec_fsm_t *fsm, ec_master_t *master)
{
    fsm->master = master;
    fsm->master_state = ec_fsm_master_start;
    fsm->master_slaves_responding = 0;
    fsm->master_slave_states = EC_SLAVE_STATE_UNKNOWN;
    fsm->slave_cat_data = NULL;

    ec_command_init(&fsm->command);
    if (ec_command_prealloc(&fsm->command, EC_MAX_DATA_SIZE)) {
        EC_ERR("FSM failed to allocate FSM command.\n");
        return -1;
    }

    return 0;
}

/*****************************************************************************/

void ec_fsm_clear(ec_fsm_t *fsm)
{
    if (fsm->slave_cat_data) kfree(fsm->slave_cat_data);
    ec_command_clear(&fsm->command);
}

/*****************************************************************************/

void ec_fsm_reset(ec_fsm_t *fsm)
{
    fsm->master_state = ec_fsm_master_start;
    fsm->master_slaves_responding = 0;
    fsm->master_slave_states = EC_SLAVE_STATE_UNKNOWN;

    if (fsm->slave_cat_data) {
        kfree(fsm->slave_cat_data);
        fsm->slave_cat_data = NULL;
    }
}

/*****************************************************************************/

void ec_fsm_execute(ec_fsm_t *fsm)
{
    fsm->master_state(fsm);
}

/******************************************************************************
 *  master state machine
 *****************************************************************************/

/**
   State: Start.
   Starts with getting slave count and slave states.
*/

void ec_fsm_master_start(ec_fsm_t *fsm)
{
    ec_command_brd(&fsm->command, 0x0130, 2);
    ec_master_queue_command(fsm->master, &fsm->command);
    fsm->master_state = ec_fsm_master_wait;
}

/*****************************************************************************/

void ec_fsm_master_wait(ec_fsm_t *fsm)
{
    ec_command_t *command = &fsm->command;
    unsigned int topology_change, i, eoe_slaves_active;
    ec_slave_t *slave;

    if (command->state != EC_CMD_RECEIVED) {
        if (!fsm->master->device->link_state)
            // treat link down as topology change
            fsm->master_slaves_responding = 0;
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    if (command->working_counter == fsm->master_slaves_responding &&
        command->data[0] == fsm->master_slave_states)
    {
        // check if any slaves are not in the state, they're supposed to be
        list_for_each_entry(slave, &fsm->master->slaves, list) {
            if (slave->state_error ||
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

        // nothing to configure...
        eoe_slaves_active = 0;
        list_for_each_entry(slave, &fsm->master->slaves, list) {
            if (slave->sii_mailbox_protocols & EC_MBOX_EOE) {
                eoe_slaves_active++;
            }
        }

        if (eoe_slaves_active && !list_empty(&fsm->master->eoe_handlers))
            ec_master_eoe_start(fsm->master);

        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    topology_change = command->working_counter !=
        fsm->master_slaves_responding;

    fsm->master_slaves_responding = command->working_counter;
    fsm->master_slave_states = command->data[0];

    EC_INFO("FSM: %i slave%s responding (", fsm->master_slaves_responding,
            fsm->master_slaves_responding == 1 ? "" : "s");
    ec_print_states(fsm->master_slave_states);
    printk(")\n");

    if (!topology_change || fsm->master->mode == EC_MASTER_MODE_RUNNING) {
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    EC_INFO("Topology change detected - Scanning bus.\n");

    ec_master_eoe_stop(fsm->master);
    ec_master_clear_slaves(fsm->master);

    if (!fsm->master_slaves_responding) {
        // no slaves present -> finish state machine.
        fsm->master_state = ec_fsm_master_start;
        fsm->master_state(fsm); // execute immediately
        return;
    }

    // init slaves
    for (i = 0; i < fsm->master_slaves_responding; i++) {
        if (!(slave =
              (ec_slave_t *) kmalloc(sizeof(ec_slave_t), GFP_ATOMIC))) {
            EC_ERR("FSM failed to allocate slave %i!\n", i);
            fsm->master_state = ec_fsm_master_start;
            return;
        }

        if (ec_slave_init(slave, fsm->master, i, i + 1)) {
            fsm->master_state = ec_fsm_master_start;
            return;
        }

        if (kobject_add(&slave->kobj)) {
            EC_ERR("FSM failed to add kobject.\n");
            kobject_put(&slave->kobj); // free
            fsm->master_state = ec_fsm_master_start;
            return;
        }

        list_add_tail(&slave->list, &fsm->master->slaves);
    }

    // begin scanning of slaves
    fsm->slave = list_entry(fsm->master->slaves.next, ec_slave_t, list);
    fsm->slave_state = ec_fsm_slave_start_reading;

    fsm->master_state = ec_fsm_master_scan;
    fsm->master_state(fsm); // execute immediately
}

/*****************************************************************************/

/**
   State: Get Slave.
   Executes the sub-statemachine of a slave.
*/

void ec_fsm_master_scan(ec_fsm_t *fsm)
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave = fsm->slave;

    fsm->slave_state(fsm); // execute slave state machine

    if (fsm->slave_state != ec_fsm_slave_end) return;

    // have all slaves been fetched?
    if (slave->list.next == &master->slaves)
    {
        uint16_t coupler_index, coupler_subindex;
        uint16_t reverse_coupler_index, current_coupler_index;
        ec_slave_t *slave;
        ec_slave_ident_t *ident;

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

            // calculate coupler-based slave address
            slave->coupler_index = current_coupler_index;
            slave->coupler_subindex = coupler_subindex;
            coupler_subindex++;
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
   Free-Run state: Configure slaves.
*/

void ec_fsm_master_conf(ec_fsm_t *fsm)
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave;

    fsm->slave_state(fsm); // execute slave's state machine

    if (fsm->slave_state != ec_fsm_slave_end) return;

    // check if any slaves are not in the state, they're supposed to be
    list_for_each_entry(slave, &master->slaves, list) {
        if (slave->state_error ||
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

    fsm->master_state = ec_fsm_master_start;
    fsm->master_state(fsm); // execute immediately
}

/******************************************************************************
 *  slave state machine
 *****************************************************************************/

/**
   Slave state: Start.
   First state of the slave state machine. Writes the station address to the
   slave, according to its ring position.
*/

void ec_fsm_slave_start_reading(ec_fsm_t *fsm)
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
   Slave state: Read status.
*/

void ec_fsm_slave_read_status(ec_fsm_t *fsm)
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
   Slave state: Read base.
*/

void ec_fsm_slave_read_base(ec_fsm_t *fsm)
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
   Slave state: Read DL.
*/

void ec_fsm_slave_read_dl(ec_fsm_t *fsm)
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
    fsm->slave_state = ec_fsm_slave_prepare_sii;
}

/*****************************************************************************/

/**
   Slave state: Prepare SII.
*/

void ec_fsm_slave_prepare_sii(ec_fsm_t *fsm)
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

    fsm->sii_offset = 0x0004;
    fsm->sii_state = ec_fsm_sii_start_reading;
    fsm->slave_sii_num = 0;
    fsm->slave_state = ec_fsm_slave_read_sii;
    fsm->slave_state(fsm); // execute state immediately
}

/*****************************************************************************/

/**
   Slave state: Read SII.
*/

void ec_fsm_slave_read_sii(ec_fsm_t *fsm)
{
    ec_slave_t *slave = fsm->slave;

    // execute SII state machine
    fsm->sii_state(fsm);

    if (fsm->sii_state == ec_fsm_sii_error) {
        fsm->slave_state = ec_fsm_slave_end;
        EC_ERR("FSM failed to read SII data at 0x%04X on slave %i.\n",
               fsm->sii_offset, slave->ring_position);
        return;
    }

    if (fsm->sii_state != ec_fsm_sii_finished) return;

    switch (fsm->slave_sii_num) {
        case 0:
            slave->sii_alias = fsm->sii_result & 0xFFFF;
            fsm->sii_offset = 0x0008;
            break;
        case 1:
            slave->sii_vendor_id = fsm->sii_result;
            fsm->sii_offset = 0x000A;
            break;
        case 2:
            slave->sii_product_code = fsm->sii_result;
            fsm->sii_offset = 0x000C;
            break;
        case 3:
            slave->sii_revision_number = fsm->sii_result;
            fsm->sii_offset = 0x000E;
            break;
        case 4:
            slave->sii_serial_number = fsm->sii_result;
            fsm->sii_offset = 0x0018;
            break;
        case 5:
            slave->sii_rx_mailbox_offset = fsm->sii_result & 0xFFFF;
            slave->sii_rx_mailbox_size = fsm->sii_result >> 16;
            fsm->sii_offset = 0x001A;
            break;
        case 6:
            slave->sii_tx_mailbox_offset = fsm->sii_result & 0xFFFF;
            slave->sii_tx_mailbox_size = fsm->sii_result >> 16;
            fsm->sii_offset = 0x001C;
            break;
        case 7:
            slave->sii_mailbox_protocols = fsm->sii_result & 0xFFFF;

            fsm->slave_cat_offset = 0x0040;

            if (fsm->slave_cat_data) {
                EC_INFO("FSM freeing old category data on slave %i...\n",
                        fsm->slave->ring_position);
                kfree(fsm->slave_cat_data);
            }

            if (!(fsm->slave_cat_data =
                  (uint8_t *) kmalloc(EC_CAT_MEM, GFP_ATOMIC))) {
                EC_ERR("FSM Failed to allocate category data.\n");
                fsm->slave_state = ec_fsm_slave_end;
                return;
            }

            // start reading first category header
            fsm->sii_offset = fsm->slave_cat_offset;
            fsm->sii_state = ec_fsm_sii_start_reading;

            fsm->slave_state = ec_fsm_slave_category_header;
            fsm->slave_state(fsm); // execute state immediately
            return;
    }

    fsm->slave_sii_num++;
    fsm->sii_state = ec_fsm_sii_start_reading;
    fsm->slave_state(fsm); // execute state immediately
}

/*****************************************************************************/

/**
   Slave state: Read categories.
   Start reading categories.
*/

void ec_fsm_slave_category_header(ec_fsm_t *fsm)
{
    // execute SII state machine
    fsm->sii_state(fsm);

    if (fsm->sii_state == ec_fsm_sii_error) {
        kfree(fsm->slave_cat_data);
        fsm->slave_cat_data = NULL;
        fsm->slave_state = ec_fsm_slave_end;
        EC_ERR("FSM failed to read category header at 0x%04X on slave %i.\n",
               fsm->slave_cat_offset, fsm->slave->ring_position);
        return;
    }

    if (fsm->sii_state != ec_fsm_sii_finished) return;

    // last category?
    if ((fsm->sii_result & 0xFFFF) == 0xFFFF) {
        kfree(fsm->slave_cat_data);
        fsm->slave_cat_data = NULL;
        fsm->slave_state = ec_fsm_slave_end;
        return;
    }

    fsm->slave_cat_type = fsm->sii_result & 0x7FFF;
    fsm->slave_cat_words = (fsm->sii_result >> 16) & 0xFFFF;

    if (fsm->slave_cat_words > EC_CAT_MEM * 2) {
        EC_ERR("FSM category memory too small! %i words needed.\n",
               fsm->slave_cat_words);
        fsm->slave_state = ec_fsm_slave_end;
        return;
    }

    // start reading category data
    fsm->slave_cat_data_offset = 0;
    fsm->sii_offset = (fsm->slave_cat_offset + 2 +
                       fsm->slave_cat_data_offset);
    fsm->sii_state = ec_fsm_sii_start_reading;
    fsm->slave_state = ec_fsm_slave_category_data;
    fsm->slave_state(fsm); // execute state immediately
}

/*****************************************************************************/

/**
   Slave state: Category data.
   Reads category data.
*/

void ec_fsm_slave_category_data(ec_fsm_t *fsm)
{
    // execute SII state machine
    fsm->sii_state(fsm);

    if (fsm->sii_state == ec_fsm_sii_error) {
        kfree(fsm->slave_cat_data);
        fsm->slave_cat_data = NULL;
        fsm->slave_state = ec_fsm_slave_end;
        EC_ERR("FSM failed to read category 0x%02X data at 0x%04X"
               " on slave %i.\n", fsm->slave_cat_type, fsm->sii_offset,
               fsm->slave->ring_position);
        return;
    }

    if (fsm->sii_state != ec_fsm_sii_finished) return;

    fsm->slave_cat_data[fsm->slave_cat_data_offset * 2] =
        fsm->sii_result & 0xFF;
    fsm->slave_cat_data[fsm->slave_cat_data_offset * 2 + 1] =
        (fsm->sii_result >> 8) & 0xFF;

    // read second word "on the fly"
    if (fsm->slave_cat_data_offset + 1 < fsm->slave_cat_words) {
        fsm->slave_cat_data_offset++;
        fsm->slave_cat_data[fsm->slave_cat_data_offset * 2] =
            (fsm->sii_result >> 16) & 0xFF;
        fsm->slave_cat_data[fsm->slave_cat_data_offset * 2 + 1] =
            (fsm->sii_result >> 24) & 0xFF;
    }

    fsm->slave_cat_data_offset++;

    if (fsm->slave_cat_data_offset < fsm->slave_cat_words) {
        fsm->sii_offset = (fsm->slave_cat_offset + 2 +
                           fsm->slave_cat_data_offset);
        fsm->sii_state = ec_fsm_sii_start_reading;
        fsm->slave_state = ec_fsm_slave_category_data;
        fsm->slave_state(fsm); // execute state immediately
        return;
    }

    // category data complete
    switch (fsm->slave_cat_type)
    {
        case 0x000A:
            if (ec_slave_fetch_strings(fsm->slave, fsm->slave_cat_data))
                goto out_free;
            break;
        case 0x001E:
            if (ec_slave_fetch_general(fsm->slave, fsm->slave_cat_data))
                goto out_free;
            break;
        case 0x0028:
            break;
        case 0x0029:
            if (ec_slave_fetch_sync(fsm->slave, fsm->slave_cat_data,
                                    fsm->slave_cat_words))
                goto out_free;
            break;
        case 0x0032:
            if (ec_slave_fetch_pdo(fsm->slave, fsm->slave_cat_data,
                                   fsm->slave_cat_words,
                                   EC_TX_PDO))
                goto out_free;
            break;
        case 0x0033:
            if (ec_slave_fetch_pdo(fsm->slave, fsm->slave_cat_data,
                                   fsm->slave_cat_words,
                                   EC_RX_PDO))
                goto out_free;
            break;
        default:
            EC_WARN("FSM: Unknown category type 0x%04X in slave %i.\n",
                    fsm->slave_cat_type, fsm->slave->ring_position);
    }

    // start reading next category header
    fsm->slave_cat_offset += 2 + fsm->slave_cat_words;
    fsm->sii_offset = fsm->slave_cat_offset;
    fsm->sii_state = ec_fsm_sii_start_reading;
    fsm->slave_state = ec_fsm_slave_category_header;
    fsm->slave_state(fsm); // execute state immediately
    return;

 out_free:
    kfree(fsm->slave_cat_data);
    fsm->slave_cat_data = NULL;
    fsm->slave_state = ec_fsm_slave_end;
}

/*****************************************************************************/

/**
   Slave state: Start configuring.
*/

void ec_fsm_slave_conf(ec_fsm_t *fsm)
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
   Slave state: Configure sync managers.
*/

void ec_fsm_slave_sync(ec_fsm_t *fsm)
{
    ec_command_t *command = &fsm->command;
    ec_slave_t *slave = fsm->slave;
    unsigned int j;
    const ec_sync_t *sync;
    ec_eeprom_sync_t *eeprom_sync, mbox_sync;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("FSM failed to reset FMMUs of slave %i.\n",
               slave->ring_position);
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
            ec_sync_config(sync, command->data + EC_SYNC_SIZE * j);
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
   Slave state: Change slave state to PREOP.
*/

void ec_fsm_slave_preop(ec_fsm_t *fsm)
{
    ec_command_t *command = &fsm->command;
    ec_slave_t *slave = fsm->slave;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("FSM failed to set sync managers on slave %i.\n",
               slave->ring_position);
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
   Slave state: Configure FMMUs.
*/

void ec_fsm_slave_fmmu(ec_fsm_t *fsm)
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

#if 0
    // slaves that are not registered are only brought into PREOP
    // state -> nice blinking and mailbox communication possible
    if (!slave->registered && !slave->type->special) {
        EC_WARN("Slave %i was not registered!\n", slave->ring_position);
        fsm->slave_state = ec_fsm_slave_end;
        return;
    }
#endif

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
        ec_fmmu_config(&slave->fmmus[j], command->data + EC_FMMU_SIZE * j);
    }

    ec_master_queue_command(master, command);
    fsm->slave_state = ec_fsm_slave_saveop;
}

/*****************************************************************************/

/**
   Slave state: Set slave state to SAVEOP.
*/

void ec_fsm_slave_saveop(ec_fsm_t *fsm)
{
    ec_command_t *command = &fsm->command;

    if (fsm->slave->base_fmmu_count && (command->state != EC_CMD_RECEIVED ||
                                        command->working_counter != 1)) {
        EC_ERR("FSM failed to set FMMUs on slave %i.\n",
               fsm->slave->ring_position);
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
   Slave state: Set slave state to OP.
*/

void ec_fsm_slave_op(ec_fsm_t *fsm)
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
   Slave state: Set slave state to OP.
*/

void ec_fsm_slave_op2(ec_fsm_t *fsm)
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
   Slave state: End.
   End state of the slave state machine.
*/

void ec_fsm_slave_end(ec_fsm_t *fsm)
{
}

/******************************************************************************
 *  SII state machine
 *****************************************************************************/

/**
   Slave SII state: Start reading.
   Starts reading the slave information interface.
*/

void ec_fsm_sii_start_reading(ec_fsm_t *fsm)
{
    ec_command_t *command = &fsm->command;

    // initiate read operation
    ec_command_npwr(command, fsm->slave->station_address, 0x502, 6);
    EC_WRITE_U8 (command->data,     0x00); // read-only access
    EC_WRITE_U8 (command->data + 1, 0x01); // request read operation
    EC_WRITE_U32(command->data + 2, fsm->sii_offset);
    ec_master_queue_command(fsm->master, command);
    fsm->sii_state = ec_fsm_sii_check;
}

/*****************************************************************************/

/**
   Slave SII state: Check.
   Checks, if the SII-read-command has been sent and issues a fetch command.
*/

void ec_fsm_sii_check(ec_fsm_t *fsm)
{
    ec_command_t *command = &fsm->command;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("FSM SII: Reception of check command failed.\n");
        fsm->sii_state = ec_fsm_sii_error;
        return;
    }

    ec_command_nprd(command, fsm->slave->station_address, 0x502, 10);
    ec_master_queue_command(fsm->master, command);
    fsm->sii_state = ec_fsm_sii_fetch;
}

/*****************************************************************************/

/**
   Slave SII state: Fetch.
   Fetches the result of an SII-read command.
*/

void ec_fsm_sii_fetch(ec_fsm_t *fsm)
{
    ec_command_t *command = &fsm->command;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("FSM SII: Reception of fetch command failed.\n");
        fsm->sii_state = ec_fsm_sii_error;
        return;
    }

    // check "busy bit"
    if (likely((EC_READ_U8(command->data + 1) & 0x81) == 0)) {
        fsm->sii_result = EC_READ_U32(command->data + 6);
        fsm->sii_state = ec_fsm_sii_finished;
    }
}

/*****************************************************************************/

/**
   Slave SII state: Finished.
   End state of the slave SII state machine.
*/

void ec_fsm_sii_finished(ec_fsm_t *fsm)
{
}

/*****************************************************************************/

/**
   Slave SII state: Error.
   End state of the slave SII state machine.
*/

void ec_fsm_sii_error(ec_fsm_t *fsm)
{
}

/******************************************************************************
 *  state change state machine
 *****************************************************************************/

/**
   State change state: Start.
*/

void ec_fsm_change_start(ec_fsm_t *fsm)
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
   State change state: Check.
*/

void ec_fsm_change_check(ec_fsm_t *fsm)
{
    ec_command_t *command = &fsm->command;
    ec_slave_t *slave = fsm->slave;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("FSM: Reception of state command failed.\n");
        slave->state_error = 1;
        fsm->change_state = ec_fsm_change_error;
        return;
    }

    //start = get_cycles();
    //timeout = (cycles_t) 10 * cpu_khz; // 10ms

    // read AL status from slave
    ec_command_nprd(command, slave->station_address, 0x0130, 2);
    ec_master_queue_command(fsm->master, command);
    fsm->change_state = ec_fsm_change_status;
}

/*****************************************************************************/

/**
   State change state: Status.
*/

void ec_fsm_change_status(ec_fsm_t *fsm)
{
    ec_command_t *command = &fsm->command;
    ec_slave_t *slave = fsm->slave;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("FSM: Reception of state check command failed.\n");
        slave->state_error = 1;
        fsm->change_state = ec_fsm_change_error;
        return;
    }

    slave->current_state = EC_READ_U8(command->data);

    if (slave->current_state & 0x10) { // state change error
        EC_ERR("Failed to set state 0x%02X - Slave %i refused state change"
               " (code 0x%02X)!\n", fsm->change_new, slave->ring_position,
               slave->current_state);

        fsm->change_new = slave->current_state & 0x0F;

        // fetch AL status error code
        ec_command_nprd(command, slave->station_address, 0x0134, 2);
        ec_master_queue_command(fsm->master, command);
        fsm->change_state = ec_fsm_change_code;
        return;
    }

    if (slave->current_state == fsm->change_new) {
        fsm->change_state = ec_fsm_change_end;
        return;
    }

    EC_ERR("Failed to check state 0x%02X of slave %i - Timeout!\n",
           fsm->change_new, slave->ring_position);
    slave->state_error = 1;
    fsm->change_state = ec_fsm_change_error;
}

/*****************************************************************************/

/**
   State change state: Code.
*/

void ec_fsm_change_code(ec_fsm_t *fsm)
{
    ec_command_t *command = &fsm->command;
    ec_slave_t *slave = fsm->slave;
    uint32_t code;
    const ec_code_msg_t *al_msg;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("FSM: Reception of AL status code command failed.\n");
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
   State change state: Acknowledge.
*/

void ec_fsm_change_ack(ec_fsm_t *fsm)
{
    ec_command_t *command = &fsm->command;
    ec_slave_t *slave = fsm->slave;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("FSM: Reception of state ack command failed.\n");
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
   State change state: Acknowledge 2.
*/

void ec_fsm_change_ack2(ec_fsm_t *fsm)
{
    ec_command_t *command = &fsm->command;
    ec_slave_t *slave = fsm->slave;

    if (command->state != EC_CMD_RECEIVED || command->working_counter != 1) {
        EC_ERR("FSM: Reception of state ack check command failed.\n");
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
   State change state: End.
*/

void ec_fsm_change_end(ec_fsm_t *fsm)
{
}

/*****************************************************************************/

/**
   State change state: Error.
*/

void ec_fsm_change_error(ec_fsm_t *fsm)
{
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
