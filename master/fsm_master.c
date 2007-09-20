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
   EtherCAT finite state machines.
*/

/*****************************************************************************/

#include "globals.h"
#include "master.h"
#include "mailbox.h"
#ifdef EC_EOE
#include "ethernet.h"
#endif
#include "fsm_master.h"

/*****************************************************************************/

void ec_fsm_master_state_start(ec_fsm_master_t *);
void ec_fsm_master_state_broadcast(ec_fsm_master_t *);
void ec_fsm_master_state_read_states(ec_fsm_master_t *);
void ec_fsm_master_state_acknowledge(ec_fsm_master_t *);
void ec_fsm_master_state_validate_vendor(ec_fsm_master_t *);
void ec_fsm_master_state_validate_product(ec_fsm_master_t *);
void ec_fsm_master_state_rewrite_addresses(ec_fsm_master_t *);
void ec_fsm_master_state_configure_slave(ec_fsm_master_t *);
void ec_fsm_master_state_clear_addresses(ec_fsm_master_t *);
void ec_fsm_master_state_scan_slaves(ec_fsm_master_t *);
void ec_fsm_master_state_write_eeprom(ec_fsm_master_t *);
void ec_fsm_master_state_sdodict(ec_fsm_master_t *);
void ec_fsm_master_state_pdomap(ec_fsm_master_t *);
void ec_fsm_master_state_sdo_request(ec_fsm_master_t *);
void ec_fsm_master_state_end(ec_fsm_master_t *);
void ec_fsm_master_state_error(ec_fsm_master_t *);

/*****************************************************************************/

/**
   Constructor.
*/

void ec_fsm_master_init(ec_fsm_master_t *fsm, /**< master state machine */
        ec_master_t *master, /**< EtherCAT master */
        ec_datagram_t *datagram /**< datagram object to use */
        )
{
    fsm->master = master;
    fsm->datagram = datagram;
    fsm->state = ec_fsm_master_state_start;
    fsm->idle = 0;
    fsm->slaves_responding = 0;
    fsm->topology_change_pending = 0;
    fsm->slave_states = EC_SLAVE_STATE_UNKNOWN;
    fsm->validate = 0;
    fsm->tainted = 0;

    // init sub-state-machines
    ec_fsm_slave_init(&fsm->fsm_slave, fsm->datagram);
    ec_fsm_sii_init(&fsm->fsm_sii, fsm->datagram);
    ec_fsm_change_init(&fsm->fsm_change, fsm->datagram);
    ec_fsm_coe_init(&fsm->fsm_coe, fsm->datagram);
    ec_fsm_coe_map_init(&fsm->fsm_coe_map, &fsm->fsm_coe);
}

/*****************************************************************************/

/**
   Destructor.
*/

void ec_fsm_master_clear(ec_fsm_master_t *fsm /**< master state machine */)
{
    // clear sub-state machines
    ec_fsm_slave_clear(&fsm->fsm_slave);
    ec_fsm_sii_clear(&fsm->fsm_sii);
    ec_fsm_change_clear(&fsm->fsm_change);
    ec_fsm_coe_clear(&fsm->fsm_coe);
    ec_fsm_coe_map_clear(&fsm->fsm_coe_map);
}

/*****************************************************************************/

/**
   Executes the current state of the state machine.
   If the state machine's datagram is not sent or received yet, the execution
   of the state machine is delayed to the next cycle.
   \return false, if state machine has terminated
*/

int ec_fsm_master_exec(ec_fsm_master_t *fsm /**< master state machine */)
{
    if (fsm->datagram->state == EC_DATAGRAM_SENT
        || fsm->datagram->state == EC_DATAGRAM_QUEUED) {
        // datagram was not sent or received yet.
        return ec_fsm_master_running(fsm);
    }

    fsm->state(fsm);
    return ec_fsm_master_running(fsm);
}

/*****************************************************************************/

/**
 * \return false, if state machine has terminated
 */

int ec_fsm_master_running(
        const ec_fsm_master_t *fsm /**< master state machine */
        )
{
    return fsm->state != ec_fsm_master_state_end
        && fsm->state != ec_fsm_master_state_error;
}

/*****************************************************************************/

/**
 * \return true, if the state machine is in an idle phase
 */

int ec_fsm_master_idle(
        const ec_fsm_master_t *fsm /**< master state machine */
        )
{
    return fsm->idle;
}

/******************************************************************************
 *  master state machine
 *****************************************************************************/

/**
   Master state: START.
   Starts with getting slave count and slave states.
*/

void ec_fsm_master_state_start(ec_fsm_master_t *fsm)
{
    fsm->idle = 1;
    ec_datagram_brd(fsm->datagram, 0x0130, 2);
    fsm->state = ec_fsm_master_state_broadcast;
}

/*****************************************************************************/

/**
   Master state: BROADCAST.
   Processes the broadcast read slave count and slaves states.
*/

void ec_fsm_master_state_broadcast(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    unsigned int i;
    ec_slave_t *slave;
    ec_master_t *master = fsm->master;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT)
        return; // always retry

    if (datagram->state != EC_DATAGRAM_RECEIVED) { // link is down
        fsm->slaves_responding = 0;
        list_for_each_entry(slave, &master->slaves, list) {
            ec_slave_set_online_state(slave, EC_SLAVE_OFFLINE);
        }
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    // bus topology change?
    if (datagram->working_counter != fsm->slaves_responding) {
        fsm->topology_change_pending = 1;
        fsm->slaves_responding = datagram->working_counter;

        EC_INFO("%i slave%s responding.\n",
                fsm->slaves_responding,
                fsm->slaves_responding == 1 ? "" : "s");

        if (master->mode == EC_MASTER_MODE_OPERATION) {
            if (fsm->slaves_responding == master->slave_count) {
                fsm->validate = 1; // start validation later
            }
            else {
                EC_WARN("Invalid slave count. Bus in tainted state.\n");
                fsm->tainted = 1;
            }
        }
    }

    // slave states changed?
    if (EC_READ_U8(datagram->data) != fsm->slave_states) {
        char states[EC_STATE_STRING_SIZE];
        fsm->slave_states = EC_READ_U8(datagram->data);
        ec_state_string(fsm->slave_states, states);
        EC_INFO("Slave states: %s.\n", states);
    }

    if (fsm->topology_change_pending) {
        down(&master->scan_sem);
        if (!master->allow_scan) {
            up(&master->scan_sem);
        }
        else {
            master->scan_state = EC_REQUEST_IN_PROGRESS;
            up(&master->scan_sem);
            
            // topology change when scan is allowed:
            // clear all slaves and scan the bus
            fsm->topology_change_pending = 0;
            fsm->tainted = 0;
            fsm->idle = 0;
            fsm->scan_jiffies = jiffies;

#ifdef EC_EOE
            ec_master_eoe_stop(master);
            ec_master_clear_eoe_handlers(master);
#endif
            ec_master_destroy_slaves(master);

            master->slave_count = datagram->working_counter;

            if (!master->slave_count) {
                // no slaves present -> finish state machine.
                master->scan_state = EC_REQUEST_COMPLETE;
                wake_up_interruptible(&master->scan_queue);
                fsm->state = ec_fsm_master_state_end;
                return;
            }

            // init slaves
            for (i = 0; i < master->slave_count; i++) {
                if (!(slave = (ec_slave_t *) kmalloc(sizeof(ec_slave_t),
                                GFP_ATOMIC))) {
                    EC_ERR("Failed to allocate slave %i!\n", i);
                    ec_master_destroy_slaves(master);
                    master->scan_state = EC_REQUEST_FAILURE;
                    wake_up_interruptible(&master->scan_queue);
                    fsm->state = ec_fsm_master_state_error;
                    return;
                }

                if (ec_slave_init(slave, master, i, i + 1)) {
                    // freeing of "slave" already done
                    ec_master_destroy_slaves(master);
                    master->scan_state = EC_REQUEST_FAILURE;
                    wake_up_interruptible(&master->scan_queue);
                    fsm->state = ec_fsm_master_state_error;
                    return;
                }

                list_add_tail(&slave->list, &master->slaves);
            }

            if (master->debug_level)
                EC_DBG("Clearing station addresses...\n");

            ec_datagram_bwr(datagram, 0x0010, 2);
            EC_WRITE_U16(datagram->data, 0x0000);
            fsm->retries = EC_FSM_RETRIES;
            fsm->state = ec_fsm_master_state_clear_addresses;
            return;
        }
    }

    // fetch state from each slave
    fsm->slave = list_entry(master->slaves.next, ec_slave_t, list);
    ec_datagram_nprd(fsm->datagram, fsm->slave->station_address, 0x0130, 2);
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_master_state_read_states;
}

/*****************************************************************************/

/**
 * Check for pending EEPROM write requests and process one.
 * \return non-zero, if an EEPROM write request is processed.
 */

int ec_fsm_master_action_process_eeprom(
        ec_fsm_master_t *fsm /**< master state machine */
        )
{
    ec_master_t *master = fsm->master;
    ec_eeprom_write_request_t *request;
    ec_slave_t *slave;

    // search the first request to be processed
    while (1) {
        down(&master->eeprom_sem);
        if (list_empty(&master->eeprom_requests)) {
            up(&master->eeprom_sem);
            break;
        }
        // get first request
        request = list_entry(master->eeprom_requests.next,
                ec_eeprom_write_request_t, list);
        list_del_init(&request->list); // dequeue
        request->state = EC_REQUEST_IN_PROGRESS;
        up(&master->eeprom_sem);

        slave = request->slave;
        if (slave->online_state == EC_SLAVE_OFFLINE) {
            EC_ERR("Discarding EEPROM data, slave %i offline.\n",
                    slave->ring_position);
            request->state = EC_REQUEST_FAILURE;
            wake_up(&master->eeprom_queue);
            continue;
        }

        // found pending EEPROM write operation. execute it!
        if (master->debug_level)
            EC_DBG("Writing EEPROM data to slave %i...\n",
                    slave->ring_position);
        fsm->eeprom_request = request;
        fsm->eeprom_index = 0;
        ec_fsm_sii_write(&fsm->fsm_sii, request->slave, request->offset,
                request->words, EC_FSM_SII_NODE);
        fsm->state = ec_fsm_master_state_write_eeprom;
        fsm->state(fsm); // execute immediately
        return 1;
    }

    return 0;
}

/*****************************************************************************/

/**
 * Check for pending SDO requests and process one.
 * \return non-zero, if an SDO request is processed.
 */

int ec_fsm_master_action_process_sdo(
        ec_fsm_master_t *fsm /**< master state machine */
        )
{
    ec_master_t *master = fsm->master;
    ec_sdo_request_t *request;
    ec_slave_t *slave;

    // search the first request to be processed
    while (1) {
        down(&master->sdo_sem);
        if (list_empty(&master->sdo_requests)) {
            up(&master->sdo_sem);
            break;
        }
        // get first request
        request =
            list_entry(master->sdo_requests.next, ec_sdo_request_t, list);
        list_del_init(&request->list); // dequeue
        request->state = EC_REQUEST_IN_PROGRESS;
        up(&master->sdo_sem);

        slave = request->entry->sdo->slave;
        if (slave->current_state == EC_SLAVE_STATE_INIT ||
                slave->online_state == EC_SLAVE_OFFLINE ||
                slave->error_flag) {
            EC_ERR("Discarding SDO request, slave %i not ready.\n",
                    slave->ring_position);
            request->state = EC_REQUEST_FAILURE;
            wake_up(&master->sdo_queue);
            continue;
        }

        // found pending SDO request. execute it!
        if (master->debug_level)
            EC_DBG("Processing SDO request for slave %i...\n",
                    slave->ring_position);

        // start uploading SDO
        fsm->idle = 0;
        fsm->slave = slave;
        fsm->sdo_request = request;
        fsm->state = ec_fsm_master_state_sdo_request;
        ec_fsm_coe_upload(&fsm->fsm_coe, slave, fsm->sdo_request);
        ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
        return 1;
    }

    return 0;
}

/*****************************************************************************/

/**
 */

int ec_fsm_master_action_configure(
        ec_fsm_master_t *fsm /**< master state machine */
        )
{
    ec_slave_t *slave;
    ec_master_t *master = fsm->master;
    char old_state[EC_STATE_STRING_SIZE], new_state[EC_STATE_STRING_SIZE];

    // check if any slaves are not in the state, they're supposed to be
    // FIXME do not check all slaves in every cycle...
    list_for_each_entry(slave, &master->slaves, list) {
        if (slave->error_flag
                || slave->online_state == EC_SLAVE_OFFLINE
                || slave->requested_state == EC_SLAVE_STATE_UNKNOWN
                || (slave->current_state == slave->requested_state
                    && slave->self_configured)) continue;

        if (master->debug_level) {
            ec_state_string(slave->current_state, old_state);
            if (slave->current_state != slave->requested_state) {
                ec_state_string(slave->requested_state, new_state);
                EC_DBG("Changing state of slave %i (%s -> %s).\n",
                        slave->ring_position, old_state, new_state);
            }
            else if (!slave->self_configured) {
                EC_DBG("Reconfiguring slave %i (%s).\n",
                        slave->ring_position, old_state);
            }
        }

        fsm->idle = 0;
        fsm->slave = slave;
        fsm->state = ec_fsm_master_state_configure_slave;
        ec_fsm_slave_start_conf(&fsm->fsm_slave, slave);
        ec_fsm_slave_exec(&fsm->fsm_slave); // execute immediately
        return 1;
    }

    if (fsm->config_error)
        master->config_state = EC_REQUEST_FAILURE;
    else
        master->config_state = EC_REQUEST_COMPLETE;
    wake_up_interruptible(&master->config_queue);
    return 0;
}

/*****************************************************************************/

/**
   Master action: PROC_STATES.
   Processes the slave states.
*/

void ec_fsm_master_action_process_states(ec_fsm_master_t *fsm
                                         /**< master state machine */
                                         )
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave;

    down(&master->config_sem);
    if (!master->allow_config) {
        up(&master->config_sem);
    }
    else {
        master->config_state = EC_REQUEST_IN_PROGRESS;
        fsm->config_error = 0;
        up(&master->config_sem);
        
        // check for pending slave configurations
        if (ec_fsm_master_action_configure(fsm))
            return;
    }

    // Check for a pending SDO request
    if (ec_fsm_master_action_process_sdo(fsm))
        return;

    if (master->mode == EC_MASTER_MODE_IDLE) {

        // check, if slaves have an SDO dictionary to read out.
        list_for_each_entry(slave, &master->slaves, list) {
            if (!(slave->sii_mailbox_protocols & EC_MBOX_COE)
                || slave->sdo_dictionary_fetched
                || slave->current_state == EC_SLAVE_STATE_INIT
                || jiffies - slave->jiffies_preop < EC_WAIT_SDO_DICT * HZ
                || slave->online_state == EC_SLAVE_OFFLINE
                || slave->error_flag) continue;

            if (master->debug_level) {
                EC_DBG("Fetching SDO dictionary from slave %i.\n",
                       slave->ring_position);
            }

            slave->sdo_dictionary_fetched = 1;

            // start fetching SDO dictionary
            fsm->idle = 0;
            fsm->slave = slave;
            fsm->state = ec_fsm_master_state_sdodict;
            ec_fsm_coe_dictionary(&fsm->fsm_coe, slave);
            ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
            return;
        }

        // check, if slaves have their PDO mapping to be read.
        list_for_each_entry(slave, &master->slaves, list) {
            if (!(slave->sii_mailbox_protocols & EC_MBOX_COE)
                || slave->pdo_mapping_fetched
                || !slave->sdo_dictionary_fetched
                || slave->current_state == EC_SLAVE_STATE_INIT
                || slave->online_state == EC_SLAVE_OFFLINE) continue;

            if (master->debug_level) {
                EC_DBG("Fetching PDO mapping from slave %i via CoE.\n",
                       slave->ring_position);
            }

            slave->pdo_mapping_fetched = 1;

            // start fetching PDO mapping
            fsm->idle = 0;
            fsm->state = ec_fsm_master_state_pdomap;
            ec_fsm_coe_map_start(&fsm->fsm_coe_map, slave);
            ec_fsm_coe_map_exec(&fsm->fsm_coe_map); // execute immediately
            return;
        }

        // check for pending EEPROM write operations.
        if (ec_fsm_master_action_process_eeprom(fsm))
            return; // EEPROM write request found
    }

    fsm->state = ec_fsm_master_state_end;
}

/*****************************************************************************/

/**
   Master action: Get state of next slave.
*/

void ec_fsm_master_action_next_slave_state(ec_fsm_master_t *fsm
                                           /**< master state machine */)
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave = fsm->slave;

    // is there another slave to query?
    if (slave->list.next != &master->slaves) {
        // process next slave
        fsm->idle = 1;
        fsm->slave = list_entry(slave->list.next, ec_slave_t, list);
        ec_datagram_nprd(fsm->datagram, fsm->slave->station_address,
                         0x0130, 2);
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_master_state_read_states;
        return;
    }

    // all slave states read

    // check, if a bus validation has to be done
    if (fsm->validate) {
        fsm->validate = 0;
        list_for_each_entry(slave, &master->slaves, list) {
            if (slave->online_state == EC_SLAVE_ONLINE) continue;

            // At least one slave is offline. validate!
            EC_INFO("Validating bus.\n");
            fsm->idle = 0;
            fsm->slave = list_entry(master->slaves.next, ec_slave_t, list);
            fsm->state = ec_fsm_master_state_validate_vendor;
            ec_fsm_sii_read(&fsm->fsm_sii, slave, 0x0008, EC_FSM_SII_POSITION);
            ec_fsm_sii_exec(&fsm->fsm_sii); // execute immediately
            return;
        }
    }

    ec_fsm_master_action_process_states(fsm);
}

/*****************************************************************************/

/**
   Master state: READ STATES.
   Fetches the AL- and online state of a slave.
*/

void ec_fsm_master_state_read_states(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = fsm->datagram;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        EC_ERR("Failed to receive AL state datagram for slave %i"
                " (datagram state %i)\n",
                slave->ring_position, datagram->state);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    // did the slave not respond to its station address?
    if (datagram->working_counter == 0) {
        ec_slave_set_online_state(slave, EC_SLAVE_OFFLINE);
        ec_fsm_master_action_next_slave_state(fsm);
        return;
    }

    // FIXME what to to on multiple response?

    // slave responded
    ec_slave_set_state(slave, EC_READ_U8(datagram->data)); // set app state first
    ec_slave_set_online_state(slave, EC_SLAVE_ONLINE);

    // check, if new slave state has to be acknowledged
    if (slave->current_state & EC_SLAVE_STATE_ACK_ERR && !slave->error_flag) {
        fsm->idle = 0;
        fsm->state = ec_fsm_master_state_acknowledge;
        ec_fsm_change_ack(&fsm->fsm_change, slave);
        ec_fsm_change_exec(&fsm->fsm_change);
        return;
    }

    ec_fsm_master_action_next_slave_state(fsm);
}

/*****************************************************************************/

/**
   Master state: ACKNOWLEDGE
*/

void ec_fsm_master_state_acknowledge(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_change_exec(&fsm->fsm_change)) return;

    if (!ec_fsm_change_success(&fsm->fsm_change)) {
        fsm->slave->error_flag = 1;
        EC_ERR("Failed to acknowledge state change on slave %i.\n",
               slave->ring_position);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    ec_fsm_master_action_next_slave_state(fsm);
}

/*****************************************************************************/

/**
   Master state: VALIDATE_VENDOR.
   Validates the vendor ID of a slave.
*/

void ec_fsm_master_state_validate_vendor(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_sii_exec(&fsm->fsm_sii)) return;

    if (!ec_fsm_sii_success(&fsm->fsm_sii)) {
        fsm->slave->error_flag = 1;
        EC_ERR("Failed to validate vendor ID of slave %i.\n",
               slave->ring_position);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    if (EC_READ_U32(fsm->fsm_sii.value) != slave->sii_vendor_id) {
        EC_ERR("Slave %i has an invalid vendor ID!\n", slave->ring_position);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    // vendor ID is ok. check product code.
    fsm->state = ec_fsm_master_state_validate_product;
    ec_fsm_sii_read(&fsm->fsm_sii, slave, 0x000A, EC_FSM_SII_POSITION);
    ec_fsm_sii_exec(&fsm->fsm_sii); // execute immediately
}

/*****************************************************************************/

/**
   Master action: ADDRESS.
   Looks for slave, that have lost their configuration and writes
   their station address, so that they can be reconfigured later.
*/

void ec_fsm_master_action_addresses(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;

    while (fsm->slave->online_state == EC_SLAVE_ONLINE) {
        if (fsm->slave->list.next == &fsm->master->slaves) { // last slave?
            fsm->state = ec_fsm_master_state_end;
            return;
        }
        // check next slave
        fsm->slave = list_entry(fsm->slave->list.next, ec_slave_t, list);
    }

    if (fsm->master->debug_level)
        EC_DBG("Reinitializing slave %i.\n", fsm->slave->ring_position);

    // write station address
    ec_datagram_apwr(datagram, fsm->slave->ring_position, 0x0010, 2);
    EC_WRITE_U16(datagram->data, fsm->slave->station_address);
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_master_state_rewrite_addresses;
}

/*****************************************************************************/

/**
   Master state: VALIDATE_PRODUCT.
   Validates the product ID of a slave.
*/

void ec_fsm_master_state_validate_product(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_sii_exec(&fsm->fsm_sii)) return;

    if (!ec_fsm_sii_success(&fsm->fsm_sii)) {
        fsm->slave->error_flag = 1;
        EC_ERR("Failed to validate product code of slave %i.\n",
               slave->ring_position);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    if (EC_READ_U32(fsm->fsm_sii.value) != slave->sii_product_code) {
        EC_ERR("Slave %i: invalid product code!\n", slave->ring_position);
        EC_ERR("expected 0x%08X, got 0x%08X.\n", slave->sii_product_code,
               EC_READ_U32(fsm->fsm_sii.value));
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    // have all states been validated?
    if (slave->list.next == &fsm->master->slaves) {
        fsm->topology_change_pending = 0;
        fsm->tainted = 0;
        fsm->slave = list_entry(fsm->master->slaves.next, ec_slave_t, list);
        // start writing addresses to offline slaves
        ec_fsm_master_action_addresses(fsm);
        return;
    }

    // validate next slave
    fsm->slave = list_entry(fsm->slave->list.next, ec_slave_t, list);
    fsm->state = ec_fsm_master_state_validate_vendor;
    ec_fsm_sii_read(&fsm->fsm_sii, slave, 0x0008, EC_FSM_SII_POSITION);
    ec_fsm_sii_exec(&fsm->fsm_sii); // execute immediately
}

/*****************************************************************************/

/**
   Master state: REWRITE ADDRESS.
   Checks, if the new station address has been written to the slave.
*/

void ec_fsm_master_state_rewrite_addresses(ec_fsm_master_t *fsm
                                     /**< master state machine */
                                     )
{
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = fsm->datagram;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        EC_ERR("Failed to receive address datagram for slave %i"
                " (datagram state %i).\n",
                slave->ring_position, datagram->state);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    if (datagram->working_counter != 1) {
        EC_ERR("Failed to write station address of slave %i: ",
               slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    if (fsm->slave->list.next == &fsm->master->slaves) { // last slave?
        fsm->state = ec_fsm_master_state_end;
        return;
    }

    // check next slave
    fsm->slave = list_entry(fsm->slave->list.next, ec_slave_t, list);
    // Write new station address to slave
    ec_fsm_master_action_addresses(fsm);
}

/*****************************************************************************/

/**
 * Master state: CLEAR ADDRESSES.
 */

void ec_fsm_master_state_clear_addresses(
        ec_fsm_master_t *fsm /**< master state machine */
        )
{
    ec_master_t *master = fsm->master;
    ec_datagram_t *datagram = fsm->datagram;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        EC_ERR("Failed to receive address clearing datagram (state %i).\n",
                datagram->state);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    if (datagram->working_counter != master->slave_count) {
        EC_WARN("Failed to clear all station addresses: Cleared %u of %u",
                datagram->working_counter, master->slave_count);
    }

    EC_INFO("Scanning bus.\n");

    // begin scanning of slaves
    fsm->slave = list_entry(master->slaves.next, ec_slave_t, list);
    fsm->state = ec_fsm_master_state_scan_slaves;
    ec_fsm_slave_start_scan(&fsm->fsm_slave, fsm->slave);
    ec_fsm_slave_exec(&fsm->fsm_slave); // execute immediately
}

/*****************************************************************************/

/**
 * Master state: SCAN SLAVES.
 * Executes the sub-statemachine for the scanning of a slave.
 */

void ec_fsm_master_state_scan_slaves(
        ec_fsm_master_t *fsm /**< master state machine */
        )
{
    ec_master_t *master = fsm->master;
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_slave_exec(&fsm->fsm_slave)) // execute slave state machine
        return;

#ifdef EC_EOE
    if (slave->sii_mailbox_protocols & EC_MBOX_EOE) {
        // create EoE handler for this slave
        ec_eoe_t *eoe;
        if (!(eoe = kmalloc(sizeof(ec_eoe_t), GFP_KERNEL))) {
            EC_ERR("Failed to allocate EoE handler memory for slave %u!\n",
                    slave->ring_position);
        }
        else if (ec_eoe_init(eoe, slave)) {
            EC_ERR("Failed to init EoE handler for slave %u!\n",
                    slave->ring_position);
            kfree(eoe);
        }
        else {
            list_add_tail(&eoe->list, &master->eoe_handlers);
        }
    }
#endif

    // another slave to fetch?
    if (slave->list.next != &master->slaves) {
        fsm->slave = list_entry(slave->list.next, ec_slave_t, list);
        ec_fsm_slave_start_scan(&fsm->fsm_slave, fsm->slave);
        ec_fsm_slave_exec(&fsm->fsm_slave); // execute immediately
        return;
    }

    EC_INFO("Bus scanning completed in %u ms.\n",
            (u32) (jiffies - fsm->scan_jiffies) * 1000 / HZ);

#ifdef EC_EOE
    // check if EoE processing has to be started
    ec_master_eoe_start(master);
#endif

    master->scan_state = EC_REQUEST_COMPLETE;
    wake_up_interruptible(&master->scan_queue);

    fsm->state = ec_fsm_master_state_end;
}

/*****************************************************************************/

/**
   Master state: CONFIGURE SLAVES.
   Starts configuring a slave.
*/

void ec_fsm_master_state_configure_slave(ec_fsm_master_t *fsm
                                   /**< master state machine */
                                   )
{
    if (ec_fsm_slave_exec(&fsm->fsm_slave)) // execute slave's state machine
        return;

    if (!ec_fsm_slave_success(&fsm->fsm_slave))
        fsm->config_error = 1;

    // configure next slave, if necessary
    if (ec_fsm_master_action_configure(fsm))
        return;

    fsm->state = ec_fsm_master_state_end;
}

/*****************************************************************************/

/**
   Master state: WRITE EEPROM.
*/

void ec_fsm_master_state_write_eeprom(
        ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_master_t *master = fsm->master;
    ec_eeprom_write_request_t *request = fsm->eeprom_request;
    ec_slave_t *slave = request->slave;

    if (ec_fsm_sii_exec(&fsm->fsm_sii)) return;

    if (!ec_fsm_sii_success(&fsm->fsm_sii)) {
        slave->error_flag = 1;
        EC_ERR("Failed to write EEPROM data to slave %i.\n",
                slave->ring_position);
        request->state = EC_REQUEST_FAILURE;
        wake_up(&master->eeprom_queue);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    fsm->eeprom_index++;
    if (fsm->eeprom_index < request->size) {
        ec_fsm_sii_write(&fsm->fsm_sii, slave,
                request->offset + fsm->eeprom_index,
                request->words + fsm->eeprom_index,
                EC_FSM_SII_NODE);
        ec_fsm_sii_exec(&fsm->fsm_sii); // execute immediately
        return;
    }

    // finished writing EEPROM
    if (master->debug_level)
        EC_DBG("Finished writing %u words of EEPROM data to slave %u.\n",
                request->size, slave->ring_position);
    request->state = EC_REQUEST_COMPLETE;
    wake_up(&master->eeprom_queue);

    // TODO: Evaluate new EEPROM contents!

    // check for another EEPROM write request
    if (ec_fsm_master_action_process_eeprom(fsm))
        return; // processing another request

    fsm->state = ec_fsm_master_state_end;
}

/*****************************************************************************/

/**
   Master state: SDODICT.
*/

void ec_fsm_master_state_sdodict(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = fsm->master;

    if (ec_fsm_coe_exec(&fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(&fsm->fsm_coe)) {
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    // SDO dictionary fetching finished

    if (master->debug_level) {
        unsigned int sdo_count, entry_count;
        ec_slave_sdo_dict_info(slave, &sdo_count, &entry_count);
        EC_DBG("Fetched %i SDOs and %i entries from slave %i.\n",
               sdo_count, entry_count, slave->ring_position);
    }

    fsm->state = ec_fsm_master_state_end;
}

/*****************************************************************************/

/**
 */

void ec_fsm_master_state_pdomap(
        ec_fsm_master_t *fsm /**< master state machine */
        )
{
    if (ec_fsm_coe_map_exec(&fsm->fsm_coe_map)) return;

    if (!ec_fsm_coe_map_success(&fsm->fsm_coe_map)) {
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    // fetching of PDO mapping finished
    fsm->state = ec_fsm_master_state_end;
}

/*****************************************************************************/

/**
   Master state: SDO REQUEST.
*/

void ec_fsm_master_state_sdo_request(ec_fsm_master_t *fsm /**< master state machine */)
{
    ec_master_t *master = fsm->master;
    ec_sdo_request_t *request = fsm->sdo_request;

    if (ec_fsm_coe_exec(&fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(&fsm->fsm_coe)) {
        EC_DBG("Failed to process SDO request for slave %i.\n",
                fsm->slave->ring_position);
        request->state = EC_REQUEST_FAILURE;
        wake_up(&master->sdo_queue);
        fsm->state = ec_fsm_master_state_error;
        return;
    }

    // SDO request finished 
    request->state = EC_REQUEST_COMPLETE;
    wake_up(&master->sdo_queue);

    if (master->debug_level)
        EC_DBG("Finished SDO request for slave %i.\n",
                fsm->slave->ring_position);

    // check for another SDO request
    if (ec_fsm_master_action_process_sdo(fsm))
        return; // processing another request

    fsm->state = ec_fsm_master_state_end;
}

/*****************************************************************************/

/**
   State: ERROR.
*/

void ec_fsm_master_state_error(
        ec_fsm_master_t *fsm /**< master state machine */
        )
{
    fsm->state = ec_fsm_master_state_start;
}

/*****************************************************************************/

/**
   State: END.
*/

void ec_fsm_master_state_end(ec_fsm_master_t *fsm /**< master state machine */)
{
    fsm->state = ec_fsm_master_state_start;
}

/*****************************************************************************/

