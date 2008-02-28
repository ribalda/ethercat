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

#include "fsm_pdo_config.h"

/*****************************************************************************/

void ec_fsm_pdo_config_state_start(ec_fsm_pdo_config_t *);
void ec_fsm_pdo_config_state_zero_count(ec_fsm_pdo_config_t *);
void ec_fsm_pdo_config_state_add_entry(ec_fsm_pdo_config_t *);
void ec_fsm_pdo_config_state_entry_count(ec_fsm_pdo_config_t *);
void ec_fsm_pdo_config_state_end(ec_fsm_pdo_config_t *);
void ec_fsm_pdo_config_state_error(ec_fsm_pdo_config_t *);

void ec_fsm_pdo_config_next_pdo(ec_fsm_pdo_config_t *);

/*****************************************************************************/

/** Constructor.
 */
void ec_fsm_pdo_config_init(
        ec_fsm_pdo_config_t *fsm, /**< pdo_config state machine */
        ec_fsm_coe_t *fsm_coe /**< CoE state machine to use */
        )
{
    fsm->fsm_coe = fsm_coe;
    fsm->sdodata.data = (uint8_t *) &fsm->sdo_value;
}

/*****************************************************************************/

/** Destructor.
 */
void ec_fsm_pdo_config_clear(
        ec_fsm_pdo_config_t *fsm /**< pdo_config state machine */
        )
{
}

/*****************************************************************************/

/** Start Pdo configuration state machine.
 */
void ec_fsm_pdo_config_start(
        ec_fsm_pdo_config_t *fsm, /**< Pdo configuration state machine */
        ec_slave_t *slave /**< slave to configure */
        )
{
    fsm->slave = slave;
    fsm->state = ec_fsm_pdo_config_state_start;
}

/*****************************************************************************/

/** Get running state.
 *
 * \return false, if state machine has terminated
 */
int ec_fsm_pdo_config_running(
        const ec_fsm_pdo_config_t *fsm /**< Pdo configuration state machine */
        )
{
    return fsm->state != ec_fsm_pdo_config_state_end
        && fsm->state != ec_fsm_pdo_config_state_error;
}

/*****************************************************************************/

/** Executes the current state.
 *
 * \return false, if state machine has terminated
 */
int ec_fsm_pdo_config_exec(
        ec_fsm_pdo_config_t *fsm /**< Pdo configuration state machine */
        )
{
    fsm->state(fsm);
    return ec_fsm_pdo_config_running(fsm);
}

/*****************************************************************************/

/** Get execution result.
 *
 * \return true, if the state machine terminated gracefully
 */
int ec_fsm_pdo_config_success(
        const ec_fsm_pdo_config_t *fsm /**< Pdo configuration state machine */
        )
{
    return fsm->state == ec_fsm_pdo_config_state_end;
}

/******************************************************************************
 * State functions.
 *****************************************************************************/

/** Start Pdo configuration.
 */
void ec_fsm_pdo_config_state_start(
        ec_fsm_pdo_config_t *fsm /**< Pdo configuration state machine */
        )
{
    if (!fsm->slave->config) {
        fsm->state = ec_fsm_pdo_config_state_end;
        return;
    }

    fsm->pdo = NULL;
    ec_fsm_pdo_config_next_pdo(fsm);
}

/*****************************************************************************/

/** Process configuration of next Pdo.
 */
void ec_fsm_pdo_config_next_pdo(
        ec_fsm_pdo_config_t *fsm /**< Pdo configuration state machine */
        )
{
    ec_direction_t dir;
    const ec_pdo_mapping_t *map;
    const ec_pdo_t *pdo, *mapped_pdo;
    
    for (dir = EC_DIR_OUTPUT; dir <= EC_DIR_INPUT; dir++) {
        map = &fsm->slave->config->mapping[dir];

        list_for_each_entry(pdo, &map->pdos, list) {
            if (fsm->pdo) { // there was a Pdo configured in the last run
                if (pdo == fsm->pdo) // this is the last Pdo
                    fsm->pdo = NULL; // take the next one
            } else {
                if ((mapped_pdo = ec_slave_find_pdo(fsm->slave, pdo->index)))
                    if (ec_pdo_equal_entries(pdo, mapped_pdo))
                        continue; // Pdo configured correctly

                fsm->pdo = pdo;
                break;
            }
        }
    }

    if (!fsm->pdo) {
        if (fsm->slave->master->debug_level)
            EC_DBG("Pdo configuration finished for slave %u.\n",
                    fsm->slave->ring_position);
        fsm->state = ec_fsm_pdo_config_state_end;
        return;
    }

    // Pdo configuration has to be changed. Does the slave support this?
    if (fsm->slave->sii.mailbox_protocols & EC_MBOX_COE
            || (fsm->slave->sii.has_general
                && !fsm->slave->sii.coe_details.enable_pdo_configuration)) {
        EC_ERR("Slave %u does not support changing the Pdo configuration!\n",
                fsm->slave->ring_position);
        fsm->state = ec_fsm_pdo_config_state_error;
        return;
    }

    if (fsm->slave->master->debug_level) {
        EC_DBG("Changing configuration of Pdo 0x%04X of slave %u.\n",
                fsm->pdo->index, fsm->slave->ring_position);
    }

    // set mapped Pdo count to zero
    fsm->sdodata.index = fsm->pdo->index;
    fsm->sdodata.subindex = 0; // number of configured entries
    EC_WRITE_U8(&fsm->sdo_value, 0);
    fsm->sdodata.size = 1;
    if (fsm->slave->master->debug_level)
        EC_DBG("Setting entry count to zero for Pdo 0x%04X.\n",
                fsm->pdo->index);

    fsm->state = ec_fsm_pdo_config_state_zero_count;
    ec_fsm_coe_download(fsm->fsm_coe, fsm->slave, &fsm->sdodata);
    ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
}

/*****************************************************************************/

/** Process next Pdo entry.
 */
ec_pdo_entry_t *ec_fsm_pdo_config_next_entry(
        const ec_fsm_pdo_config_t *fsm, /**< Pdo configuration state machine */
        const struct list_head *list /**< current entry list item */
        )
{
    list = list->next; 
    if (list == &fsm->pdo->entries)
        return NULL; // no next entry
    return list_entry(list, ec_pdo_entry_t, list);
}

/*****************************************************************************/

/** Starts to add a Pdo entry.
 */
void ec_fsm_pdo_config_add_entry(
        ec_fsm_pdo_config_t *fsm /**< Pdo configuration state machine. */
        )
{
    uint32_t value;

    fsm->sdodata.subindex = fsm->entry_count;
    value = fsm->entry->index << 16
        | fsm->entry->subindex << 8 | fsm->entry->bit_length;
    EC_WRITE_U32(&fsm->sdo_value, value);
    fsm->sdodata.size = 4;

    if (fsm->slave->master->debug_level)
        EC_DBG("Configuring Pdo entry %08X at position %u.\n",
                value, fsm->entry_count);
    
    fsm->state = ec_fsm_pdo_config_state_add_entry;
    ec_fsm_coe_download(fsm->fsm_coe, fsm->slave, &fsm->sdodata);
    ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
}

/*****************************************************************************/

/** Set the number of configured entries to zero.
 */
void ec_fsm_pdo_config_state_zero_count(
        ec_fsm_pdo_config_t *fsm /**< Pdo configuration state machine */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("Failed to clear Pdo configuration for slave %u.\n",
                fsm->slave->ring_position);
        fsm->state = ec_fsm_pdo_config_state_error;
        return;
    }

    // find first entry
    if (!(fsm->entry =
                ec_fsm_pdo_config_next_entry(fsm, &fsm->pdo->entries))) {
        if (fsm->slave->master->debug_level)
            EC_DBG("No entries to configure for Pdo 0x%04X of slave %u.\n",
                    fsm->pdo->index, fsm->slave->ring_position);
        ec_fsm_pdo_config_next_pdo(fsm);
        return;
    }

    // add first entry
    fsm->entry_count = 1;
    ec_fsm_pdo_config_add_entry(fsm);
}

/*****************************************************************************/

/** Add a Pdo entry.
 */
void ec_fsm_pdo_config_state_add_entry(
        ec_fsm_pdo_config_t *fsm /**< Pdo configuration state machine */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("Failed to add entry 0x%04X:%u for slave %u.\n",
                fsm->entry->index, fsm->entry->subindex,
                fsm->slave->ring_position);
        fsm->state = ec_fsm_pdo_config_state_error;
        return;
    }

    // find next entry
    if (!(fsm->entry = ec_fsm_pdo_config_next_entry(fsm, &fsm->entry->list))) {
        // No more entries to add. Write entry count.
        fsm->sdodata.subindex = 0;
        EC_WRITE_U8(&fsm->sdo_value, fsm->entry_count);
        fsm->sdodata.size = 1;

        if (fsm->slave->master->debug_level)
            EC_DBG("Setting number of Pdo entries to %u.\n",
                    fsm->entry_count);
        
        fsm->state = ec_fsm_pdo_config_state_entry_count;
        ec_fsm_coe_download(fsm->fsm_coe, fsm->slave, &fsm->sdodata);
        ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
        return;
    }

    // add next entry
    fsm->entry_count++;
    ec_fsm_pdo_config_add_entry(fsm);
}

/*****************************************************************************/

/** Set the number of entries.
 */
void ec_fsm_pdo_config_state_entry_count(
        ec_fsm_pdo_config_t *fsm /**< Pdo configuration state machine */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("Failed to set number of entries for slave %u.\n",
                fsm->slave->ring_position);
        fsm->state = ec_fsm_pdo_config_state_error;
        return;
    }

    if (fsm->slave->master->debug_level)
        EC_DBG("Successfully configured Pdo 0x%04X on slave %u.\n",
                fsm->pdo->index, fsm->slave->ring_position);

    ec_fsm_pdo_config_next_pdo(fsm);
}

/******************************************************************************
 * Common state functions
 *****************************************************************************/

/** State: ERROR.
 */
void ec_fsm_pdo_config_state_error(
        ec_fsm_pdo_config_t *fsm /**< Pdo configuration state machine */
        )
{
}

/*****************************************************************************/

/** State: END.
 */
void ec_fsm_pdo_config_state_end(
        ec_fsm_pdo_config_t *fsm /**< Pdo configuration state machine */
        )
{
}

/*****************************************************************************/
