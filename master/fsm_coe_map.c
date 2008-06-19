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
   EtherCAT Pdo information state machines.
*/

/*****************************************************************************/

#include "globals.h"
#include "master.h"
#include "mailbox.h"
#include "sdo_request.h"
#include "fsm_coe_map.h"

/*****************************************************************************/

void ec_fsm_coe_map_state_start(ec_fsm_coe_map_t *);
void ec_fsm_coe_map_state_pdo_count(ec_fsm_coe_map_t *);
void ec_fsm_coe_map_state_pdo(ec_fsm_coe_map_t *);
void ec_fsm_coe_map_state_pdo_entry_count(ec_fsm_coe_map_t *);
void ec_fsm_coe_map_state_pdo_entry(ec_fsm_coe_map_t *);

void ec_fsm_coe_map_state_end(ec_fsm_coe_map_t *);
void ec_fsm_coe_map_state_error(ec_fsm_coe_map_t *);

void ec_fsm_coe_map_action_next_dir(ec_fsm_coe_map_t *);
void ec_fsm_coe_map_action_next_pdo(ec_fsm_coe_map_t *);
void ec_fsm_coe_map_action_next_pdo_entry(ec_fsm_coe_map_t *);

/*****************************************************************************/

/**
   Constructor.
*/

void ec_fsm_coe_map_init(
        ec_fsm_coe_map_t *fsm, /**< finite state machine */
        ec_fsm_coe_t *fsm_coe /**< CoE state machine to use */
        )
{
    fsm->fsm_coe = fsm_coe;
    fsm->state = NULL;
    ec_sdo_request_init(&fsm->request);
    ec_pdo_list_init(&fsm->pdos);
}

/*****************************************************************************/

/**
   Destructor.
*/

void ec_fsm_coe_map_clear(ec_fsm_coe_map_t *fsm /**< finite state machine */)
{
    ec_sdo_request_clear(&fsm->request);
    ec_pdo_list_clear(&fsm->pdos);
}

/*****************************************************************************/

/**
   Starts to upload an Sdo from a slave.
*/

void ec_fsm_coe_map_start(
        ec_fsm_coe_map_t *fsm, /**< finite state machine */
        ec_slave_t *slave /**< EtherCAT slave */
        )
{
    fsm->slave = slave;
    fsm->state = ec_fsm_coe_map_state_start;
}

/*****************************************************************************/

/**
   Executes the current state of the state machine.
   \return false, if state machine has terminated
*/

int ec_fsm_coe_map_exec(ec_fsm_coe_map_t *fsm /**< finite state machine */)
{
    fsm->state(fsm);

    return fsm->state != ec_fsm_coe_map_state_end
        && fsm->state != ec_fsm_coe_map_state_error;
}

/*****************************************************************************/

/**
   Returns, if the state machine terminated with success.
   \return non-zero if successful.
*/

int ec_fsm_coe_map_success(ec_fsm_coe_map_t *fsm /**< Finite state machine */)
{
    return fsm->state == ec_fsm_coe_map_state_end;
}

/******************************************************************************
 *  state functions
 *****************************************************************************/

/**
 * Start reading Pdo assignment.
 */

void ec_fsm_coe_map_state_start(
        ec_fsm_coe_map_t *fsm /**< finite state machine */
        )
{
    // read Pdo assignment for first direction
    fsm->dir = (ec_direction_t) -1; // next is EC_DIR_OUTPUT
    ec_fsm_coe_map_action_next_dir(fsm);
}

/*****************************************************************************/

/**
 * Read Pdo assignment of next direction manager.
 */

void ec_fsm_coe_map_action_next_dir(
        ec_fsm_coe_map_t *fsm /**< finite state machine */
        )
{
    ec_slave_t *slave = fsm->slave;

    fsm->dir++;

    if (slave->master->debug_level)
        EC_DBG("Processing dir %u of slave %u.\n",
                fsm->dir, slave->ring_position);

    for (; fsm->dir <= EC_DIR_INPUT; fsm->dir++) {

        if (!(fsm->sync = ec_slave_get_pdo_sync(slave, fsm->dir))) {
            if (slave->master->debug_level)
                EC_DBG("No sync manager for direction %u!\n", fsm->dir);
            continue;
        }

        fsm->sync_sdo_index = 0x1C10 + fsm->sync->index;

        if (slave->master->debug_level)
            EC_DBG("Reading Pdo assignment of sync manager %u of slave %u.\n",
                    fsm->sync->index, slave->ring_position);

        ec_pdo_list_clear_pdos(&fsm->pdos);

        ec_sdo_request_address(&fsm->request, fsm->sync_sdo_index, 0);
        fsm->request.response_timeout = 10000;
        ecrt_sdo_request_read(&fsm->request);
        fsm->state = ec_fsm_coe_map_state_pdo_count;
        ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request);
        ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
        return;
    }

    if (slave->master->debug_level)
        EC_DBG("Reading of Pdo assignment finished for slave %u.\n",
                slave->ring_position);

    fsm->state = ec_fsm_coe_map_state_end;
}

/*****************************************************************************/

/**
 * Count assigned Pdos.
 */

void ec_fsm_coe_map_state_pdo_count(
        ec_fsm_coe_map_t *fsm /**< finite state machine */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("Failed to read number of assigned Pdos from slave %u.\n",
                fsm->slave->ring_position);
        fsm->state = ec_fsm_coe_map_state_error;
        return;
    }

    if (fsm->request.data_size != sizeof(uint8_t)) {
        EC_ERR("Invalid data size %u returned when uploading Sdo 0x%04X:%02X "
                "from slave %u.\n", fsm->request.data_size,
                fsm->request.index, fsm->request.subindex,
                fsm->slave->ring_position);
        fsm->state = ec_fsm_coe_map_state_error;
        return;
    }
    fsm->sync_subindices = EC_READ_U8(fsm->request.data);

    if (fsm->slave->master->debug_level)
        EC_DBG("  %u Pdos assigned.\n", fsm->sync_subindices);

    // read first Pdo
    fsm->sync_subindex = 1;
    ec_fsm_coe_map_action_next_pdo(fsm);
}

/*****************************************************************************/

/**
 * Read next Pdo.
 */

void ec_fsm_coe_map_action_next_pdo(
        ec_fsm_coe_map_t *fsm /**< finite state machine */
        )
{
    if (fsm->sync_subindex <= fsm->sync_subindices) {
        ec_sdo_request_address(&fsm->request, fsm->sync_sdo_index,
                fsm->sync_subindex);
        fsm->request.response_timeout = 0;
        ecrt_sdo_request_read(&fsm->request);
        fsm->state = ec_fsm_coe_map_state_pdo;
        ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request);
        ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
        return;
    }

    // finished reading Pdo assignment/mapping
    
    if (ec_pdo_list_copy(&fsm->sync->pdos, &fsm->pdos)) {
        fsm->state = ec_fsm_coe_map_state_error;
        return;
    }

    fsm->sync->assign_source = EC_ASSIGN_COE;
    ec_pdo_list_clear_pdos(&fsm->pdos);

    // next direction
    ec_fsm_coe_map_action_next_dir(fsm);
}

/*****************************************************************************/

/**
 * Fetch Pdo information.
 */

void ec_fsm_coe_map_state_pdo(
        ec_fsm_coe_map_t *fsm /**< finite state machine */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("Failed to read assigned Pdo index from slave %u.\n",
                fsm->slave->ring_position);
        fsm->state = ec_fsm_coe_map_state_error;
        return;
    }

    if (fsm->request.data_size != sizeof(uint16_t)) {
        EC_ERR("Invalid data size %u returned when uploading Sdo 0x%04X:%02X "
                "from slave %u.\n", fsm->request.data_size,
                fsm->request.index, fsm->request.subindex,
                fsm->slave->ring_position);
        fsm->state = ec_fsm_coe_map_state_error;
        return;
    }

    if (!(fsm->pdo = (ec_pdo_t *)
                kmalloc(sizeof(ec_pdo_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate Pdo.\n");
        fsm->state = ec_fsm_coe_map_state_error;
        return;
    }

    ec_pdo_init(fsm->pdo);
    fsm->pdo->index = EC_READ_U16(fsm->request.data);
    fsm->pdo->dir = ec_sync_direction(fsm->sync);

    if (fsm->slave->master->debug_level)
        EC_DBG("  Pdo 0x%04X.\n", fsm->pdo->index);

    list_add_tail(&fsm->pdo->list, &fsm->pdos.list);

    ec_sdo_request_address(&fsm->request, fsm->pdo->index, 0);
    fsm->request.response_timeout = 0;
    ecrt_sdo_request_read(&fsm->request);
    fsm->state = ec_fsm_coe_map_state_pdo_entry_count;
    ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request);
    ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
}

/*****************************************************************************/

/**
 * Read number of mapped Pdo entries.
 */

void ec_fsm_coe_map_state_pdo_entry_count(
        ec_fsm_coe_map_t *fsm /**< finite state machine */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("Failed to read number of mapped Pdo entries from slave %u.\n",
                fsm->slave->ring_position);
        fsm->state = ec_fsm_coe_map_state_error;
        return;
    }

    if (fsm->request.data_size != sizeof(uint8_t)) {
        EC_ERR("Invalid data size %u returned when uploading Sdo 0x%04X:%02X "
                "from slave %u.\n", fsm->request.data_size,
                fsm->request.index, fsm->request.subindex,
                fsm->slave->ring_position);
        fsm->state = ec_fsm_coe_map_state_error;
        return;
    }
    fsm->pdo_subindices = EC_READ_U8(fsm->request.data);

    if (fsm->slave->master->debug_level)
        EC_DBG("    %u Pdo entries mapped.\n", fsm->pdo_subindices);

    // read first Pdo entry
    fsm->pdo_subindex = 1;
    ec_fsm_coe_map_action_next_pdo_entry(fsm);
}

/*****************************************************************************/

/**
 * Read next Pdo entry.
 */

void ec_fsm_coe_map_action_next_pdo_entry(
        ec_fsm_coe_map_t *fsm /**< finite state machine */
        )
{
    if (fsm->pdo_subindex <= fsm->pdo_subindices) {
        ec_sdo_request_address(&fsm->request, fsm->pdo->index, fsm->pdo_subindex);
        fsm->request.response_timeout = 0;
        ecrt_sdo_request_read(&fsm->request);
        fsm->state = ec_fsm_coe_map_state_pdo_entry;
        ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request);
        ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
        return;
    }

    // next Pdo
    fsm->sync_subindex++;
    ec_fsm_coe_map_action_next_pdo(fsm);
}

/*****************************************************************************/

/**
 * Read Pdo entry information.
 */

void ec_fsm_coe_map_state_pdo_entry(
        ec_fsm_coe_map_t *fsm /**< finite state machine */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("Failed to read mapped Pdo entry from slave %u.\n",
                fsm->slave->ring_position);
        fsm->state = ec_fsm_coe_map_state_error;
        return;
    }

    if (fsm->request.data_size != sizeof(uint32_t)) {
        EC_ERR("Invalid data size %u returned when uploading Sdo 0x%04X:%02X "
                "from slave %u.\n", fsm->request.data_size,
                fsm->request.index, fsm->request.subindex,
                fsm->slave->ring_position);
        fsm->state = ec_fsm_coe_map_state_error;
    } else {
        uint32_t pdo_entry_info;
        ec_pdo_entry_t *pdo_entry;

        pdo_entry_info = EC_READ_U32(fsm->request.data);

        if (!(pdo_entry = (ec_pdo_entry_t *)
                    kmalloc(sizeof(ec_pdo_entry_t), GFP_KERNEL))) {
            EC_ERR("Failed to allocate Pdo entry.\n");
            fsm->state = ec_fsm_coe_map_state_error;
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
                fsm->state = ec_fsm_coe_map_state_error;
                return;
            }
        }

        if (fsm->slave->master->debug_level) {
            EC_DBG("    Pdo entry 0x%04X:%02X, %u bit,  \"%s\".\n",
                    pdo_entry->index, pdo_entry->subindex,
                    pdo_entry->bit_length,
                    pdo_entry->name ? pdo_entry->name : "???");
        }

        // next Pdo entry
        list_add_tail(&pdo_entry->list, &fsm->pdo->entries);
        fsm->pdo_subindex++;
        ec_fsm_coe_map_action_next_pdo_entry(fsm);
    }
}

/*****************************************************************************/

/**
   State: ERROR.
*/

void ec_fsm_coe_map_state_error(
        ec_fsm_coe_map_t *fsm /**< finite state machine */
        )
{
}

/*****************************************************************************/

/**
   State: END.
*/

void ec_fsm_coe_map_state_end(
        ec_fsm_coe_map_t *fsm /**< finite state machine */
        )
{
}

/*****************************************************************************/
