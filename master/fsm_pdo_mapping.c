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
 * EtherCAT Pdo mapping state machine.
 */

/*****************************************************************************/

#include "globals.h"
#include "master.h"
#include "mailbox.h"
#include "slave_config.h"

#include "fsm_pdo_mapping.h"

/*****************************************************************************/

void ec_fsm_pdo_mapping_state_start(ec_fsm_pdo_mapping_t *);
void ec_fsm_pdo_mapping_state_zero_count(ec_fsm_pdo_mapping_t *);
void ec_fsm_pdo_mapping_state_add_pdo(ec_fsm_pdo_mapping_t *);
void ec_fsm_pdo_mapping_state_pdo_count(ec_fsm_pdo_mapping_t *);
void ec_fsm_pdo_mapping_state_end(ec_fsm_pdo_mapping_t *);
void ec_fsm_pdo_mapping_state_error(ec_fsm_pdo_mapping_t *);

void ec_fsm_pdo_mapping_next_dir(ec_fsm_pdo_mapping_t *);

/*****************************************************************************/

/** Constructor.
 */
void ec_fsm_pdo_mapping_init(
        ec_fsm_pdo_mapping_t *fsm, /**< mapping state machine */
        ec_fsm_coe_t *fsm_coe /**< CoE state machine to use */
        )
{
    fsm->fsm_coe = fsm_coe;
    fsm->sdodata.data = (uint8_t *) &fsm->sdo_value;
}

/*****************************************************************************/

/** Destructor.
 */
void ec_fsm_pdo_mapping_clear(
        ec_fsm_pdo_mapping_t *fsm /**< mapping state machine */
        )
{
}

/*****************************************************************************/

/** Start Pdo mapping configuration state machine.
 */
void ec_fsm_pdo_mapping_start(
        ec_fsm_pdo_mapping_t *fsm, /**< mapping state machine */
        ec_slave_t *slave /**< slave to configure */
        )
{
    fsm->slave = slave;
    fsm->state = ec_fsm_pdo_mapping_state_start;
}

/*****************************************************************************/

/** Get running state.
 *
 * \return false, if state machine has terminated
 */
int ec_fsm_pdo_mapping_running(
        const ec_fsm_pdo_mapping_t *fsm /**< mapping state machine */
        )
{
    return fsm->state != ec_fsm_pdo_mapping_state_end
        && fsm->state != ec_fsm_pdo_mapping_state_error;
}

/*****************************************************************************/

/** Executes the current state of the state machine.
 *
 * If the state machine's datagram is not sent or received yet, the execution
 * of the state machine is delayed to the next cycle.
 *
 * \return false, if state machine has terminated
 */
int ec_fsm_pdo_mapping_exec(
        ec_fsm_pdo_mapping_t *fsm /**< mapping state machine */
        )
{
    fsm->state(fsm);
    return ec_fsm_pdo_mapping_running(fsm);
}

/*****************************************************************************/

/** Get execution result.
 *
 * \return true, if the state machine terminated gracefully
 */
int ec_fsm_pdo_mapping_success(
        const ec_fsm_pdo_mapping_t *fsm /**< mapping state machine */
        )
{
    return fsm->state == ec_fsm_pdo_mapping_state_end;
}

/******************************************************************************
 * State functions.
 *****************************************************************************/

/** Start mapping configuration.
 */
void ec_fsm_pdo_mapping_state_start(
        ec_fsm_pdo_mapping_t *fsm /**< mapping state machine */
        )
{
    if (!fsm->slave->config) {
        fsm->state = ec_fsm_pdo_mapping_state_end;
        return;
    }

    fsm->dir = (ec_direction_t) -1; // next is EC_DIR_OUTPUT
    ec_fsm_pdo_mapping_next_dir(fsm);
}

/*****************************************************************************/

/** Process mapping of next direction.
 */
void ec_fsm_pdo_mapping_next_dir(
        ec_fsm_pdo_mapping_t *fsm /**< mapping state machine */
        )
{
    fsm->dir++;

    for (; fsm->dir <= EC_DIR_INPUT; fsm->dir++) {
        fsm->mapping = &fsm->slave->config->mapping[fsm->dir];
        
        if (!(fsm->sync = ec_slave_get_pdo_sync(fsm->slave, fsm->dir))) {
            if (!list_empty(&fsm->mapping->pdos)) {
                EC_ERR("No sync manager for direction %u!\n", fsm->dir);
                fsm->state = ec_fsm_pdo_mapping_state_end;
                return;
            }
            continue;
        }

        // check if mapping has to be altered
        if (ec_pdo_mapping_equal(&fsm->sync->mapping, fsm->mapping))
            continue;

        // Pdo mapping has to be changed. Does the slave support this?
        if (!fsm->slave->sii.mailbox_protocols & EC_MBOX_COE
                || (fsm->slave->sii.has_general
                    && !fsm->slave->sii.coe_details.enable_pdo_assign)) {
            EC_ERR("Slave %u does not support changing the Pdo mapping!\n",
                    fsm->slave->ring_position);
            fsm->state = ec_fsm_pdo_mapping_state_error;
            return;
        }

        if (fsm->slave->master->debug_level) {
            EC_DBG("Changing Pdo mapping for SM%u of slave %u.\n",
                    fsm->sync->index, fsm->slave->ring_position);
        }

        // set mapped Pdo count to zero
        fsm->sdodata.index = 0x1C10 + fsm->sync->index;
        fsm->sdodata.subindex = 0; // mapped Pdo count
        EC_WRITE_U8(&fsm->sdo_value, 0); // zero Pdos mapped
        fsm->sdodata.size = 1;
        if (fsm->slave->master->debug_level)
            EC_DBG("Setting Pdo count to zero for SM%u.\n", fsm->sync->index);

        fsm->state = ec_fsm_pdo_mapping_state_zero_count;
        ec_fsm_coe_download(fsm->fsm_coe, fsm->slave, &fsm->sdodata);
        ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
        return;
    }

    if (fsm->slave->master->debug_level)
        EC_DBG("Pdo mapping finished for slave %u.\n",
                fsm->slave->ring_position);
    fsm->state = ec_fsm_pdo_mapping_state_end;
}

/*****************************************************************************/

/** Process mapping of next Pdo.
 */
ec_pdo_t *ec_fsm_pdo_mapping_next_pdo(
        const ec_fsm_pdo_mapping_t *fsm, /**< mapping state machine */
        const struct list_head *list /**< current Pdo list item */
        )
{
    list = list->next; 
    if (list == &fsm->mapping->pdos)
        return NULL; // no next Pdo
    return list_entry(list, ec_pdo_t, list);
}

/*****************************************************************************/

/** Add a Pdo to the mapping.
 */
void ec_fsm_pdo_mapping_add_pdo(
        ec_fsm_pdo_mapping_t *fsm /**< mapping state machine */
        )
{
    fsm->sdodata.subindex = fsm->pdo_count;
    EC_WRITE_U16(&fsm->sdo_value, fsm->pdo->index);
    fsm->sdodata.size = 2;

    if (fsm->slave->master->debug_level)
        EC_DBG("Mapping Pdo 0x%04X at position %u.\n",
                fsm->pdo->index, fsm->sdodata.subindex);
    
    fsm->state = ec_fsm_pdo_mapping_state_add_pdo;
    ec_fsm_coe_download(fsm->fsm_coe, fsm->slave, &fsm->sdodata);
    ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
}

/*****************************************************************************/

/** Set the number of mapped Pdos to zero.
 */
void ec_fsm_pdo_mapping_state_zero_count(
        ec_fsm_pdo_mapping_t *fsm /**< mapping state machine */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("Failed to clear Pdo mapping for slave %u.\n",
                fsm->slave->ring_position);
        fsm->state = ec_fsm_pdo_mapping_state_error;
        return;
    }

    // map all Pdos belonging to the current sync manager
    
    // find first Pdo
    if (!(fsm->pdo = ec_fsm_pdo_mapping_next_pdo(fsm, &fsm->mapping->pdos))) {
        if (fsm->slave->master->debug_level)
            EC_DBG("No Pdos to map for SM%u of slave %u.\n",
                    fsm->sync->index, fsm->slave->ring_position);
        ec_fsm_pdo_mapping_next_dir(fsm);
        return;
    }

    // add first Pdo to mapping
    fsm->pdo_count = 1;
    ec_fsm_pdo_mapping_add_pdo(fsm);
}

/*****************************************************************************/

/** Add a Pdo to the sync managers mapping.
 */
void ec_fsm_pdo_mapping_state_add_pdo(
        ec_fsm_pdo_mapping_t *fsm /**< mapping state machine */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("Failed to map Pdo 0x%04X for SM%u of slave %u.\n",
                fsm->pdo->index, fsm->sync->index, fsm->slave->ring_position);
        fsm->state = ec_fsm_pdo_mapping_state_error;
        return;
    }

    // find next Pdo
    if (!(fsm->pdo = ec_fsm_pdo_mapping_next_pdo(fsm, &fsm->pdo->list))) {
        // no more Pdos to map. write Pdo count
        fsm->sdodata.subindex = 0;
        EC_WRITE_U8(&fsm->sdo_value, fsm->pdo_count);
        fsm->sdodata.size = 1;

        if (fsm->slave->master->debug_level)
            EC_DBG("Setting number of mapped Pdos to %u.\n",
                    fsm->pdo_count);
        
        fsm->state = ec_fsm_pdo_mapping_state_pdo_count;
        ec_fsm_coe_download(fsm->fsm_coe, fsm->slave, &fsm->sdodata);
        ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
        return;
    }

    // add next Pdo to mapping
    fsm->pdo_count++;
    ec_fsm_pdo_mapping_add_pdo(fsm);
}

/*****************************************************************************/

/** Set the number of mapped Pdos.
 */
void ec_fsm_pdo_mapping_state_pdo_count(
        ec_fsm_pdo_mapping_t *fsm /**< mapping state machine */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("Failed to set number of mapped Pdos for slave %u.\n",
                fsm->slave->ring_position);
        fsm->state = ec_fsm_pdo_mapping_state_error;
        return;
    }

    if (fsm->slave->master->debug_level)
        EC_DBG("Successfully set Pdo mapping for SM%u of slave %u.\n",
                fsm->sync->index, fsm->slave->ring_position);

    // mapping configuration for this direction finished
    ec_fsm_pdo_mapping_next_dir(fsm);
}

/******************************************************************************
 * Common state functions
 *****************************************************************************/

/** State: ERROR.
 */
void ec_fsm_pdo_mapping_state_error(
        ec_fsm_pdo_mapping_t *fsm /**< mapping state machine */
        )
{
}

/*****************************************************************************/

/** State: END.
 */
void ec_fsm_pdo_mapping_state_end(
        ec_fsm_pdo_mapping_t *fsm /**< mapping state machine */
        )
{
}

/*****************************************************************************/
