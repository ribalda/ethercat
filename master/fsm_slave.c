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
 * EtherCAT slave (SDO) state machine.
 */

/*****************************************************************************/

#include "globals.h"
#include "master.h"
#include "mailbox.h"

#include "fsm_slave.h"

/*****************************************************************************/

void ec_fsm_slave_state_idle(ec_fsm_slave_t *);
void ec_fsm_slave_state_sdo_request(ec_fsm_slave_t *);


/*****************************************************************************/

/** Constructor.
 */
void ec_fsm_slave_init(
        ec_fsm_slave_t *fsm, /**< Slave state machine. */
        ec_slave_t *slave, /**< EtherCAT slave. */
        ec_datagram_t *datagram /**< Datagram object to use. */
        )
{
    fsm->slave = slave;
    fsm->datagram = datagram;
    fsm->datagram->data_size = 0;
    fsm->state = ec_fsm_slave_state_idle;

    // init sub-state-machines
    ec_fsm_coe_init(&fsm->fsm_coe, fsm->datagram);
}

/*****************************************************************************/

/** Destructor.
 */
void ec_fsm_slave_clear(
        ec_fsm_slave_t *fsm /**< Master state machine. */
        )
{
    // clear sub-state machines
    ec_fsm_coe_clear(&fsm->fsm_coe);
}

/*****************************************************************************/

/** Executes the current state of the state machine.
 *
 * If the state machine's datagram is not sent or received yet, the execution
 * of the state machine is delayed to the next cycle.
 */
void ec_fsm_slave_exec(
        ec_fsm_slave_t *fsm /**< Slave state machine. */
        )
{
    if (fsm->datagram->state == EC_DATAGRAM_SENT
        || fsm->datagram->state == EC_DATAGRAM_QUEUED) {
        // datagram was not sent or received yet.
        return;
    }

    fsm->state(fsm);
    return;
}

/******************************************************************************
 * Slave state machine
 *****************************************************************************/

/** Slave state: IDLE.
 *
 * Check for pending SDO requests and process one.
 *
 */
void ec_fsm_slave_state_idle(
        ec_fsm_slave_t *fsm /**< Slave state machine. */
        )
{
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = slave->master;
    ec_master_sdo_request_t *request, *next;

    // search the first matching external request to be processed
    list_for_each_entry_safe(request, next, &slave->slave_sdo_requests, list) {
        list_del_init(&request->list); // dequeue
        request->req.state = EC_INT_REQUEST_BUSY;

        if (slave->current_state == EC_SLAVE_STATE_INIT) {
            EC_ERR("Discarding SDO request, slave %u is in INIT.\n",
                    slave->ring_position);
            request->req.state = EC_INT_REQUEST_FAILURE;
            wake_up(&slave->sdo_queue);
            continue;
        }

        // Found pending SDO request. Execute it!
        if (master->debug_level)
            EC_DBG("Processing SDO request for slave %u...\n",
                    slave->ring_position);

        // Start SDO transfer
        fsm->sdo_request = &request->req;
        fsm->state = ec_fsm_slave_state_sdo_request;
        ec_fsm_coe_transfer(&fsm->fsm_coe, slave, &request->req);
        ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
        ec_master_queue_sdo_datagram(fsm->slave->master,fsm->datagram);
        return;
    }
}

/*****************************************************************************/

/** Slave state: SDO_REQUEST.
 */
void ec_fsm_slave_state_sdo_request(
        ec_fsm_slave_t *fsm /**< Slave state machine. */
        )
{
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = slave->master;
    ec_sdo_request_t *request = fsm->sdo_request;

    if (ec_fsm_coe_exec(&fsm->fsm_coe))
    {
        ec_master_queue_sdo_datagram(fsm->slave->master,fsm->datagram);
        return;
    }
    if (!ec_fsm_coe_success(&fsm->fsm_coe)) {
        EC_DBG("Failed to process SDO request for slave %u.\n",
                fsm->slave->ring_position);
        request->state = EC_INT_REQUEST_FAILURE;
        wake_up(&slave->sdo_queue);
        fsm->sdo_request = NULL;
        fsm->state = ec_fsm_slave_state_idle;
        return;
    }

    // SDO request finished
    request->state = EC_INT_REQUEST_SUCCESS;
    wake_up(&slave->sdo_queue);

    if (master->debug_level)
        EC_DBG("Finished SDO request for slave %u.\n",
                fsm->slave->ring_position);

    fsm->sdo_request = NULL;
    fsm->datagram->data_size = 0;
    fsm->state = ec_fsm_slave_state_idle;
}
