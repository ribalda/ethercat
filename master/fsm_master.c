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
 * EtherCAT master state machine.
 */

/*****************************************************************************/

#include "globals.h"
#include "master.h"
#include "mailbox.h"
#include "slave_config.h"
#ifdef EC_EOE
#include "ethernet.h"
#endif

#include "fsm_master.h"

/*****************************************************************************/

void ec_fsm_master_state_start(ec_fsm_master_t *);
void ec_fsm_master_state_broadcast(ec_fsm_master_t *);
void ec_fsm_master_state_read_state(ec_fsm_master_t *);
void ec_fsm_master_state_acknowledge(ec_fsm_master_t *);
void ec_fsm_master_state_configure_slave(ec_fsm_master_t *);
void ec_fsm_master_state_clear_addresses(ec_fsm_master_t *);
void ec_fsm_master_state_scan_slave(ec_fsm_master_t *);
void ec_fsm_master_state_write_sii(ec_fsm_master_t *);
void ec_fsm_master_state_sdo_dictionary(ec_fsm_master_t *);
void ec_fsm_master_state_sdo_request(ec_fsm_master_t *);

/*****************************************************************************/

/** Constructor.
 */
void ec_fsm_master_init(
        ec_fsm_master_t *fsm, /**< Master state machine. */
        ec_master_t *master, /**< EtherCAT master. */
        ec_datagram_t *datagram /**< Datagram object to use. */
        )
{
    fsm->master = master;
    fsm->datagram = datagram;
    fsm->state = ec_fsm_master_state_start;
    fsm->idle = 0;
    fsm->slaves_responding = 0;
    fsm->topology_change_pending = 0;
    fsm->slave_states = EC_SLAVE_STATE_UNKNOWN;

    // init sub-state-machines
    ec_fsm_coe_init(&fsm->fsm_coe, fsm->datagram);
    ec_fsm_pdo_init(&fsm->fsm_pdo, &fsm->fsm_coe);
    ec_fsm_change_init(&fsm->fsm_change, fsm->datagram);
    ec_fsm_slave_config_init(&fsm->fsm_slave_config, fsm->datagram,
            &fsm->fsm_change, &fsm->fsm_coe, &fsm->fsm_pdo);
    ec_fsm_slave_scan_init(&fsm->fsm_slave_scan, fsm->datagram,
            &fsm->fsm_slave_config, &fsm->fsm_pdo);
    ec_fsm_sii_init(&fsm->fsm_sii, fsm->datagram);
}

/*****************************************************************************/

/** Destructor.
 */
void ec_fsm_master_clear(
        ec_fsm_master_t *fsm /**< Master state machine. */
        )
{
    // clear sub-state machines
    ec_fsm_coe_clear(&fsm->fsm_coe);
    ec_fsm_pdo_clear(&fsm->fsm_pdo);
    ec_fsm_change_clear(&fsm->fsm_change);
    ec_fsm_slave_config_clear(&fsm->fsm_slave_config);
    ec_fsm_slave_scan_clear(&fsm->fsm_slave_scan);
    ec_fsm_sii_clear(&fsm->fsm_sii);
}

/*****************************************************************************/

/** Executes the current state of the state machine.
 *
 * If the state machine's datagram is not sent or received yet, the execution
 * of the state machine is delayed to the next cycle.
 */
void ec_fsm_master_exec(
        ec_fsm_master_t *fsm /**< Master state machine. */
        )
{
    if (fsm->datagram->state == EC_DATAGRAM_SENT
        || fsm->datagram->state == EC_DATAGRAM_QUEUED) {
        // datagram was not sent or received yet.
        return;
    }

    fsm->state(fsm);
}

/*****************************************************************************/

/**
 * \return true, if the state machine is in an idle phase
 */
int ec_fsm_master_idle(
        const ec_fsm_master_t *fsm /**< Master state machine. */
        )
{
    return fsm->idle;
}

/*****************************************************************************/

/** Restarts the master state machine.
 */
void ec_fsm_master_restart(
        ec_fsm_master_t *fsm /**< Master state machine. */
        )
{
    fsm->state = ec_fsm_master_state_start;
    fsm->state(fsm); // execute immediately
}

/******************************************************************************
 * Master state machine
 *****************************************************************************/

/** Master state: START.
 *
 * Starts with getting slave count and slave states.
 */
void ec_fsm_master_state_start(
        ec_fsm_master_t *fsm /**< Master state machine. */
        )
{
    fsm->idle = 1;
    ec_datagram_brd(fsm->datagram, 0x0130, 2);
    fsm->state = ec_fsm_master_state_broadcast;
}

/*****************************************************************************/

/** Master state: BROADCAST.
 *
 * Processes the broadcast read slave count and slaves states.
 */
void ec_fsm_master_state_broadcast(
        ec_fsm_master_t *fsm /**< Master state machine. */
        )
{
    ec_datagram_t *datagram = fsm->datagram;
    unsigned int i, size;
    ec_slave_t *slave;
    ec_master_t *master = fsm->master;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT)
        return; // always retry

    // bus topology change?
    if (datagram->working_counter != fsm->slaves_responding) {
        fsm->topology_change_pending = 1;
        fsm->slaves_responding = datagram->working_counter;
        EC_INFO("%u slave(s) responding.\n", fsm->slaves_responding);
    }

    if (datagram->state != EC_DATAGRAM_RECEIVED) { // link is down
        ec_fsm_master_restart(fsm);
        return;
    }

    if (fsm->slaves_responding) {
        uint8_t states = EC_READ_U8(datagram->data);
        if (states != fsm->slave_states) { // slave states changed?
            char state_str[EC_STATE_STRING_SIZE];
            fsm->slave_states = states;
            ec_state_string(fsm->slave_states, state_str);
            EC_INFO("Slave states: %s.\n", state_str);
        }
    } else {
        fsm->slave_states = 0x00;
    }

    if (fsm->topology_change_pending) {
        down(&master->scan_sem);
        if (!master->allow_scan) {
            up(&master->scan_sem);
        } else {
            master->scan_busy = 1;
            up(&master->scan_sem);
            
            // topology change when scan is allowed:
            // clear all slaves and scan the bus
            fsm->topology_change_pending = 0;
            fsm->idle = 0;
            fsm->scan_jiffies = jiffies;

#ifdef EC_EOE
            ec_master_eoe_stop(master);
            ec_master_clear_eoe_handlers(master);
#endif
            ec_master_clear_slaves(master);

            master->slave_count = fsm->slaves_responding;

            if (!master->slave_count) {
                // no slaves present -> finish state machine.
                master->scan_busy = 0;
                wake_up_interruptible(&master->scan_queue);
                ec_fsm_master_restart(fsm);
                return;
            }

            size = sizeof(ec_slave_t) * master->slave_count;
            if (!(master->slaves = (ec_slave_t *) kmalloc(size, GFP_KERNEL))) {
                EC_ERR("Failed to allocate %u bytes of slave memory!\n",
                        size);
                master->slave_count = 0; // TODO avoid retrying scan!
                master->scan_busy = 0;
                wake_up_interruptible(&master->scan_queue);
                ec_fsm_master_restart(fsm);
                return;
            }

            // init slaves
            for (i = 0; i < master->slave_count; i++) {
                slave = master->slaves + i;
                ec_slave_init(slave, master, i, i + 1);

                // do not force reconfiguration in operation phase to avoid
                // unnecesssary process data interruptions
                if (master->phase != EC_OPERATION)
                    slave->force_config = 1;
            }

            // broadcast clear all station addresses
            ec_datagram_bwr(datagram, 0x0010, 2);
            EC_WRITE_U16(datagram->data, 0x0000);
            fsm->retries = EC_FSM_RETRIES;
            fsm->state = ec_fsm_master_state_clear_addresses;
            return;
        }
    }

    if (master->slave_count) {
        // fetch state from first slave
        fsm->slave = master->slaves;
        ec_datagram_fprd(fsm->datagram, fsm->slave->station_address,
                0x0130, 2);
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_master_state_read_state;
    } else {
        ec_fsm_master_restart(fsm);
    }
}

/*****************************************************************************/

/** Check for pending SII write requests and process one.
 * 
 * \return non-zero, if an SII write request is processed.
 */
int ec_fsm_master_action_process_sii(
        ec_fsm_master_t *fsm /**< Master state machine. */
        )
{
    ec_master_t *master = fsm->master;
    ec_sii_write_request_t *request;

    // search the first request to be processed
    while (1) {
        if (list_empty(&master->sii_requests))
            break;

        // get first request
        request = list_entry(master->sii_requests.next,
                ec_sii_write_request_t, list);
        list_del_init(&request->list); // dequeue
        request->state = EC_REQUEST_BUSY;

        // found pending SII write operation. execute it!
        if (master->debug_level)
            EC_DBG("Writing SII data to slave %u...\n",
                    request->slave->ring_position);
        fsm->sii_request = request;
        fsm->sii_index = 0;
        ec_fsm_sii_write(&fsm->fsm_sii, request->slave, request->offset,
                request->words, EC_FSM_SII_USE_CONFIGURED_ADDRESS);
        fsm->state = ec_fsm_master_state_write_sii;
        fsm->state(fsm); // execute immediately
        return 1;
    }

    return 0;
}

/*****************************************************************************/

/** Check for pending Sdo requests and process one.
 * 
 * \return non-zero, if an Sdo request is processed.
 */
int ec_fsm_master_action_process_sdo(
        ec_fsm_master_t *fsm /**< Master state machine. */
        )
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave;
    ec_sdo_request_t *req;
    ec_master_sdo_request_t *request;

    // search for internal requests to be processed
    for (slave = master->slaves;
            slave < master->slaves + master->slave_count;
            slave++) {
        if (!slave->config)
            continue;
        list_for_each_entry(req, &slave->config->sdo_requests, list) {
            if (req->state == EC_REQUEST_QUEUED) {

                if (ec_sdo_request_timed_out(req)) {
                    req->state = EC_REQUEST_FAILURE;
                    if (master->debug_level)
                        EC_DBG("Sdo request for slave %u timed out...\n",
                                slave->ring_position);
                    continue;
                }

                if (slave->current_state == EC_SLAVE_STATE_INIT) {
                    req->state = EC_REQUEST_FAILURE;
                    continue;
                }

                req->state = EC_REQUEST_BUSY;
                if (master->debug_level)
                    EC_DBG("Processing Sdo request for slave %u...\n",
                            slave->ring_position);

                fsm->idle = 0;
                fsm->sdo_request = req;
                fsm->slave = slave;
                fsm->state = ec_fsm_master_state_sdo_request;
                ec_fsm_coe_transfer(&fsm->fsm_coe, slave, req);
                ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
                return 1;
            }
        }
    }
    
    // search the first external request to be processed
    while (1) {
        if (list_empty(&master->slave_sdo_requests))
            break;

        // get first request
        request = list_entry(master->slave_sdo_requests.next,
                ec_master_sdo_request_t, list);
        list_del_init(&request->list); // dequeue
        request->req.state = EC_REQUEST_BUSY;

        slave = request->slave;
        if (slave->current_state == EC_SLAVE_STATE_INIT) {
            EC_ERR("Discarding Sdo request, slave %u is in INIT.\n",
                    slave->ring_position);
            request->req.state = EC_REQUEST_FAILURE;
            wake_up(&master->sdo_queue);
            continue;
        }

        // Found pending Sdo request. Execute it!
        if (master->debug_level)
            EC_DBG("Processing Sdo request for slave %u...\n",
                    slave->ring_position);

        // Start uploading Sdo
        fsm->idle = 0;
        fsm->sdo_request = &request->req;
        fsm->slave = slave;
        fsm->state = ec_fsm_master_state_sdo_request;
        ec_fsm_coe_transfer(&fsm->fsm_coe, slave, &request->req);
        ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
        return 1;
    }

    return 0;
}

/*****************************************************************************/

/** Master action: IDLE.
 *
 * Does secondary work.
 */
void ec_fsm_master_action_idle(
        ec_fsm_master_t *fsm /**< Master state machine. */
        )
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave;

    // Check for pending Sdo requests
    if (ec_fsm_master_action_process_sdo(fsm))
        return;

    // check, if slaves have an Sdo dictionary to read out.
    for (slave = master->slaves;
            slave < master->slaves + master->slave_count;
            slave++) {
        if (!(slave->sii.mailbox_protocols & EC_MBOX_COE)
                || slave->sdo_dictionary_fetched
                || slave->current_state == EC_SLAVE_STATE_INIT
                || jiffies - slave->jiffies_preop < EC_WAIT_SDO_DICT * HZ
                ) continue;

        if (master->debug_level) {
            EC_DBG("Fetching Sdo dictionary from slave %u.\n",
                    slave->ring_position);
        }

        slave->sdo_dictionary_fetched = 1;

        // start fetching Sdo dictionary
        fsm->idle = 0;
        fsm->slave = slave;
        fsm->state = ec_fsm_master_state_sdo_dictionary;
        ec_fsm_coe_dictionary(&fsm->fsm_coe, slave);
        ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
        return;
    }

    // check for pending SII write operations.
    if (ec_fsm_master_action_process_sii(fsm))
        return; // SII write request found

    ec_fsm_master_restart(fsm);
}

/*****************************************************************************/

/** Master action: Get state of next slave.
 */
void ec_fsm_master_action_next_slave_state(
        ec_fsm_master_t *fsm /**< Master state machine. */
        )
{
    ec_master_t *master = fsm->master;

    // is there another slave to query?
    fsm->slave++;
    if (fsm->slave < master->slaves + master->slave_count) {
        // fetch state from next slave
        fsm->idle = 1;
        ec_datagram_fprd(fsm->datagram,
                fsm->slave->station_address, 0x0130, 2);
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_master_state_read_state;
        return;
    }

    // all slaves processed
    ec_fsm_master_action_idle(fsm);
}

/*****************************************************************************/

/** Master action: Configure.
 */
void ec_fsm_master_action_configure(
        ec_fsm_master_t *fsm /**< Master state machine. */
        )
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave = fsm->slave;

    // Does the slave have to be configured?
    if ((slave->current_state != slave->requested_state
                || slave->force_config) && !slave->error_flag) {

        // Start slave configuration, if it is allowed.
        down(&master->config_sem);
        if (!master->allow_config) {
            up(&master->config_sem);
        } else {
            master->config_busy = 1;
            up(&master->config_sem);

            if (master->debug_level) {
                char old_state[EC_STATE_STRING_SIZE],
                     new_state[EC_STATE_STRING_SIZE];
                ec_state_string(slave->current_state, old_state);
                ec_state_string(slave->requested_state, new_state);
                EC_DBG("Changing state of slave %u from %s to %s%s.\n",
                        slave->ring_position, old_state, new_state,
                        slave->force_config ? " (forced)" : "");
            }

            fsm->idle = 0;
            fsm->state = ec_fsm_master_state_configure_slave;
            ec_fsm_slave_config_start(&fsm->fsm_slave_config, slave);
            fsm->state(fsm); // execute immediately
            return;
        }
    }

    // slave has error flag set; process next one
    ec_fsm_master_action_next_slave_state(fsm);
}

/*****************************************************************************/

/** Master state: READ STATE.
 *
 * Fetches the AL state of a slave.
 */
void ec_fsm_master_state_read_state(
        ec_fsm_master_t *fsm /**< Master state machine. */
        )
{
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = fsm->datagram;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        EC_ERR("Failed to receive AL state datagram for slave %u"
                " (datagram state %u)\n",
                slave->ring_position, datagram->state);
        ec_fsm_master_restart(fsm);
        return;
    }

    // did the slave not respond to its station address?
    if (datagram->working_counter != 1) {
        if (!slave->error_flag) {
            slave->error_flag = 1;
            if (fsm->master->debug_level)
                EC_DBG("Slave %u did not respond to state query.\n",
                        fsm->slave->ring_position);
        }
        fsm->topology_change_pending = 1;
        ec_fsm_master_restart(fsm);
        return;
    }

    // A single slave responded
    ec_slave_set_state(slave, EC_READ_U8(datagram->data));

    if (!slave->error_flag) {
        // Check, if new slave state has to be acknowledged
        if (slave->current_state & EC_SLAVE_STATE_ACK_ERR) {
            fsm->idle = 0;
            fsm->state = ec_fsm_master_state_acknowledge;
            ec_fsm_change_ack(&fsm->fsm_change, slave);
            fsm->state(fsm); // execute immediately
            return;
        }

        // No acknowlegde necessary; check for configuration
        ec_fsm_master_action_configure(fsm);
        return;
    }

    // slave has error flag set; process next one
    ec_fsm_master_action_next_slave_state(fsm);
}

/*****************************************************************************/

/** Master state: ACKNOWLEDGE.
 */
void ec_fsm_master_state_acknowledge(
        ec_fsm_master_t *fsm /**< Master state machine. */
        )
{
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_change_exec(&fsm->fsm_change))
        return;

    if (!ec_fsm_change_success(&fsm->fsm_change)) {
        fsm->slave->error_flag = 1;
        EC_ERR("Failed to acknowledge state change on slave %u.\n",
               slave->ring_position);
    }

    ec_fsm_master_action_configure(fsm);
}

/*****************************************************************************/

/** Master state: CLEAR ADDRESSES.
 */
void ec_fsm_master_state_clear_addresses(
        ec_fsm_master_t *fsm /**< Master state machine. */
        )
{
    ec_master_t *master = fsm->master;
    ec_datagram_t *datagram = fsm->datagram;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        EC_ERR("Failed to receive address clearing datagram (state %u).\n",
                datagram->state);
        master->scan_busy = 0;
        wake_up_interruptible(&master->scan_queue);
        ec_fsm_master_restart(fsm);
        return;
    }

    if (datagram->working_counter != master->slave_count) {
        EC_WARN("Failed to clear all station addresses: Cleared %u of %u",
                datagram->working_counter, master->slave_count);
    }

    EC_INFO("Scanning bus.\n");

    // begin scanning of slaves
    fsm->slave = master->slaves;
    fsm->state = ec_fsm_master_state_scan_slave;
    ec_fsm_slave_scan_start(&fsm->fsm_slave_scan, fsm->slave);
    ec_fsm_slave_scan_exec(&fsm->fsm_slave_scan); // execute immediately
}

/*****************************************************************************/

/** Master state: SCAN SLAVE.
 *
 * Executes the sub-statemachine for the scanning of a slave.
 */
void ec_fsm_master_state_scan_slave(
        ec_fsm_master_t *fsm /**< Master state machine. */
        )
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_slave_scan_exec(&fsm->fsm_slave_scan))
        return;

#ifdef EC_EOE
    if (slave->sii.mailbox_protocols & EC_MBOX_EOE) {
        // create EoE handler for this slave
        ec_eoe_t *eoe;
        if (!(eoe = kmalloc(sizeof(ec_eoe_t), GFP_KERNEL))) {
            EC_ERR("Failed to allocate EoE handler memory for slave %u!\n",
                    slave->ring_position);
        } else if (ec_eoe_init(eoe, slave)) {
            EC_ERR("Failed to init EoE handler for slave %u!\n",
                    slave->ring_position);
            kfree(eoe);
        } else {
            list_add_tail(&eoe->list, &master->eoe_handlers);
        }
    }
#endif

    // another slave to fetch?
    fsm->slave++;
    if (fsm->slave < master->slaves + master->slave_count) {
        ec_fsm_slave_scan_start(&fsm->fsm_slave_scan, fsm->slave);
        ec_fsm_slave_scan_exec(&fsm->fsm_slave_scan); // execute immediately
        return;
    }

    EC_INFO("Bus scanning completed in %u ms.\n",
            (u32) (jiffies - fsm->scan_jiffies) * 1000 / HZ);

    master->scan_busy = 0;
    wake_up_interruptible(&master->scan_queue);

    // Attach slave configurations
    ec_master_attach_slave_configs(master);

#ifdef EC_EOE
    // check if EoE processing has to be started
    ec_master_eoe_start(master);
#endif

    ec_fsm_master_restart(fsm);
}

/*****************************************************************************/

/** Master state: CONFIGURE SLAVE.
 *
 * Starts configuring a slave.
 */
void ec_fsm_master_state_configure_slave(
        ec_fsm_master_t *fsm /**< Master state machine. */
        )
{
    ec_master_t *master = fsm->master;

    if (ec_fsm_slave_config_exec(&fsm->fsm_slave_config))
        return;

    // configuration finished
    master->config_busy = 0;
    wake_up_interruptible(&master->config_queue);

    if (!ec_fsm_slave_config_success(&fsm->fsm_slave_config)) {
        // TODO: mark slave_config as failed.
    }

    fsm->idle = 1;
    ec_fsm_master_action_next_slave_state(fsm);
}

/*****************************************************************************/

/** Master state: WRITE SII.
 */
void ec_fsm_master_state_write_sii(
        ec_fsm_master_t *fsm /**< Master state machine. */
        )
{
    ec_master_t *master = fsm->master;
    ec_sii_write_request_t *request = fsm->sii_request;
    ec_slave_t *slave = request->slave;

    if (ec_fsm_sii_exec(&fsm->fsm_sii)) return;

    if (!ec_fsm_sii_success(&fsm->fsm_sii)) {
        EC_ERR("Failed to write SII data to slave %u.\n",
                slave->ring_position);
        request->state = EC_REQUEST_FAILURE;
        wake_up(&master->sii_queue);
        ec_fsm_master_restart(fsm);
        return;
    }

    fsm->sii_index++;
    if (fsm->sii_index < request->nwords) {
        ec_fsm_sii_write(&fsm->fsm_sii, slave,
                request->offset + fsm->sii_index,
                request->words + fsm->sii_index,
                EC_FSM_SII_USE_CONFIGURED_ADDRESS);
        ec_fsm_sii_exec(&fsm->fsm_sii); // execute immediately
        return;
    }

    // finished writing SII
    if (master->debug_level)
        EC_DBG("Finished writing %u words of SII data to slave %u.\n",
                request->nwords, slave->ring_position);

    if (request->offset <= 4 && request->offset + request->nwords > 4) {
        // alias was written
        slave->sii.alias = EC_READ_U16(request->words + 4);
    }
    // TODO: Evaluate other SII contents!
    
    request->state = EC_REQUEST_SUCCESS;
    wake_up(&master->sii_queue);

    // check for another SII write request
    if (ec_fsm_master_action_process_sii(fsm))
        return; // processing another request

    ec_fsm_master_restart(fsm);
}

/*****************************************************************************/

/** Master state: SDO DICTIONARY.
 */
void ec_fsm_master_state_sdo_dictionary(
        ec_fsm_master_t *fsm /**< Master state machine. */
        )
{
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = fsm->master;

    if (ec_fsm_coe_exec(&fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(&fsm->fsm_coe)) {
        ec_fsm_master_restart(fsm);
        return;
    }

    // Sdo dictionary fetching finished

    if (master->debug_level) {
        unsigned int sdo_count, entry_count;
        ec_slave_sdo_dict_info(slave, &sdo_count, &entry_count);
        EC_DBG("Fetched %u Sdos and %u entries from slave %u.\n",
               sdo_count, entry_count, slave->ring_position);
    }

    ec_fsm_master_restart(fsm);
}

/*****************************************************************************/

/** Master state: SDO REQUEST.
 */
void ec_fsm_master_state_sdo_request(
        ec_fsm_master_t *fsm /**< Master state machine. */
        )
{
    ec_master_t *master = fsm->master;
    ec_sdo_request_t *request = fsm->sdo_request;

    if (ec_fsm_coe_exec(&fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(&fsm->fsm_coe)) {
        EC_DBG("Failed to process Sdo request for slave %u.\n",
                fsm->slave->ring_position);
        request->state = EC_REQUEST_FAILURE;
        wake_up(&master->sdo_queue);
        ec_fsm_master_restart(fsm);
        return;
    }

    // Sdo request finished 
    request->state = EC_REQUEST_SUCCESS;
    wake_up(&master->sdo_queue);

    if (master->debug_level)
        EC_DBG("Finished Sdo request for slave %u.\n",
                fsm->slave->ring_position);

    // check for another Sdo request
    if (ec_fsm_master_action_process_sdo(fsm))
        return; // processing another request

    ec_fsm_master_restart(fsm);
}

/*****************************************************************************/
