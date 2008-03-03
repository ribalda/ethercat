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
   EtherCAT slave configuration state machine.
*/

/*****************************************************************************/

#include "globals.h"
#include "master.h"
#include "mailbox.h"
#include "slave_config.h"
#include "fsm_slave_config.h"

/*****************************************************************************/

void ec_fsm_slave_config_state_start(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_init(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_clear_fmmus(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_mbox_sync(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_preop(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_sdo_conf(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_mapping(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_pdo_conf(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_pdo_sync(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_fmmu(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_safeop(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_op(ec_fsm_slave_config_t *);

void ec_fsm_slave_config_enter_mbox_sync(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_enter_preop(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_enter_sdo_conf(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_enter_mapping(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_enter_pdo_sync(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_enter_fmmu(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_enter_safeop(ec_fsm_slave_config_t *);

void ec_fsm_slave_config_state_end(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_error(ec_fsm_slave_config_t *);

/*****************************************************************************/

/** Constructor.
 */
void ec_fsm_slave_config_init(ec_fsm_slave_config_t *fsm, /**< slave state machine */
        ec_datagram_t *datagram /**< datagram structure to use */
        )
{
    fsm->datagram = datagram;

    // init sub state machines
    ec_fsm_change_init(&fsm->fsm_change, fsm->datagram);
    ec_fsm_coe_init(&fsm->fsm_coe, fsm->datagram);
    ec_fsm_pdo_mapping_init(&fsm->fsm_pdo_map, &fsm->fsm_coe);
    ec_fsm_pdo_config_init(&fsm->fsm_pdo_conf, &fsm->fsm_coe);
}

/*****************************************************************************/

/** Destructor.
 */
void ec_fsm_slave_config_clear(ec_fsm_slave_config_t *fsm /**< slave state machine */)
{
    // clear sub state machines
    ec_fsm_change_clear(&fsm->fsm_change);
    ec_fsm_coe_clear(&fsm->fsm_coe);
    ec_fsm_pdo_mapping_clear(&fsm->fsm_pdo_map);
    ec_fsm_pdo_config_clear(&fsm->fsm_pdo_conf);
}

/*****************************************************************************/

/**
 * Start slave configuration state machine.
 */

void ec_fsm_slave_config_start(ec_fsm_slave_config_t *fsm, /**< slave state machine */
        ec_slave_t *slave /**< slave to configure */
        )
{
    fsm->slave = slave;
    fsm->state = ec_fsm_slave_config_state_start;
}

/*****************************************************************************/

/**
   \return false, if state machine has terminated
*/

int ec_fsm_slave_config_running(const ec_fsm_slave_config_t *fsm /**< slave state machine */)
{
    return fsm->state != ec_fsm_slave_config_state_end
        && fsm->state != ec_fsm_slave_config_state_error;
}

/*****************************************************************************/

/**
   Executes the current state of the state machine.
   If the state machine's datagram is not sent or received yet, the execution
   of the state machine is delayed to the next cycle.
   \return false, if state machine has terminated
*/

int ec_fsm_slave_config_exec(ec_fsm_slave_config_t *fsm /**< slave state machine */)
{
    if (fsm->datagram->state == EC_DATAGRAM_SENT
        || fsm->datagram->state == EC_DATAGRAM_QUEUED) {
        // datagram was not sent or received yet.
        return ec_fsm_slave_config_running(fsm);
    }

    fsm->state(fsm);
    return ec_fsm_slave_config_running(fsm);
}

/*****************************************************************************/

/**
   \return true, if the state machine terminated gracefully
*/

int ec_fsm_slave_config_success(const ec_fsm_slave_config_t *fsm /**< slave state machine */)
{
    return fsm->state == ec_fsm_slave_config_state_end;
}

/******************************************************************************
 * Slave configuration state machine
 *****************************************************************************/

/**
   Slave configuration state: START.
*/

void ec_fsm_slave_config_state_start(ec_fsm_slave_config_t *fsm /**< slave state machine */)
{
    if (fsm->slave->master->debug_level) {
        EC_DBG("Configuring slave %i...\n", fsm->slave->ring_position);
    }

    ec_fsm_change_start(&fsm->fsm_change, fsm->slave, EC_SLAVE_STATE_INIT);
    ec_fsm_change_exec(&fsm->fsm_change);
    fsm->state = ec_fsm_slave_config_state_init;
}

/*****************************************************************************/

/**
   Slave configuration state: INIT.
*/

void ec_fsm_slave_config_state_init(ec_fsm_slave_config_t *fsm /**< slave state machine */)
{
    ec_master_t *master = fsm->slave->master;
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = fsm->datagram;

    if (ec_fsm_change_exec(&fsm->fsm_change)) return;

    if (!ec_fsm_change_success(&fsm->fsm_change)) {
        if (!fsm->fsm_change.spontaneous_change)
            slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        return;
    }

    slave->self_configured = 1;

    if (master->debug_level) {
        EC_DBG("Slave %i is now in INIT.\n", slave->ring_position);
    }

    // check and reset CRC fault counters
    //ec_slave_check_crc(slave);
    // TODO: Implement state machine for CRC checking.

    if (!slave->base_fmmu_count) { // skip FMMU configuration
        ec_fsm_slave_config_enter_mbox_sync(fsm);
        return;
    }

    if (master->debug_level)
        EC_DBG("Clearing FMMU configurations of slave %i...\n",
               slave->ring_position);

    // clear FMMU configurations
    ec_datagram_fpwr(datagram, slave->station_address,
            0x0600, EC_FMMU_PAGE_SIZE * slave->base_fmmu_count);
    memset(datagram->data, 0x00, EC_FMMU_PAGE_SIZE * slave->base_fmmu_count);
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_config_state_clear_fmmus;
}

/*****************************************************************************/

/**
   Slave configuration state: CLEAR FMMU.
*/

void ec_fsm_slave_config_state_clear_fmmus(ec_fsm_slave_config_t *fsm
                                        /**< slave state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_config_state_error;
        EC_ERR("Failed receive FMMU clearing datagram for slave %i.\n",
               fsm->slave->ring_position);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        EC_ERR("Failed to clear FMMUs of slave %i: ",
               fsm->slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    ec_fsm_slave_config_enter_mbox_sync(fsm);
}

/*****************************************************************************/

/**
 * Check for mailbox sync managers to be configured.
 */

void ec_fsm_slave_config_enter_mbox_sync(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    ec_master_t *master = fsm->slave->master;
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = fsm->datagram;
    unsigned int i;

    // slave is now in INIT
    if (slave->current_state == slave->requested_state) {
        fsm->state = ec_fsm_slave_config_state_end; // successful
        if (master->debug_level) {
            EC_DBG("Finished configuration of slave %i.\n",
                   slave->ring_position);
        }
        return;
    }

    if (!slave->sii.mailbox_protocols) {
        // no mailbox protocols supported
        if (master->debug_level)
            EC_DBG("Slave %u does not support mailbox communication.\n",
                    slave->ring_position);
        ec_fsm_slave_config_enter_preop(fsm);
        return;
    }

    if (master->debug_level) {
        EC_DBG("Configuring mailbox sync managers of slave %i.\n",
               slave->ring_position);
    }

    if (slave->sii.sync_count >= 2) { // mailbox configuration provided
        ec_datagram_fpwr(datagram, slave->station_address, 0x0800,
                EC_SYNC_PAGE_SIZE * slave->sii.sync_count);
        memset(datagram->data, 0x00,
                EC_SYNC_PAGE_SIZE * slave->sii.sync_count);

        for (i = 0; i < 2; i++) {
            ec_sync_config(&slave->sii.syncs[i], slave->sii.syncs[i].length,
                    datagram->data + EC_SYNC_PAGE_SIZE * i);
        }
    } else { // no mailbox sync manager configurations provided
        ec_sync_t sync;

        if (master->debug_level)
            EC_DBG("Slave %u does not provide"
                    " mailbox sync manager configurations.\n",
                    slave->ring_position);

        ec_datagram_fpwr(datagram, slave->station_address, 0x0800,
                EC_SYNC_PAGE_SIZE * 2);
        memset(datagram->data, 0x00, EC_SYNC_PAGE_SIZE * 2);

        ec_sync_init(&sync, slave, 0);
        sync.physical_start_address = slave->sii.rx_mailbox_offset;
        sync.control_register = 0x26;
        sync.enable = 1;
        ec_sync_config(&sync, slave->sii.rx_mailbox_size,
                datagram->data + EC_SYNC_PAGE_SIZE * sync.index);

        ec_sync_init(&sync, slave, 1);
        sync.physical_start_address = slave->sii.tx_mailbox_offset;
        sync.control_register = 0x22;
        sync.enable = 1;
        ec_sync_config(&sync, slave->sii.tx_mailbox_size,
                datagram->data + EC_SYNC_PAGE_SIZE * sync.index);
    }

    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_config_state_mbox_sync;
}

/*****************************************************************************/

/**
   Slave configuration state: SYNC.
*/

void ec_fsm_slave_config_state_mbox_sync(ec_fsm_slave_config_t *fsm /**< slave state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_config_state_error;
        EC_ERR("Failed to receive sync manager configuration datagram for"
               " slave %i (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        EC_ERR("Failed to set sync managers of slave %i: ",
               slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    ec_fsm_slave_config_enter_preop(fsm);
}

/*****************************************************************************/

/**
 * Request PREOP state.
 */

void ec_fsm_slave_config_enter_preop(ec_fsm_slave_config_t *fsm /**< slave state machine */)
{
    fsm->state = ec_fsm_slave_config_state_preop;
    ec_fsm_change_start(&fsm->fsm_change, fsm->slave, EC_SLAVE_STATE_PREOP);
    ec_fsm_change_exec(&fsm->fsm_change); // execute immediately
}

/*****************************************************************************/

/**
   Slave configuration state: PREOP.
*/

void ec_fsm_slave_config_state_preop(ec_fsm_slave_config_t *fsm /**< slave state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = fsm->slave->master;

    if (ec_fsm_change_exec(&fsm->fsm_change)) return;

    if (!ec_fsm_change_success(&fsm->fsm_change)) {
        if (!fsm->fsm_change.spontaneous_change)
            slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        return;
    }

    // slave is now in PREOP
    slave->jiffies_preop = fsm->datagram->jiffies_received;

    if (master->debug_level) {
        EC_DBG("Slave %i is now in PREOP.\n", slave->ring_position);
    }

    if (slave->current_state == slave->requested_state) {
        fsm->state = ec_fsm_slave_config_state_end; // successful
        if (master->debug_level) {
            EC_DBG("Finished configuration of slave %i.\n",
                   slave->ring_position);
        }
        return;
    }

    ec_fsm_slave_config_enter_sdo_conf(fsm);
}

/*****************************************************************************/

/**
 * Check for Sdo configurations to be applied.
 */

void ec_fsm_slave_config_enter_sdo_conf(ec_fsm_slave_config_t *fsm /**< slave state machine */)
{
    ec_slave_t *slave = fsm->slave;

    if (!slave->config) {
        EC_DBG("Slave %u is not configured.\n", slave->ring_position);
        ec_fsm_slave_config_enter_safeop(fsm);
        return;
    }

    // No CoE configuration to be applied?
    if (list_empty(&slave->config->sdo_configs)) { // skip Sdo configuration
        ec_fsm_slave_config_enter_mapping(fsm);
        return;
    }

    // start Sdo configuration
    fsm->state = ec_fsm_slave_config_state_sdo_conf;
    fsm->request = list_entry(fsm->slave->config->sdo_configs.next,
            ec_sdo_request_t, list);
    ec_sdo_request_write(fsm->request);
    ec_fsm_coe_download(&fsm->fsm_coe, fsm->slave, fsm->request);
    ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
}

/*****************************************************************************/

/**
   Slave configuration state: SDO_CONF.
*/

void ec_fsm_slave_config_state_sdo_conf(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    if (ec_fsm_coe_exec(&fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(&fsm->fsm_coe)) {
        EC_ERR("Sdo configuration failed for slave %u.\n",
                fsm->slave->ring_position);
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        return;
    }

    // Another Sdo to configure?
    if (fsm->request->list.next != &fsm->slave->config->sdo_configs) {
        fsm->request = list_entry(fsm->request->list.next, ec_sdo_request_t,
                list);
        ec_sdo_request_write(fsm->request);
        ec_fsm_coe_download(&fsm->fsm_coe, fsm->slave, fsm->request);
        ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
        return;
    }

    // All Sdos are now configured.
    ec_fsm_slave_config_enter_mapping(fsm);
}

/*****************************************************************************/

/**
 * Check for Pdo mappings to be applied.
 */

void ec_fsm_slave_config_enter_mapping(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    // start configuring Pdo mapping
    fsm->state = ec_fsm_slave_config_state_mapping;
    ec_fsm_pdo_mapping_start(&fsm->fsm_pdo_map, fsm->slave);
    ec_fsm_pdo_mapping_exec(&fsm->fsm_pdo_map); // execute immediately
}

/*****************************************************************************/

/**
   Slave configuration state: MAPPING.
*/

void ec_fsm_slave_config_state_mapping(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    if (ec_fsm_pdo_mapping_exec(&fsm->fsm_pdo_map)) return;

    if (!ec_fsm_pdo_mapping_success(&fsm->fsm_pdo_map)) {
        EC_ERR("Pdo mapping configuration failed for slave %u.\n",
                fsm->slave->ring_position);
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        return;
    }

    // Start Pdo configuration
    fsm->state = ec_fsm_slave_config_state_pdo_conf;
    ec_fsm_pdo_config_start(&fsm->fsm_pdo_conf, fsm->slave);
    ec_fsm_pdo_config_exec(&fsm->fsm_pdo_conf); // execute immediately
}

/*****************************************************************************/

/**
   Slave configuration state: PDO_CONF.
*/

void ec_fsm_slave_config_state_pdo_conf(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    if (ec_fsm_pdo_config_exec(&fsm->fsm_pdo_conf)) return;

    if (!ec_fsm_pdo_config_success(&fsm->fsm_pdo_conf)) {
        EC_ERR("Pdo configuration failed for slave %u.\n",
                fsm->slave->ring_position);
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        return;
    }

    ec_fsm_slave_config_enter_pdo_sync(fsm);
}

/*****************************************************************************/

/**
 * Check for Pdo sync managers to be configured.
 */

void ec_fsm_slave_config_enter_pdo_sync(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = fsm->datagram;
    unsigned int i, offset, num_syncs;
    const ec_sync_t *sync;
    ec_direction_t dir;
    uint16_t size;

    if (!slave->sii.sync_count) {
        ec_fsm_slave_config_enter_fmmu(fsm);
        return;
    }

    if (slave->sii.mailbox_protocols) {
        offset = 2; // slave has mailboxes
    } else {
        offset = 0;
    }
    num_syncs = slave->sii.sync_count - offset;

    // configure sync managers for process data
    ec_datagram_fpwr(datagram, slave->station_address,
            0x0800 + EC_SYNC_PAGE_SIZE * offset,
            EC_SYNC_PAGE_SIZE * num_syncs);
    memset(datagram->data, 0x00, EC_SYNC_PAGE_SIZE * num_syncs);

    for (i = 0; i < num_syncs; i++) {
        sync = &slave->sii.syncs[i + offset];
        dir = ec_sync_direction(sync);
        size = ec_pdo_mapping_total_size(&slave->config->mapping[dir]);
        ec_sync_config(sync, size, datagram->data + EC_SYNC_PAGE_SIZE * i);
    }

    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_config_state_pdo_sync;
}

/*****************************************************************************/

/**
 * Configure Pdo sync managers.
 */

void ec_fsm_slave_config_state_pdo_sync(ec_fsm_slave_config_t *fsm /**< slave state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_config_state_error;
        EC_ERR("Failed to receive process data sync manager configuration"
               " datagram for slave %i (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        EC_ERR("Failed to set process data sync managers of slave %i: ",
                slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    ec_fsm_slave_config_enter_fmmu(fsm);
}

/*****************************************************************************/

/**
 * Check for FMMUs to be configured.
 */

void ec_fsm_slave_config_enter_fmmu(ec_fsm_slave_config_t *fsm /**< slave state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = fsm->datagram;
    unsigned int i;
    const ec_fmmu_config_t *fmmu;
    const ec_sync_t *sync;

    if (slave->base_fmmu_count < slave->config->used_fmmus) {
        slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        EC_ERR("Slave %u has less FMMUs (%u) than requested (%u).\n",
                slave->ring_position, slave->base_fmmu_count,
                slave->config->used_fmmus);
        return;
    }

    if (!slave->base_fmmu_count) { // skip FMMU configuration
        ec_fsm_slave_config_enter_safeop(fsm);
        return;
    }

    // configure FMMUs
    ec_datagram_fpwr(datagram, slave->station_address,
                     0x0600, EC_FMMU_PAGE_SIZE * slave->base_fmmu_count);
    memset(datagram->data, 0x00, EC_FMMU_PAGE_SIZE * slave->base_fmmu_count);
    for (i = 0; i < slave->config->used_fmmus; i++) {
        fmmu = &slave->config->fmmu_configs[i];
        if (!(sync = ec_slave_get_pdo_sync(slave, fmmu->dir))) {
            slave->error_flag = 1;
            fsm->state = ec_fsm_slave_config_state_error;
            EC_ERR("Failed to determine Pdo sync manager for FMMU on slave"
                    " %u!\n", slave->ring_position);
            return;
        }
        ec_fmmu_config_page(fmmu, sync,
                datagram->data + EC_FMMU_PAGE_SIZE * i);
    }

    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_config_state_fmmu;
}

/*****************************************************************************/

/**
   Slave configuration state: FMMU.
*/

void ec_fsm_slave_config_state_fmmu(ec_fsm_slave_config_t *fsm /**< slave state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_config_state_error;
        EC_ERR("Failed to receive FMMUs datagram for slave %i"
                " (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        EC_ERR("Failed to set FMMUs of slave %i: ",
               slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    ec_fsm_slave_config_enter_safeop(fsm);
}

/*****************************************************************************/

/**
 * Request SAFEOP state.
 */

void ec_fsm_slave_config_enter_safeop(ec_fsm_slave_config_t *fsm /**< slave state machine */)
{
    fsm->state = ec_fsm_slave_config_state_safeop;
    ec_fsm_change_start(&fsm->fsm_change, fsm->slave, EC_SLAVE_STATE_SAFEOP);
    ec_fsm_change_exec(&fsm->fsm_change); // execute immediately
}

/*****************************************************************************/

/**
   Slave configuration state: SAFEOP.
*/

void ec_fsm_slave_config_state_safeop(ec_fsm_slave_config_t *fsm /**< slave state machine */)
{
    ec_master_t *master = fsm->slave->master;
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_change_exec(&fsm->fsm_change)) return;

    if (!ec_fsm_change_success(&fsm->fsm_change)) {
        if (!fsm->fsm_change.spontaneous_change)
            fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        return;
    }

    // slave is now in SAFEOP

    if (master->debug_level) {
        EC_DBG("Slave %i is now in SAFEOP.\n", slave->ring_position);
    }

    if (fsm->slave->current_state == fsm->slave->requested_state) {
        fsm->state = ec_fsm_slave_config_state_end; // successful
        if (master->debug_level) {
            EC_DBG("Finished configuration of slave %i.\n",
                   slave->ring_position);
        }
        return;
    }

    // set state to OP
    fsm->state = ec_fsm_slave_config_state_op;
    ec_fsm_change_start(&fsm->fsm_change, slave, EC_SLAVE_STATE_OP);
    ec_fsm_change_exec(&fsm->fsm_change); // execute immediately
}

/*****************************************************************************/

/**
   Slave configuration state: OP
*/

void ec_fsm_slave_config_state_op(ec_fsm_slave_config_t *fsm /**< slave state machine */)
{
    ec_master_t *master = fsm->slave->master;
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_change_exec(&fsm->fsm_change)) return;

    if (!ec_fsm_change_success(&fsm->fsm_change)) {
        if (!fsm->fsm_change.spontaneous_change)
            slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        return;
    }

    // slave is now in OP

    if (master->debug_level) {
        EC_DBG("Slave %i is now in OP.\n", slave->ring_position);
        EC_DBG("Finished configuration of slave %i.\n", slave->ring_position);
    }

    fsm->state = ec_fsm_slave_config_state_end; // successful
}

/******************************************************************************
 *  Common state functions
 *****************************************************************************/

/**
   State: ERROR.
*/

void ec_fsm_slave_config_state_error(ec_fsm_slave_config_t *fsm /**< slave state machine */)
{
}

/*****************************************************************************/

/**
   State: END.
*/

void ec_fsm_slave_config_state_end(ec_fsm_slave_config_t *fsm /**< slave state machine */)
{
}

/*****************************************************************************/
