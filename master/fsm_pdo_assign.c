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

void ec_fsm_pdo_assign_next_dir(ec_fsm_pdo_assign_t *);

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

    fsm->dir = (ec_direction_t) -1; // next is EC_DIR_OUTPUT
    ec_fsm_pdo_assign_next_dir(fsm);
}

/*****************************************************************************/

/** Process Pdo assignment of next direction.
 */
void ec_fsm_pdo_assign_next_dir(
        ec_fsm_pdo_assign_t *fsm /**< Pdo assignment state machine. */
        )
{
    fsm->dir++;

    for (; fsm->dir <= EC_DIR_INPUT; fsm->dir++) {
        fsm->pdos = &fsm->slave->config->pdos[fsm->dir];
        
        if (!(fsm->sync = ec_slave_get_pdo_sync(fsm->slave, fsm->dir))) {
            if (!list_empty(&fsm->pdos->list)) {
                EC_ERR("No sync manager for direction %u!\n", fsm->dir);
                fsm->state = ec_fsm_pdo_assign_state_end;
                return;
            }
            continue;
        }

        // check if assignment has to be altered
        if (ec_pdo_list_equal(&fsm->sync->pdos, fsm->pdos))
            continue;

        // Pdo assignment has to be changed. Does the slave support this?
        if (!(fsm->slave->sii.mailbox_protocols & EC_MBOX_COE)
                || (fsm->slave->sii.has_general
                    && !fsm->slave->sii.coe_details.enable_pdo_assign)) {
            EC_ERR("Slave %u does not support assigning Pdos!\n",
                    fsm->slave->ring_position);
            fsm->state = ec_fsm_pdo_assign_state_error;
            return;
        }

        if (fsm->slave->master->debug_level) {
            EC_DBG("Changing Pdo assignment for SM%u of slave %u.\n",
                    fsm->sync->index, fsm->slave->ring_position);
        }

        if (ec_sdo_request_alloc(&fsm->request, 2)) {
            fsm->state = ec_fsm_pdo_assign_state_error;
            return;
        }

        // set mapped Pdo count to zero
        EC_WRITE_U8(fsm->request.data, 0); // zero Pdos mapped
        fsm->request.data_size = 1;
        ec_sdo_request_address(&fsm->request, 0x1C10 + fsm->sync->index, 0);
        ecrt_sdo_request_write(&fsm->request);
        if (fsm->slave->master->debug_level)
            EC_DBG("Setting Pdo count to zero for SM%u.\n", fsm->sync->index);

        fsm->state = ec_fsm_pdo_assign_state_zero_count;
        ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request);
        ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
        return;
    }

    if (fsm->slave->master->debug_level)
        EC_DBG("Pdo assignment finished for slave %u.\n",
                fsm->slave->ring_position);
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
            0x1C10 + fsm->sync->index, fsm->pdo_count);
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
                    fsm->sync->index, fsm->slave->ring_position);
        ec_fsm_pdo_assign_next_dir(fsm);
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
                fsm->pdo->index, fsm->sync->index, fsm->slave->ring_position);
        fsm->state = ec_fsm_pdo_assign_state_error;
        return;
    }

    // find next Pdo
    if (!(fsm->pdo = ec_fsm_pdo_assign_next_pdo(fsm, &fsm->pdo->list))) {
        // no more Pdos to map. write Pdo count
        EC_WRITE_U8(fsm->request.data, fsm->pdo_count);
        fsm->request.data_size = 1;
        ec_sdo_request_address(&fsm->request, 0x1C10 + fsm->sync->index, 0);
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
        EC_DBG("Successfully set Pdo assignment for SM%u of slave %u.\n",
                fsm->sync->index, fsm->slave->ring_position);

    // assignment for this direction finished
    ec_fsm_pdo_assign_next_dir(fsm);
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
