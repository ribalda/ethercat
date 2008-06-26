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
 * EtherCAT Pdo assignment state machine.
 */

/*****************************************************************************/

#include "globals.h"
#include "master.h"
#include "mailbox.h"
#include "slave_config.h"

#include "fsm_pdo_assign.h"

/*****************************************************************************/

void ec_fsm_pdo_assign_state_start(ec_fsm_pdo_assign_t *);
void ec_fsm_pdo_assign_state_zero_count(ec_fsm_pdo_assign_t *);
void ec_fsm_pdo_assign_state_add_pdo(ec_fsm_pdo_assign_t *);
void ec_fsm_pdo_assign_state_pdo_count(ec_fsm_pdo_assign_t *);
void ec_fsm_pdo_assign_state_end(ec_fsm_pdo_assign_t *);
void ec_fsm_pdo_assign_state_error(ec_fsm_pdo_assign_t *);

void ec_fsm_pdo_assign_next_sync(ec_fsm_pdo_assign_t *);

/*****************************************************************************/

/** Constructor.
 */
void ec_fsm_pdo_assign_init(
        ec_fsm_pdo_assign_t *fsm, /**< Pdo assignment state machine. */
        ec_fsm_coe_t *fsm_coe /**< CoE state machine to use */
        )
{
    fsm->fsm_coe = fsm_coe;
    ec_sdo_request_init(&fsm->request);
}

/*****************************************************************************/

/** Destructor.
 */
void ec_fsm_pdo_assign_clear(
        ec_fsm_pdo_assign_t *fsm /**< Pdo assignment state machine. */
        )
{
    ec_sdo_request_clear(&fsm->request);
}

/*****************************************************************************/

/** Start Pdo assignment state machine.
 */
void ec_fsm_pdo_assign_start(
        ec_fsm_pdo_assign_t *fsm, /**< Pdo assignment state machine. */
        ec_slave_t *slave /**< slave to configure */
        )
{
    fsm->slave = slave;
    fsm->state = ec_fsm_pdo_assign_state_start;
}

/*****************************************************************************/

/** Get running state.
 *
 * \return false, if state machine has terminated
 */
int ec_fsm_pdo_assign_running(
        const ec_fsm_pdo_assign_t *fsm /**< Pdo assignment state machine. */
        )
{
    return fsm->state != ec_fsm_pdo_assign_state_end
        && fsm->state != ec_fsm_pdo_assign_state_error;
}

/*****************************************************************************/

/** Executes the current state of the state machine.
 *
 * If the state machine's datagram is not sent or received yet, the execution
 * of the state machine is delayed to the next cycle.
 *
 * \return false, if state machine has terminated
 */
int ec_fsm_pdo_assign_exec(
        ec_fsm_pdo_assign_t *fsm /**< Pdo assignment state machine. */
        )
{
    fsm->state(fsm);
    return ec_fsm_pdo_assign_running(fsm);
}

/*****************************************************************************/

/** Get execution result.
 *
 * \return true, if the state machine terminated gracefully
 */
int ec_fsm_pdo_assign_success(
        const ec_fsm_pdo_assign_t *fsm /**< Pdo assignment state machine. */
        )
{
    return fsm->state == ec_fsm_pdo_assign_state_end;
}

/******************************************************************************
 * State functions.
 *****************************************************************************/

/** Start Pdo assignment.
 */
void ec_fsm_pdo_assign_state_start(
        ec_fsm_pdo_assign_t *fsm /**< Pdo assignment state machine. */
        )
{
    if (!fsm->slave->config) {
        fsm->state = ec_fsm_pdo_assign_state_end;
        return;
    }

    fsm->sync_index = 0xff; // next is zero
    fsm->num_configured_syncs = 0;
    ec_fsm_pdo_assign_next_sync(fsm);
}

/*****************************************************************************/

/** Process Pdo assignment of next sync manager.
 */
void ec_fsm_pdo_assign_next_sync(
        ec_fsm_pdo_assign_t *fsm /**< Pdo assignment state machine. */
        )
{
    fsm->sync_index++;

    for (; fsm->sync_index < EC_MAX_SYNCS; fsm->sync_index++) {
        fsm->pdos = &fsm->slave->config->sync_configs[fsm->sync_index].pdos;
        
        if (!(fsm->sync = ec_slave_get_sync(fsm->slave, fsm->sync_index))) {
            if (!list_empty(&fsm->pdos->list)) {
                EC_ERR("Slave %u does not provide a configuration for sync "
                        "manager %u!\n", fsm->slave->ring_position,
                        fsm->sync_index);
                fsm->state = ec_fsm_pdo_assign_state_end;
                return;
            }
            continue;
        }

        // check if assignment has to be altered
        if (ec_pdo_list_equal(&fsm->sync->pdos, fsm->pdos))
            continue;

        if (fsm->slave->master->debug_level) {
            EC_DBG("Pdo assignment of SM%u differs in slave %u:\n",
                    fsm->sync_index, fsm->slave->ring_position);
            EC_DBG("Currently assigned Pdos: ");
            ec_pdo_list_print(&fsm->sync->pdos);
            printk("\n");
            EC_DBG("Pdos to assign: ");
            ec_pdo_list_print(fsm->pdos);
            printk("\n");
        }

        // Pdo assignment has to be changed. Does the slave support this?
        if (!(fsm->slave->sii.mailbox_protocols & EC_MBOX_COE)
                || (fsm->slave->sii.has_general
                    && !fsm->slave->sii.coe_details.enable_pdo_assign)) {
            EC_ERR("Slave %u does not support assigning Pdos!\n",
                    fsm->slave->ring_position);
            fsm->state = ec_fsm_pdo_assign_state_error;
            return;
        }

        fsm->num_configured_syncs++;

        if (fsm->slave->master->debug_level) {
            EC_DBG("Changing Pdo assignment for SM%u of slave %u.\n",
                    fsm->sync_index, fsm->slave->ring_position);
        }

        if (ec_sdo_request_alloc(&fsm->request, 2)) {
            fsm->state = ec_fsm_pdo_assign_state_error;
            return;
        }

        // set mapped Pdo count to zero
        EC_WRITE_U8(fsm->request.data, 0); // zero Pdos mapped
        fsm->request.data_size = 1;
        ec_sdo_request_address(&fsm->request, 0x1C10 + fsm->sync_index, 0);
        ecrt_sdo_request_write(&fsm->request);
        if (fsm->slave->master->debug_level)
            EC_DBG("Setting Pdo count to zero for SM%u.\n", fsm->sync_index);

        fsm->state = ec_fsm_pdo_assign_state_zero_count;
        ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request);
        ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
        return;
    }

    if (fsm->slave->master->debug_level && !fsm->num_configured_syncs)
        EC_DBG("Pdo assignments of slave %u are already configured"
                " correctly.\n", fsm->slave->ring_position);
    fsm->state = ec_fsm_pdo_assign_state_end;
}

/*****************************************************************************/

/** Assign next Pdo.
 */
ec_pdo_t *ec_fsm_pdo_assign_next_pdo(
        const ec_fsm_pdo_assign_t *fsm, /**< Pdo assignment state machine. */
        const struct list_head *list /**< current Pdo list item */
        )
{
    list = list->next; 
    if (list == &fsm->pdos->list)
        return NULL; // no next Pdo
    return list_entry(list, ec_pdo_t, list);
}

/*****************************************************************************/

/** Assign a Pdo.
 */
void ec_fsm_pdo_assign_add_pdo(
        ec_fsm_pdo_assign_t *fsm /**< Pdo assignment state machine. */
        )
{
    EC_WRITE_U16(fsm->request.data, fsm->pdo->index);
    fsm->request.data_size = 2;
    ec_sdo_request_address(&fsm->request,
            0x1C10 + fsm->sync_index, fsm->pdo_count);
    ecrt_sdo_request_write(&fsm->request);
    if (fsm->slave->master->debug_level)
        EC_DBG("Assigning Pdo 0x%04X at position %u.\n",
                fsm->pdo->index, fsm->pdo_count);
    
    fsm->state = ec_fsm_pdo_assign_state_add_pdo;
    ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request);
    ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
}

/*****************************************************************************/

/** Set the number of assigned Pdos to zero.
 */
void ec_fsm_pdo_assign_state_zero_count(
        ec_fsm_pdo_assign_t *fsm /**< Pdo assignment state machine. */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("Failed to clear Pdo assignment of slave %u.\n",
                fsm->slave->ring_position);
        fsm->state = ec_fsm_pdo_assign_state_error;
        return;
    }

    // assign all Pdos belonging to the current sync manager
    
    // find first Pdo
    if (!(fsm->pdo = ec_fsm_pdo_assign_next_pdo(fsm, &fsm->pdos->list))) {
        if (fsm->slave->master->debug_level)
            EC_DBG("No Pdos to assign for SM%u of slave %u.\n",
                    fsm->sync_index, fsm->slave->ring_position);
        ec_fsm_pdo_assign_next_sync(fsm);
        return;
    }

    // assign first Pdo
    fsm->pdo_count = 1;
    ec_fsm_pdo_assign_add_pdo(fsm);
}

/*****************************************************************************/

/** Add a Pdo to the sync managers Pdo assignment.
 */
void ec_fsm_pdo_assign_state_add_pdo(
        ec_fsm_pdo_assign_t *fsm /**< Pdo assignment state machine. */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("Failed to map Pdo 0x%04X for SM%u of slave %u.\n",
                fsm->pdo->index, fsm->sync_index, fsm->slave->ring_position);
        fsm->state = ec_fsm_pdo_assign_state_error;
        return;
    }

    // find next Pdo
    if (!(fsm->pdo = ec_fsm_pdo_assign_next_pdo(fsm, &fsm->pdo->list))) {
        // no more Pdos to map. write Pdo count
        EC_WRITE_U8(fsm->request.data, fsm->pdo_count);
        fsm->request.data_size = 1;
        ec_sdo_request_address(&fsm->request, 0x1C10 + fsm->sync_index, 0);
        ecrt_sdo_request_write(&fsm->request);
        if (fsm->slave->master->debug_level)
            EC_DBG("Setting number of assigned Pdos to %u.\n",
                    fsm->pdo_count);
        
        fsm->state = ec_fsm_pdo_assign_state_pdo_count;
        ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request);
        ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
        return;
    }

    // add next Pdo to assignment
    fsm->pdo_count++;
    ec_fsm_pdo_assign_add_pdo(fsm);
}

/*****************************************************************************/

/** Set the number of assigned Pdos.
 */
void ec_fsm_pdo_assign_state_pdo_count(
        ec_fsm_pdo_assign_t *fsm /**< Pdo assignment state machine. */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("Failed to set number of assigned Pdos for slave %u.\n",
                fsm->slave->ring_position);
        fsm->state = ec_fsm_pdo_assign_state_error;
        return;
    }

    if (fsm->slave->master->debug_level)
        EC_DBG("Successfully configured Pdo assignment for SM%u of"
                " slave %u.\n", fsm->sync_index, fsm->slave->ring_position);

    // assignment for this sync manager finished
    ec_fsm_pdo_assign_next_sync(fsm);
}

/******************************************************************************
 * Common state functions
 *****************************************************************************/

/** State: ERROR.
 */
void ec_fsm_pdo_assign_state_error(
        ec_fsm_pdo_assign_t *fsm /**< Pdo assignment state machine. */
        )
{
}

/*****************************************************************************/

/** State: END.
 */
void ec_fsm_pdo_assign_state_end(
        ec_fsm_pdo_assign_t *fsm /**< Pdo assignment state machine. */
        )
{
}

/*****************************************************************************/
