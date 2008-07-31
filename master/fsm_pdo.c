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
 * EtherCAT Pdo configuration state machine.
 */

/*****************************************************************************/

#include "globals.h"
#include "master.h"
#include "mailbox.h"
#include "slave_config.h"

#include "fsm_pdo.h"

/*****************************************************************************/

void ec_fsm_pdo_read_state_start(ec_fsm_pdo_t *);
void ec_fsm_pdo_read_state_pdo_count(ec_fsm_pdo_t *);
void ec_fsm_pdo_read_state_pdo(ec_fsm_pdo_t *);
void ec_fsm_pdo_read_state_pdo_entries(ec_fsm_pdo_t *);

void ec_fsm_pdo_read_action_next_sync(ec_fsm_pdo_t *);
void ec_fsm_pdo_read_action_next_pdo(ec_fsm_pdo_t *);

void ec_fsm_pdo_conf_state_start(ec_fsm_pdo_t *);
void ec_fsm_pdo_conf_state_read_mapping(ec_fsm_pdo_t *);
void ec_fsm_pdo_conf_state_mapping(ec_fsm_pdo_t *);
void ec_fsm_pdo_conf_state_zero_pdo_count(ec_fsm_pdo_t *);
void ec_fsm_pdo_conf_state_assign_pdo(ec_fsm_pdo_t *);
void ec_fsm_pdo_conf_state_set_pdo_count(ec_fsm_pdo_t *);
void ec_fsm_pdo_conf_state_entries(ec_fsm_pdo_t *);

void ec_fsm_pdo_conf_action_next_sync(ec_fsm_pdo_t *);
void ec_fsm_pdo_conf_action_pdo_mapping(ec_fsm_pdo_t *);
void ec_fsm_pdo_conf_action_check_mapping(ec_fsm_pdo_t *);
void ec_fsm_pdo_conf_action_next_pdo_mapping(ec_fsm_pdo_t *);
void ec_fsm_pdo_conf_action_check_assignment(ec_fsm_pdo_t *);
void ec_fsm_pdo_conf_action_assign_pdo(ec_fsm_pdo_t *);

void ec_fsm_pdo_state_end(ec_fsm_pdo_t *);
void ec_fsm_pdo_state_error(ec_fsm_pdo_t *);

/*****************************************************************************/

/** Constructor.
 */
void ec_fsm_pdo_init(
        ec_fsm_pdo_t *fsm, /**< Pdo configuration state machine. */
        ec_fsm_coe_t *fsm_coe /**< CoE state machine to use */
        )
{
    fsm->fsm_coe = fsm_coe;
    ec_fsm_pdo_entry_init(&fsm->fsm_pdo_entry, fsm_coe);
    ec_pdo_list_init(&fsm->pdos);
    ec_sdo_request_init(&fsm->request);
    ec_pdo_init(&fsm->slave_pdo);
}

/*****************************************************************************/

/** Destructor.
 */
void ec_fsm_pdo_clear(
        ec_fsm_pdo_t *fsm /**< Pdo configuration state machine. */
        )
{
    ec_fsm_pdo_entry_clear(&fsm->fsm_pdo_entry);
    ec_pdo_list_clear(&fsm->pdos);
    ec_sdo_request_clear(&fsm->request);
    ec_pdo_clear(&fsm->slave_pdo);
}

/*****************************************************************************/

/** Start reading the Pdo configuration.
 */
void ec_fsm_pdo_start_reading(
        ec_fsm_pdo_t *fsm, /**< Pdo configuration state machine. */
        ec_slave_t *slave /**< slave to configure */
        )
{
    fsm->slave = slave;
    fsm->state = ec_fsm_pdo_read_state_start;
}

/*****************************************************************************/

/** Start writing the Pdo configuration.
 */
void ec_fsm_pdo_start_configuration(
        ec_fsm_pdo_t *fsm, /**< Pdo configuration state machine. */
        ec_slave_t *slave /**< slave to configure */
        )
{
    fsm->slave = slave;
    fsm->state = ec_fsm_pdo_conf_state_start;
}

/*****************************************************************************/

/** Get running state.
 *
 * \return false, if state machine has terminated
 */
int ec_fsm_pdo_running(
        const ec_fsm_pdo_t *fsm /**< Pdo configuration state machine. */
        )
{
    return fsm->state != ec_fsm_pdo_state_end
        && fsm->state != ec_fsm_pdo_state_error;
}

/*****************************************************************************/

/** Executes the current state of the state machine.
 *
 * If the state machine's datagram is not sent or received yet, the execution
 * of the state machine is delayed to the next cycle.
 *
 * \return false, if state machine has terminated
 */
int ec_fsm_pdo_exec(
        ec_fsm_pdo_t *fsm /**< Pdo configuration state machine. */
        )
{
    fsm->state(fsm);
    return ec_fsm_pdo_running(fsm);
}

/*****************************************************************************/

/** Get execution result.
 *
 * \return true, if the state machine terminated gracefully
 */
int ec_fsm_pdo_success(
        const ec_fsm_pdo_t *fsm /**< Pdo configuration state machine. */
        )
{
    return fsm->state == ec_fsm_pdo_state_end;
}

/******************************************************************************
 * Reading state funtions.
 *****************************************************************************/

/** Start reading Pdo assignment.
 */
void ec_fsm_pdo_read_state_start(
        ec_fsm_pdo_t *fsm /**< finite state machine */
        )
{
    // read Pdo assignment for first sync manager not reserved for mailbox
    fsm->sync_index = 1; // next is 2
    ec_fsm_pdo_read_action_next_sync(fsm);
}

/*****************************************************************************/

/** Read Pdo assignment of next sync manager.
 */
void ec_fsm_pdo_read_action_next_sync(
        ec_fsm_pdo_t *fsm /**< Finite state machine */
        )
{
    ec_slave_t *slave = fsm->slave;

    fsm->sync_index++;

    for (; fsm->sync_index < EC_MAX_SYNC_MANAGERS; fsm->sync_index++) {
        if (!(fsm->sync = ec_slave_get_sync(slave, fsm->sync_index)))
            continue;

        if (slave->master->debug_level)
            EC_DBG("Reading Pdo assignment of SM%u.\n", fsm->sync_index);

        ec_pdo_list_clear_pdos(&fsm->pdos);

        ec_sdo_request_address(&fsm->request, 0x1C10 + fsm->sync_index, 0);
        ecrt_sdo_request_read(&fsm->request);
        fsm->state = ec_fsm_pdo_read_state_pdo_count;
        ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request);
        ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
        return;
    }

    if (slave->master->debug_level)
        EC_DBG("Reading of Pdo configuration finished.\n");

    fsm->state = ec_fsm_pdo_state_end;
}

/*****************************************************************************/

/** Count assigned Pdos.
 */
void ec_fsm_pdo_read_state_pdo_count(
        ec_fsm_pdo_t *fsm /**< finite state machine */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("Failed to read number of assigned Pdos for SM%u.\n",
                fsm->sync_index);
        fsm->state = ec_fsm_pdo_state_error;
        return;
    }

    if (fsm->request.data_size != sizeof(uint8_t)) {
        EC_ERR("Invalid data size %u returned when uploading Sdo 0x%04X:%02X "
                "from slave %u.\n", fsm->request.data_size,
                fsm->request.index, fsm->request.subindex,
                fsm->slave->ring_position);
        fsm->state = ec_fsm_pdo_state_error;
        return;
    }
    fsm->pdo_count = EC_READ_U8(fsm->request.data);

    if (fsm->slave->master->debug_level)
        EC_DBG("%u Pdos assigned.\n", fsm->pdo_count);

    // read first Pdo
    fsm->pdo_pos = 1;
    ec_fsm_pdo_read_action_next_pdo(fsm);
}

/*****************************************************************************/

/** Read next Pdo.
 */
void ec_fsm_pdo_read_action_next_pdo(
        ec_fsm_pdo_t *fsm /**< finite state machine */
        )
{
    if (fsm->pdo_pos <= fsm->pdo_count) {
        ec_sdo_request_address(&fsm->request, 0x1C10 + fsm->sync_index,
                fsm->pdo_pos);
        ecrt_sdo_request_read(&fsm->request);
        fsm->state = ec_fsm_pdo_read_state_pdo;
        ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request);
        ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
        return;
    }

    // finished reading Pdo configuration
    
    if (ec_pdo_list_copy(&fsm->sync->pdos, &fsm->pdos)) {
        fsm->state = ec_fsm_pdo_state_error;
        return;
    }

    fsm->sync->assign_source = EC_ASSIGN_COE;
    ec_pdo_list_clear_pdos(&fsm->pdos);

    // next sync manager
    ec_fsm_pdo_read_action_next_sync(fsm);
}

/*****************************************************************************/

/** Fetch Pdo information.
 */
void ec_fsm_pdo_read_state_pdo(
        ec_fsm_pdo_t *fsm /**< finite state machine */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("Failed to read index of assigned Pdo %u from SM%u.\n",
                fsm->pdo_pos, fsm->sync_index);
        fsm->state = ec_fsm_pdo_state_error;
        return;
    }

    if (fsm->request.data_size != sizeof(uint16_t)) {
        EC_ERR("Invalid data size %u returned when uploading Sdo 0x%04X:%02X "
                "from slave %u.\n", fsm->request.data_size,
                fsm->request.index, fsm->request.subindex,
                fsm->slave->ring_position);
        fsm->state = ec_fsm_pdo_state_error;
        return;
    }

    if (!(fsm->pdo = (ec_pdo_t *)
                kmalloc(sizeof(ec_pdo_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate Pdo.\n");
        fsm->state = ec_fsm_pdo_state_error;
        return;
    }

    ec_pdo_init(fsm->pdo);
    fsm->pdo->index = EC_READ_U16(fsm->request.data);
    fsm->pdo->sync_index = fsm->sync_index;

    if (fsm->slave->master->debug_level)
        EC_DBG("Pdo 0x%04X.\n", fsm->pdo->index);

    list_add_tail(&fsm->pdo->list, &fsm->pdos.list);

    fsm->state = ec_fsm_pdo_read_state_pdo_entries;
    ec_fsm_pdo_entry_start_reading(&fsm->fsm_pdo_entry, fsm->slave, fsm->pdo);
    fsm->state(fsm); // execute immediately
}

/*****************************************************************************/

/** Fetch Pdo information.
 */
void ec_fsm_pdo_read_state_pdo_entries(
        ec_fsm_pdo_t *fsm /**< finite state machine */
        )
{
    if (ec_fsm_pdo_entry_exec(&fsm->fsm_pdo_entry))
        return;

    if (!ec_fsm_pdo_entry_success(&fsm->fsm_pdo_entry)) {
        EC_ERR("Failed to read mapped Pdo entries for Pdo 0x%04X.\n",
                fsm->pdo->index);
        fsm->state = ec_fsm_pdo_state_error;
        return;
    }

    // next Pdo
    fsm->pdo_pos++;
    ec_fsm_pdo_read_action_next_pdo(fsm);
}

/******************************************************************************
 * Writing state functions.
 *****************************************************************************/

/** Start Pdo configuration.
 */
void ec_fsm_pdo_conf_state_start(
        ec_fsm_pdo_t *fsm /**< Pdo configuration state machine. */
        )
{
    if (!fsm->slave->config) {
        fsm->state = ec_fsm_pdo_state_end;
        return;
    }

    fsm->sync_index = 0xff; // next is zero
    ec_fsm_pdo_conf_action_next_sync(fsm);
}

/*****************************************************************************/

/** Assign next Pdo.
 */
ec_pdo_t *ec_fsm_pdo_conf_action_next_pdo(
        const ec_fsm_pdo_t *fsm, /**< Pdo configuration state machine. */
        const struct list_head *list /**< current Pdo list item */
        )
{
    list = list->next; 
    if (list == &fsm->pdos.list)
        return NULL; // no next Pdo
    return list_entry(list, ec_pdo_t, list);
}

/*****************************************************************************/

/** Get the next sync manager for a pdo configuration.
 */
void ec_fsm_pdo_conf_action_next_sync(
        ec_fsm_pdo_t *fsm /**< Pdo configuration state machine. */
        )
{
    fsm->sync_index++;

    for (; fsm->sync_index < EC_MAX_SYNC_MANAGERS; fsm->sync_index++) {
        if (ec_pdo_list_copy(&fsm->pdos,
                    &fsm->slave->config->sync_configs[fsm->sync_index].pdos)) {
            fsm->state = ec_fsm_pdo_state_error;
            return;
        }
        
        if (!(fsm->sync = ec_slave_get_sync(fsm->slave, fsm->sync_index))) {
            if (!list_empty(&fsm->pdos.list))
                EC_WARN("Pdos configured for SM%u, but slave %u does not "
                        "provide a sync manager configuration!\n",
                        fsm->sync_index, fsm->slave->ring_position);
            continue;
        }

        // get first configured Pdo
        if (!(fsm->pdo = ec_fsm_pdo_conf_action_next_pdo(fsm, &fsm->pdos.list))) {
            // no pdos configured
            ec_fsm_pdo_conf_action_check_assignment(fsm);
            return;
        }

        ec_fsm_pdo_conf_action_pdo_mapping(fsm);
        return;
    }

    fsm->state = ec_fsm_pdo_state_end;
}

/*****************************************************************************/

/**
 */
void ec_fsm_pdo_conf_action_pdo_mapping(
        ec_fsm_pdo_t *fsm /**< Pdo configuration state machine. */
        )
{
    const ec_pdo_t *assigned_pdo;

    fsm->slave_pdo.index = fsm->pdo->index;

    if ((assigned_pdo = ec_slave_find_pdo(fsm->slave, fsm->pdo->index))) {
        ec_pdo_copy_entries(&fsm->slave_pdo, assigned_pdo);
    } else { // configured Pdo is not assigned and thus unknown
        ec_pdo_clear_entries(&fsm->slave_pdo);
    }

    if (list_empty(&fsm->slave_pdo.entries)) {

        if (fsm->slave->master->debug_level)
            EC_DBG("Reading mapping of Pdo 0x%04X.\n",
                    fsm->pdo->index);
            
        // pdo mapping is unknown; start loading it
        ec_fsm_pdo_entry_start_reading(&fsm->fsm_pdo_entry, fsm->slave,
                &fsm->slave_pdo);
        fsm->state = ec_fsm_pdo_conf_state_read_mapping;
        fsm->state(fsm); // execute immediately
        return;
    }

    // pdo mapping is known, check if it most be re-configured
    ec_fsm_pdo_conf_action_check_mapping(fsm);
}

/*****************************************************************************/

/**
 */
void ec_fsm_pdo_conf_state_read_mapping(
        ec_fsm_pdo_t *fsm /**< Pdo configuration state machine. */
        )
{
    if (ec_fsm_pdo_entry_exec(&fsm->fsm_pdo_entry))
        return;

    if (!ec_fsm_pdo_entry_success(&fsm->fsm_pdo_entry))
        EC_WARN("Failed to read mapped Pdo entries for Pdo 0x%04X.\n",
                fsm->pdo->index);

    // check if the mapping must be re-configured
    ec_fsm_pdo_conf_action_check_mapping(fsm);
}

/*****************************************************************************/

/**
 */
void ec_fsm_pdo_conf_action_check_mapping(
        ec_fsm_pdo_t *fsm /**< Pdo configuration state machine. */
        )
{
    if (ec_pdo_equal_entries(fsm->pdo, &fsm->slave_pdo)) {
        if (fsm->slave->master->debug_level)
            EC_DBG("Mapping of Pdo 0x%04X is already configured correctly.\n",
                    fsm->pdo->index);
        ec_fsm_pdo_conf_action_next_pdo_mapping(fsm);
        return;
    }

    if (fsm->slave->master->debug_level) {
        // TODO display diff
        EC_DBG("Changing mapping of Pdo 0x%04X.\n", fsm->pdo->index);
    }

    ec_fsm_pdo_entry_start_configuration(&fsm->fsm_pdo_entry, fsm->slave,
            fsm->pdo);
    fsm->state = ec_fsm_pdo_conf_state_mapping;
    fsm->state(fsm); // execure immediately
}

/*****************************************************************************/

/**
 */
void ec_fsm_pdo_conf_state_mapping(
        ec_fsm_pdo_t *fsm /**< Pdo configuration state machine. */
        )
{
    if (ec_fsm_pdo_entry_exec(&fsm->fsm_pdo_entry))
        return;

    if (!ec_fsm_pdo_entry_success(&fsm->fsm_pdo_entry))
        EC_WARN("Failed to configure mapping of Pdo 0x%04X.\n",
                fsm->pdo->index);

    ec_fsm_pdo_conf_action_next_pdo_mapping(fsm);
}

/*****************************************************************************/

/**
 */
void ec_fsm_pdo_conf_action_next_pdo_mapping(
        ec_fsm_pdo_t *fsm /**< Pdo configuration state machine. */
        )
{
    // get next configured Pdo
    if (!(fsm->pdo = ec_fsm_pdo_conf_action_next_pdo(fsm, &fsm->pdo->list))) {
        // no more configured pdos
        ec_fsm_pdo_conf_action_check_assignment(fsm);
        return;
    }

    ec_fsm_pdo_conf_action_pdo_mapping(fsm);
}

/*****************************************************************************/

/**
 */
void ec_fsm_pdo_conf_action_check_assignment(
        ec_fsm_pdo_t *fsm /**< Pdo configuration state machine. */
        )
{
    // check if assignment has to be re-configured
    if (ec_pdo_list_equal(&fsm->sync->pdos, &fsm->pdos)) {

        if (fsm->slave->master->debug_level)
            EC_DBG("Pdo assignment for SM%u is already configured "
                    "correctly.\n", fsm->sync_index);

        ec_fsm_pdo_conf_action_next_sync(fsm);
        return;
    }

    if (fsm->slave->master->debug_level) {
        EC_DBG("Pdo assignment of SM%u differs:\n", fsm->sync_index);
        EC_DBG("Currently assigned Pdos: ");
        ec_pdo_list_print(&fsm->sync->pdos);
        printk("\n");
        EC_DBG("Pdos to assign: ");
        ec_pdo_list_print(&fsm->pdos);
        printk("\n");
    }

    // Pdo assignment has to be changed. Does the slave support this?
    if (!(fsm->slave->sii.mailbox_protocols & EC_MBOX_COE)
            || (fsm->slave->sii.has_general
                && !fsm->slave->sii.coe_details.enable_pdo_assign)) {
        EC_WARN("Slave %u does not support assigning Pdos!\n",
                fsm->slave->ring_position);
        ec_fsm_pdo_conf_action_next_sync(fsm);
        return;
    }

    if (ec_sdo_request_alloc(&fsm->request, 2)) {
        fsm->state = ec_fsm_pdo_state_error;
        return;
    }

    // set mapped Pdo count to zero
    EC_WRITE_U8(fsm->request.data, 0); // zero Pdos mapped
    fsm->request.data_size = 1;
    ec_sdo_request_address(&fsm->request, 0x1C10 + fsm->sync_index, 0);
    ecrt_sdo_request_write(&fsm->request);

    if (fsm->slave->master->debug_level)
        EC_DBG("Setting number of assigned Pdos to zero.\n");

    fsm->state = ec_fsm_pdo_conf_state_zero_pdo_count;
    ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request);
    ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
}

/*****************************************************************************/

/** Set the number of assigned Pdos to zero.
 */
void ec_fsm_pdo_conf_state_zero_pdo_count(
        ec_fsm_pdo_t *fsm /**< Pdo configuration state machine. */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe))
        return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_WARN("Failed to clear Pdo assignment of SM%u.\n", fsm->sync_index);
        fsm->state = ec_fsm_pdo_state_error;
        return;
    }

    // the sync manager's assigned Pdos have been cleared
    ec_pdo_list_clear_pdos(&fsm->sync->pdos);

    // assign all Pdos belonging to the current sync manager
    
    // find first Pdo
    if (!(fsm->pdo = ec_fsm_pdo_conf_action_next_pdo(fsm, &fsm->pdos.list))) {

        if (fsm->slave->master->debug_level)
            EC_DBG("No Pdos to assign.\n");

        // check for mapping to be altered
        ec_fsm_pdo_conf_action_next_sync(fsm);
        return;
    }

    // assign first Pdo
    fsm->pdo_pos = 1;
	ec_fsm_pdo_conf_action_assign_pdo(fsm);
}

/*****************************************************************************/

/** Assign a Pdo.
 */
void ec_fsm_pdo_conf_action_assign_pdo(
        ec_fsm_pdo_t *fsm /**< Pdo configuration state machine. */
        )
{
    EC_WRITE_U16(fsm->request.data, fsm->pdo->index);
    fsm->request.data_size = 2;
    ec_sdo_request_address(&fsm->request,
            0x1C10 + fsm->sync_index, fsm->pdo_pos);
    ecrt_sdo_request_write(&fsm->request);

    if (fsm->slave->master->debug_level)
        EC_DBG("Assigning Pdo 0x%04X at position %u.\n",
                fsm->pdo->index, fsm->pdo_pos);
    
    fsm->state = ec_fsm_pdo_conf_state_assign_pdo;
    ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request);
    ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
}

/*****************************************************************************/

/** Add a Pdo to the sync managers Pdo assignment.
 */
void ec_fsm_pdo_conf_state_assign_pdo(
        ec_fsm_pdo_t *fsm /**< Pdo configuration state machine. */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_WARN("Failed to assign Pdo 0x%04X at position %u of SM%u.\n",
                fsm->pdo->index, fsm->pdo_pos, fsm->sync_index);
        fsm->state = ec_fsm_pdo_state_error;
        return;
    }

    // find next Pdo
    if (!(fsm->pdo = ec_fsm_pdo_conf_action_next_pdo(fsm, &fsm->pdo->list))) {

        // no more Pdos to assign, set Pdo count
        EC_WRITE_U8(fsm->request.data, fsm->pdo_pos);
        fsm->request.data_size = 1;
        ec_sdo_request_address(&fsm->request, 0x1C10 + fsm->sync_index, 0);
        ecrt_sdo_request_write(&fsm->request);

        if (fsm->slave->master->debug_level)
            EC_DBG("Setting number of assigned Pdos to %u.\n", fsm->pdo_pos);
        
        fsm->state = ec_fsm_pdo_conf_state_set_pdo_count;
        ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request);
        ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
        return;
    }

    // add next Pdo to assignment
    fsm->pdo_pos++;
    ec_fsm_pdo_conf_action_assign_pdo(fsm);
}
    
/*****************************************************************************/

/** Set the number of assigned Pdos.
 */
void ec_fsm_pdo_conf_state_set_pdo_count(
        ec_fsm_pdo_t *fsm /**< Pdo configuration state machine. */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_WARN("Failed to set number of assigned Pdos of SM%u.\n",
                fsm->sync_index);
        fsm->state = ec_fsm_pdo_state_error;
        return;
    }

    // Pdos have been configured
    ec_pdo_list_copy(&fsm->sync->pdos, &fsm->pdos);

    if (fsm->slave->master->debug_level)
        EC_DBG("Successfully configured Pdo assignment of SM%u.\n",
                fsm->sync_index);

    // check if Pdo mapping has to be altered
    ec_fsm_pdo_conf_action_next_sync(fsm);
}

/******************************************************************************
 * Common state functions
 *****************************************************************************/

/** State: ERROR.
 */
void ec_fsm_pdo_state_error(
        ec_fsm_pdo_t *fsm /**< Pdo configuration state machine. */
        )
{
}

/*****************************************************************************/

/** State: END.
 */
void ec_fsm_pdo_state_end(
        ec_fsm_pdo_t *fsm /**< Pdo configuration state machine. */
        )
{
}

/*****************************************************************************/
