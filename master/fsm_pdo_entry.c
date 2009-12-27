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

/** \file
 * EtherCAT PDO mapping state machine.
 */

/*****************************************************************************/

#include "globals.h"
#include "master.h"
#include "mailbox.h"
#include "slave_config.h"

#include "fsm_pdo_entry.h"

/*****************************************************************************/

void ec_fsm_pdo_entry_read_state_start(ec_fsm_pdo_entry_t *);
void ec_fsm_pdo_entry_read_state_count(ec_fsm_pdo_entry_t *);
void ec_fsm_pdo_entry_read_state_entry(ec_fsm_pdo_entry_t *);

void ec_fsm_pdo_entry_read_action_next(ec_fsm_pdo_entry_t *);

void ec_fsm_pdo_entry_conf_state_start(ec_fsm_pdo_entry_t *);
void ec_fsm_pdo_entry_conf_state_zero_entry_count(ec_fsm_pdo_entry_t *);
void ec_fsm_pdo_entry_conf_state_map_entry(ec_fsm_pdo_entry_t *);
void ec_fsm_pdo_entry_conf_state_set_entry_count(ec_fsm_pdo_entry_t *);

void ec_fsm_pdo_entry_conf_action_map(ec_fsm_pdo_entry_t *);

void ec_fsm_pdo_entry_state_end(ec_fsm_pdo_entry_t *);
void ec_fsm_pdo_entry_state_error(ec_fsm_pdo_entry_t *);

/*****************************************************************************/

/** Constructor.
 */
void ec_fsm_pdo_entry_init(
        ec_fsm_pdo_entry_t *fsm, /**< PDO mapping state machine. */
        ec_fsm_coe_t *fsm_coe /**< CoE state machine to use. */
        )
{
    fsm->fsm_coe = fsm_coe;
    ec_sdo_request_init(&fsm->request);
}

/*****************************************************************************/

/** Destructor.
 */
void ec_fsm_pdo_entry_clear(
        ec_fsm_pdo_entry_t *fsm /**< PDO mapping state machine. */
        )
{
    ec_sdo_request_clear(&fsm->request);
}

/*****************************************************************************/

/** Start reading a PDO's entries.
 */
void ec_fsm_pdo_entry_start_reading(
        ec_fsm_pdo_entry_t *fsm, /**< PDO mapping state machine. */
        ec_slave_t *slave, /**< slave to configure */
        ec_pdo_t *pdo /**< PDO to read entries for. */
        )
{
    fsm->slave = slave;
    fsm->target_pdo = pdo;

    ec_pdo_clear_entries(fsm->target_pdo);
    
    fsm->state = ec_fsm_pdo_entry_read_state_start;
}

/*****************************************************************************/

/** Start PDO mapping state machine.
 */
void ec_fsm_pdo_entry_start_configuration(
        ec_fsm_pdo_entry_t *fsm, /**< PDO mapping state machine. */
        ec_slave_t *slave, /**< slave to configure */
        const ec_pdo_t *pdo /**< PDO with the desired entries. */
        )
{
    fsm->slave = slave;
    fsm->source_pdo = pdo;

    fsm->state = ec_fsm_pdo_entry_conf_state_start;
}

/*****************************************************************************/

/** Get running state.
 *
 * \return false, if state machine has terminated
 */
int ec_fsm_pdo_entry_running(
        const ec_fsm_pdo_entry_t *fsm /**< PDO mapping state machine. */
        )
{
    return fsm->state != ec_fsm_pdo_entry_state_end
        && fsm->state != ec_fsm_pdo_entry_state_error;
}

/*****************************************************************************/

/** Executes the current state.
 *
 * \return false, if state machine has terminated
 */
int ec_fsm_pdo_entry_exec(
        ec_fsm_pdo_entry_t *fsm /**< PDO mapping state machine. */
        )
{
    fsm->state(fsm);
    return ec_fsm_pdo_entry_running(fsm);
}

/*****************************************************************************/

/** Get execution result.
 *
 * \return true, if the state machine terminated gracefully
 */
int ec_fsm_pdo_entry_success(
        const ec_fsm_pdo_entry_t *fsm /**< PDO mapping state machine. */
        )
{
    return fsm->state == ec_fsm_pdo_entry_state_end;
}

/******************************************************************************
 * Reading state functions.
 *****************************************************************************/

/** Request reading the number of mapped PDO entries.
 */
void ec_fsm_pdo_entry_read_state_start(
        ec_fsm_pdo_entry_t *fsm /**< PDO mapping state machine. */
        )
{
    ec_sdo_request_address(&fsm->request, fsm->target_pdo->index, 0);
    ecrt_sdo_request_read(&fsm->request);

    fsm->state = ec_fsm_pdo_entry_read_state_count;
    ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request);
    ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
}

/*****************************************************************************/

/** Read number of mapped PDO entries.
 */
void ec_fsm_pdo_entry_read_state_count(
        ec_fsm_pdo_entry_t *fsm /**< finite state machine */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe))
        return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("Failed to read number of mapped PDO entries.\n");
        fsm->state = ec_fsm_pdo_entry_state_error;
        return;
    }

    if (fsm->request.data_size != sizeof(uint8_t)) {
        EC_ERR("Invalid data size %zu at uploading SDO 0x%04X:%02X.\n",
                fsm->request.data_size, fsm->request.index,
                fsm->request.subindex);
        fsm->state = ec_fsm_pdo_entry_state_error;
        return;
    }

    fsm->entry_count = EC_READ_U8(fsm->request.data);

    if (fsm->slave->master->debug_level)
        EC_DBG("%u PDO entries mapped.\n", fsm->entry_count);

    // read first PDO entry
    fsm->entry_pos = 1;
    ec_fsm_pdo_entry_read_action_next(fsm);
}

/*****************************************************************************/

/** Read next PDO entry.
 */
void ec_fsm_pdo_entry_read_action_next(
        ec_fsm_pdo_entry_t *fsm /**< finite state machine */
        )
{
    if (fsm->entry_pos <= fsm->entry_count) {
        ec_sdo_request_address(&fsm->request, fsm->target_pdo->index, fsm->entry_pos);
        ecrt_sdo_request_read(&fsm->request);
        fsm->state = ec_fsm_pdo_entry_read_state_entry;
        ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request);
        ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
        return;
    }

    // finished reading entries.
    fsm->state = ec_fsm_pdo_entry_state_end;
}

/*****************************************************************************/

/** Read PDO entry information.
 */
void ec_fsm_pdo_entry_read_state_entry(
        ec_fsm_pdo_entry_t *fsm /**< finite state machine */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("Failed to read mapped PDO entry.\n");
        fsm->state = ec_fsm_pdo_entry_state_error;
        return;
    }

    if (fsm->request.data_size != sizeof(uint32_t)) {
        EC_ERR("Invalid data size %zu at uploading SDO 0x%04X:%02X.\n",
                fsm->request.data_size, fsm->request.index,
                fsm->request.subindex);
        fsm->state = ec_fsm_pdo_entry_state_error;
    } else {
        uint32_t pdo_entry_info;
        ec_pdo_entry_t *pdo_entry;

        pdo_entry_info = EC_READ_U32(fsm->request.data);

        if (!(pdo_entry = (ec_pdo_entry_t *)
                    kmalloc(sizeof(ec_pdo_entry_t), GFP_KERNEL))) {
            EC_ERR("Failed to allocate PDO entry.\n");
            fsm->state = ec_fsm_pdo_entry_state_error;
            return;
        }

        ec_pdo_entry_init(pdo_entry);
        pdo_entry->index = pdo_entry_info >> 16;
        pdo_entry->subindex = (pdo_entry_info >> 8) & 0xFF;
        pdo_entry->bit_length = pdo_entry_info & 0xFF;

        if (!pdo_entry->index && !pdo_entry->subindex) {
            if (ec_pdo_entry_set_name(pdo_entry, "Gap")) {
                ec_pdo_entry_clear(pdo_entry);
                kfree(pdo_entry);
                fsm->state = ec_fsm_pdo_entry_state_error;
                return;
            }
        }

        if (fsm->slave->master->debug_level) {
            EC_DBG("PDO entry 0x%04X:%02X, %u bit, \"%s\".\n",
                    pdo_entry->index, pdo_entry->subindex,
                    pdo_entry->bit_length,
                    pdo_entry->name ? pdo_entry->name : "???");
        }

        list_add_tail(&pdo_entry->list, &fsm->target_pdo->entries);

        // next PDO entry
        fsm->entry_pos++;
        ec_fsm_pdo_entry_read_action_next(fsm);
    }
}

/******************************************************************************
 * Configuration state functions.
 *****************************************************************************/

/** Start PDO mapping.
 */
void ec_fsm_pdo_entry_conf_state_start(
        ec_fsm_pdo_entry_t *fsm /**< PDO mapping state machine. */
        )
{
    // PDO mapping has to be changed. Does the slave support this?
    if (!(fsm->slave->sii.mailbox_protocols & EC_MBOX_COE)
            || (fsm->slave->sii.has_general
                && !fsm->slave->sii.coe_details.enable_pdo_configuration)) {
        EC_WARN("Slave %u does not support changing the PDO mapping!\n",
                fsm->slave->ring_position);
        fsm->state = ec_fsm_pdo_entry_state_error;
        return;
    }

    if (ec_sdo_request_alloc(&fsm->request, 4)) {
        fsm->state = ec_fsm_pdo_entry_state_error;
        return;
    }

    // set mapped PDO entry count to zero
    EC_WRITE_U8(fsm->request.data, 0);
    fsm->request.data_size = 1;
    ec_sdo_request_address(&fsm->request, fsm->source_pdo->index, 0);
    ecrt_sdo_request_write(&fsm->request);

    if (fsm->slave->master->debug_level)
        EC_DBG("Setting entry count to zero.\n");

    fsm->state = ec_fsm_pdo_entry_conf_state_zero_entry_count;
    ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request);
    ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
}

/*****************************************************************************/

/** Process next PDO entry.
 */
ec_pdo_entry_t *ec_fsm_pdo_entry_conf_next_entry(
        const ec_fsm_pdo_entry_t *fsm, /**< PDO mapping state machine. */
        const struct list_head *list /**< current entry list item */
        )
{
    list = list->next; 
    if (list == &fsm->source_pdo->entries)
        return NULL; // no next entry
    return list_entry(list, ec_pdo_entry_t, list);
}

/*****************************************************************************/

/** Set the number of mapped entries to zero.
 */
void ec_fsm_pdo_entry_conf_state_zero_entry_count(
        ec_fsm_pdo_entry_t *fsm /**< PDO mapping state machine. */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe))
        return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_WARN("Failed to clear PDO mapping.\n");
        fsm->state = ec_fsm_pdo_entry_state_error;
        return;
    }

    // find first entry
    if (!(fsm->entry = ec_fsm_pdo_entry_conf_next_entry(
                    fsm, &fsm->source_pdo->entries))) {
        
        if (fsm->slave->master->debug_level)
            EC_DBG("No entries to map.\n");

        fsm->state = ec_fsm_pdo_entry_state_end; // finished
        return;
    }

    // add first entry
    fsm->entry_pos = 1;
    ec_fsm_pdo_entry_conf_action_map(fsm);
}

/*****************************************************************************/

/** Starts to add a PDO entry.
 */
void ec_fsm_pdo_entry_conf_action_map(
        ec_fsm_pdo_entry_t *fsm /**< PDO mapping state machine. */
        )
{
    uint32_t value;

    if (fsm->slave->master->debug_level)
        EC_DBG("Mapping PDO entry 0x%04X:%02X (%u bit) at position %u.\n",
                fsm->entry->index, fsm->entry->subindex,
                fsm->entry->bit_length, fsm->entry_pos);

    value = fsm->entry->index << 16
        | fsm->entry->subindex << 8 | fsm->entry->bit_length;
    EC_WRITE_U32(fsm->request.data, value);
    fsm->request.data_size = 4;
    ec_sdo_request_address(&fsm->request, fsm->source_pdo->index, fsm->entry_pos);
    ecrt_sdo_request_write(&fsm->request);
    
    fsm->state = ec_fsm_pdo_entry_conf_state_map_entry;
    ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request);
    ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
}

/*****************************************************************************/

/** Add a PDO entry.
 */
void ec_fsm_pdo_entry_conf_state_map_entry(
        ec_fsm_pdo_entry_t *fsm /**< PDO mapping state machine. */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_WARN("Failed to map PDO entry 0x%04X:%02X (%u bit) to "
                "position %u.\n", fsm->entry->index, fsm->entry->subindex,
                fsm->entry->bit_length, fsm->entry_pos);
        fsm->state = ec_fsm_pdo_entry_state_error;
        return;
    }

    // find next entry
    if (!(fsm->entry = ec_fsm_pdo_entry_conf_next_entry(
                    fsm, &fsm->entry->list))) {

        // No more entries to add. Write entry count.
        EC_WRITE_U8(fsm->request.data, fsm->entry_pos);
        fsm->request.data_size = 1;
        ec_sdo_request_address(&fsm->request, fsm->source_pdo->index, 0);
        ecrt_sdo_request_write(&fsm->request);

        if (fsm->slave->master->debug_level)
            EC_DBG("Setting number of PDO entries to %u.\n", fsm->entry_pos);
        
        fsm->state = ec_fsm_pdo_entry_conf_state_set_entry_count;
        ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request);
        ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
        return;
    }

    // add next entry
    fsm->entry_pos++;
    ec_fsm_pdo_entry_conf_action_map(fsm);
}

/*****************************************************************************/

/** Set the number of entries.
 */
void ec_fsm_pdo_entry_conf_state_set_entry_count(
        ec_fsm_pdo_entry_t *fsm /**< PDO mapping state machine. */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("Failed to set number of entries.\n");
        fsm->state = ec_fsm_pdo_entry_state_error;
        return;
    }

    if (fsm->slave->master->debug_level)
        EC_DBG("Successfully configured mapping for PDO 0x%04X.\n",
                fsm->source_pdo->index);

    fsm->state = ec_fsm_pdo_entry_state_end; // finished
}

/******************************************************************************
 * Common state functions
 *****************************************************************************/

/** State: ERROR.
 */
void ec_fsm_pdo_entry_state_error(
        ec_fsm_pdo_entry_t *fsm /**< PDO mapping state machine. */
        )
{
}

/*****************************************************************************/

/** State: END.
 */
void ec_fsm_pdo_entry_state_end(
        ec_fsm_pdo_entry_t *fsm /**< PDO mapping state machine. */
        )
{
}

/*****************************************************************************/
