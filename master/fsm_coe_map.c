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
   EtherCAT CoE mapping state machines.
*/

/*****************************************************************************/

#include "globals.h"
#include "master.h"
#include "mailbox.h"
#include "canopen.h"
#include "fsm_coe_map.h"

/*****************************************************************************/

void ec_fsm_coe_map_state_start(ec_fsm_coe_map_t *);
void ec_fsm_coe_map_state_pdo_count(ec_fsm_coe_map_t *);
void ec_fsm_coe_map_state_pdo(ec_fsm_coe_map_t *);
void ec_fsm_coe_map_state_pdo_entry_count(ec_fsm_coe_map_t *);
void ec_fsm_coe_map_state_pdo_entry(ec_fsm_coe_map_t *);

void ec_fsm_coe_map_state_end(ec_fsm_coe_map_t *);
void ec_fsm_coe_map_state_error(ec_fsm_coe_map_t *);

void ec_fsm_coe_map_action_next_sync(ec_fsm_coe_map_t *);
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
    fsm->state = NULL;
    fsm->fsm_coe = fsm_coe;
    ec_pdo_mapping_init(&fsm->mapping);
}

/*****************************************************************************/

/**
   Destructor.
*/

void ec_fsm_coe_map_clear(ec_fsm_coe_map_t *fsm /**< finite state machine */)
{
    ec_pdo_mapping_clear(&fsm->mapping);
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
 * Start reading mapping.
 */

void ec_fsm_coe_map_state_start(
        ec_fsm_coe_map_t *fsm /**< finite state machine */
        )
{
    // read mapping of first sync manager
    fsm->sync_index = 0;
    ec_fsm_coe_map_action_next_sync(fsm);
}

/*****************************************************************************/

/**
 * Read mapping of next sync manager.
 */

void ec_fsm_coe_map_action_next_sync(
        ec_fsm_coe_map_t *fsm /**< finite state machine */
        )
{
    ec_slave_t *slave = fsm->slave;
    ec_sdo_entry_t *entry;

    for (; fsm->sync_index < slave->sii_sync_count; fsm->sync_index++) {
        if (!(fsm->sync_sdo = ec_slave_get_sdo(slave, 0x1C10 + fsm->sync_index)))
            continue;

        if (slave->master->debug_level)
            EC_DBG("Reading Pdo mapping of sync manager %u of slave %u.\n",
                    fsm->sync_index, slave->ring_position);

        ec_pdo_mapping_clear_pdos(&fsm->mapping);

        if (!(entry = ec_sdo_get_entry(fsm->sync_sdo, 0))) {
            EC_ERR("Sdo 0x%04X has no subindex 0 on slave %u.\n",
                    fsm->sync_sdo->index,
                    fsm->slave->ring_position);
            fsm->state = ec_fsm_coe_map_state_error;
            return;
        }

        ec_sdo_request_init_read(&fsm->request, entry);
        fsm->state = ec_fsm_coe_map_state_pdo_count;
        ec_fsm_coe_upload(fsm->fsm_coe, fsm->slave, &fsm->request);
        ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
        return;
    }

    fsm->state = ec_fsm_coe_map_state_end;
}

/*****************************************************************************/

/**
 * Count mapped Pdos.
 */

void ec_fsm_coe_map_state_pdo_count(
        ec_fsm_coe_map_t *fsm /**< finite state machine */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("Failed to read number of mapped Pdos from slave %u.\n",
                fsm->slave->ring_position);
        fsm->state = ec_fsm_coe_map_state_error;
        return;
    }

    fsm->sync_subindices = EC_READ_U8(fsm->request.data);

    if (fsm->slave->master->debug_level)
        EC_DBG("  %u Pdos mapped.\n", fsm->sync_subindices);

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
    ec_sdo_entry_t *entry;

    if (fsm->sync_subindex <= fsm->sync_subindices) {
        if (!(entry = ec_sdo_get_entry(fsm->sync_sdo,
                        fsm->sync_subindex))) {
            EC_ERR("Sdo 0x%04X has no subindex %u on slave %u.\n",
                    fsm->sync_sdo->index,
                    fsm->sync_subindex,
                    fsm->slave->ring_position);
            fsm->state = ec_fsm_coe_map_state_error;
            return;
        }

        ec_sdo_request_init_read(&fsm->request, entry);
        fsm->state = ec_fsm_coe_map_state_pdo;
        ec_fsm_coe_upload(fsm->fsm_coe, fsm->slave, &fsm->request);
        ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
        return;
    }

    {
        ec_sync_t *sync = fsm->slave->sii_syncs + fsm->sync_index;

        if (ec_pdo_mapping_copy(&sync->mapping, &fsm->mapping)) {
            fsm->state = ec_fsm_coe_map_state_error;
            return;
        }
        
        sync->mapping_source = EC_SYNC_MAPPING_COE;
        ec_pdo_mapping_clear_pdos(&fsm->mapping);

        // next sync manager
        fsm->sync_index++;
        ec_fsm_coe_map_action_next_sync(fsm);
    }
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
        EC_ERR("Failed to read mapped Pdo index from slave %u.\n",
                fsm->slave->ring_position);
        fsm->state = ec_fsm_coe_map_state_error;
        return;
    }

    {
        ec_sdo_entry_t *entry;

        if (!(fsm->pdo = (ec_pdo_t *)
                    kmalloc(sizeof(ec_pdo_t), GFP_KERNEL))) {
            EC_ERR("Failed to allocate Pdo.\n");
            fsm->state = ec_fsm_coe_map_state_error;
            return;
        }

        ec_pdo_init(fsm->pdo);
        fsm->pdo->index = EC_READ_U16(fsm->request.data);
        fsm->pdo->dir =
            ec_sync_direction(fsm->slave->sii_syncs + fsm->sync_index);

        if (fsm->slave->master->debug_level)
            EC_DBG("  Pdo 0x%04X.\n", fsm->pdo->index);

        if (!(fsm->pdo_sdo = ec_slave_get_sdo(fsm->slave, fsm->pdo->index))) {
            EC_ERR("Slave %u has no Sdo 0x%04X.\n",
                    fsm->slave->ring_position, fsm->pdo->index);
            ec_pdo_clear(fsm->pdo);
            kfree(fsm->pdo);
            fsm->state = ec_fsm_coe_map_state_error;
            return;
        }

        if (ec_pdo_set_name(fsm->pdo, fsm->pdo_sdo->name)) {
            ec_pdo_clear(fsm->pdo);
            kfree(fsm->pdo);
            fsm->state = ec_fsm_coe_map_state_error;
            return;
        }

        if (!(entry = ec_sdo_get_entry(fsm->pdo_sdo, 0))) {
            EC_ERR("Sdo 0x%04X has no subindex 0 on slave %u.\n",
                    fsm->pdo_sdo->index,
                    fsm->slave->ring_position);
            ec_pdo_clear(fsm->pdo);
            kfree(fsm->pdo);
            fsm->state = ec_fsm_coe_map_state_error;
            return;
        }

        list_add_tail(&fsm->pdo->list, &fsm->mapping.pdos);

        ec_sdo_request_init_read(&fsm->request, entry);
        fsm->state = ec_fsm_coe_map_state_pdo_entry_count;
        ec_fsm_coe_upload(fsm->fsm_coe, fsm->slave, &fsm->request);
        ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
        return;
    }
}

/*****************************************************************************/

/**
 * Read number of Pdo entries.
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
    ec_sdo_entry_t *entry;

    if (fsm->pdo_subindex <= fsm->pdo_subindices) {
        if (!(entry = ec_sdo_get_entry(fsm->pdo_sdo,
                        fsm->pdo_subindex))) {
            EC_ERR("Sdo 0x%04X has no subindex %u on slave %u.\n",
                    fsm->pdo_sdo->index, fsm->pdo_subindex,
                    fsm->slave->ring_position);
            fsm->state = ec_fsm_coe_map_state_error;
            return;
        }

        ec_sdo_request_init_read(&fsm->request, entry);
        fsm->state = ec_fsm_coe_map_state_pdo_entry;
        ec_fsm_coe_upload(fsm->fsm_coe, fsm->slave, &fsm->request);
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
        EC_ERR("Failed to read index of mapped Pdo entry from slave %u.\n",
                fsm->slave->ring_position);
        fsm->state = ec_fsm_coe_map_state_error;
        return;
    }

    {
        uint32_t pdo_entry_info;
        ec_sdo_t *sdo;
        ec_sdo_entry_t *entry;
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
            // we have a gap in the Pdo, next Pdo entry
            if (fsm->slave->master->debug_level) {
                EC_DBG("    Pdo entry gap: %u bit.\n",
                        pdo_entry->bit_length);
            }

            if (ec_pdo_entry_set_name(pdo_entry, "Gap")) {
                ec_pdo_entry_clear(pdo_entry);
                kfree(pdo_entry);
                fsm->state = ec_fsm_coe_map_state_error;
                return;
            }

            list_add_tail(&pdo_entry->list, &fsm->pdo->entries);
            fsm->pdo_subindex++;
            ec_fsm_coe_map_action_next_pdo_entry(fsm);
            return;
        }

        if (!(sdo = ec_slave_get_sdo(fsm->slave, pdo_entry->index))) {
            EC_ERR("Slave %u has no Sdo 0x%04X.\n",
                    fsm->slave->ring_position, pdo_entry->index);
            ec_pdo_entry_clear(pdo_entry);
            kfree(pdo_entry);
            fsm->state = ec_fsm_coe_map_state_error;
            return;
        }

        if (!(entry = ec_sdo_get_entry(sdo, pdo_entry->subindex))) {
            EC_ERR("Slave %u has no Sdo entry 0x%04X:%u.\n",
                    fsm->slave->ring_position, pdo_entry->index,
                    pdo_entry->subindex);
            ec_pdo_entry_clear(pdo_entry);
            kfree(pdo_entry);
            fsm->state = ec_fsm_coe_map_state_error;
            return;
        }

        if (ec_pdo_entry_set_name(pdo_entry, entry->description)) {
            ec_pdo_entry_clear(pdo_entry);
            kfree(pdo_entry);
            fsm->state = ec_fsm_coe_map_state_error;
            return;
        }

        if (fsm->slave->master->debug_level) {
            EC_DBG("    Pdo entry 0x%04X \"%s\" (%u bit).\n", pdo_entry->index,
                    pdo_entry->name ? pdo_entry->name : "???",
                    pdo_entry->bit_length);
        }

        list_add_tail(&pdo_entry->list, &fsm->pdo->entries);

        // next Pdo entry
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
