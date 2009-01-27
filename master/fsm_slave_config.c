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
 *  Using the EtherCAT technology and brand is permitted in compliance with
 *  the industrial property and similar rights of Beckhoff Automation GmbH.
 *
 *****************************************************************************/

/** \file
 *
 * EtherCAT slave configuration state machine.
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
void ec_fsm_slave_config_state_clear_sync(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_mbox_sync(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_boot_preop(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_sdo_conf(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_pdo_sync(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_pdo_conf(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_fmmu(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_safeop(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_op(ec_fsm_slave_config_t *);

void ec_fsm_slave_config_enter_init(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_enter_clear_sync(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_enter_mbox_sync(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_enter_boot_preop(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_enter_sdo_conf(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_enter_pdo_conf(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_enter_pdo_sync(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_enter_fmmu(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_enter_safeop(ec_fsm_slave_config_t *);

void ec_fsm_slave_config_state_end(ec_fsm_slave_config_t *);
void ec_fsm_slave_config_state_error(ec_fsm_slave_config_t *);

void ec_fsm_slave_config_reconfigure(ec_fsm_slave_config_t *);

/*****************************************************************************/

/** Constructor.
 */
void ec_fsm_slave_config_init(
        ec_fsm_slave_config_t *fsm, /**< slave state machine */
        ec_datagram_t *datagram, /**< datagram structure to use */
        ec_fsm_change_t *fsm_change, /**< State change state machine to use. */
        ec_fsm_coe_t *fsm_coe, /**< CoE state machine to use. */
        ec_fsm_pdo_t *fsm_pdo /**< PDO configuration state machine to use. */
        )
{
    ec_sdo_request_init(&fsm->request_copy);

    fsm->datagram = datagram;
    fsm->fsm_change = fsm_change;
    fsm->fsm_coe = fsm_coe;
    fsm->fsm_pdo = fsm_pdo;
}

/*****************************************************************************/

/** Destructor.
 */
void ec_fsm_slave_config_clear(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    ec_sdo_request_clear(&fsm->request_copy);
}

/*****************************************************************************/

/** Start slave configuration state machine.
 */
void ec_fsm_slave_config_start(
        ec_fsm_slave_config_t *fsm, /**< slave state machine */
        ec_slave_t *slave /**< slave to configure */
        )
{
    fsm->slave = slave;
    fsm->state = ec_fsm_slave_config_state_start;
}

/*****************************************************************************/

/**
 * \return false, if state machine has terminated
 */
int ec_fsm_slave_config_running(
        const ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    return fsm->state != ec_fsm_slave_config_state_end
        && fsm->state != ec_fsm_slave_config_state_error;
}

/*****************************************************************************/

/** Executes the current state of the state machine.
 *
 * If the state machine's datagram is not sent or received yet, the execution
 * of the state machine is delayed to the next cycle.
 *
 * \return false, if state machine has terminated
 */
int ec_fsm_slave_config_exec(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
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
 * \return true, if the state machine terminated gracefully
 */
int ec_fsm_slave_config_success(
        const ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    return fsm->state == ec_fsm_slave_config_state_end;
}

/******************************************************************************
 * Slave configuration state machine
 *****************************************************************************/

/** Slave configuration state: START.
 */
void ec_fsm_slave_config_state_start(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    if (fsm->slave->master->debug_level) {
        EC_DBG("Configuring slave %u...\n", fsm->slave->ring_position);
    }
    
    ec_fsm_slave_config_enter_init(fsm);
}

/*****************************************************************************/

/** Start state change to INIT.
 */
void ec_fsm_slave_config_enter_init(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    ec_fsm_change_start(fsm->fsm_change, fsm->slave, EC_SLAVE_STATE_INIT);
    ec_fsm_change_exec(fsm->fsm_change);
    fsm->state = ec_fsm_slave_config_state_init;
}

/*****************************************************************************/

/** Slave configuration state: INIT.
 */
void ec_fsm_slave_config_state_init(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    ec_master_t *master = fsm->slave->master;
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = fsm->datagram;

    if (ec_fsm_change_exec(fsm->fsm_change)) return;

    if (!ec_fsm_change_success(fsm->fsm_change)) {
        if (!fsm->fsm_change->spontaneous_change)
            slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        return;
    }

    if (master->debug_level) {
        EC_DBG("Slave %u is now in INIT.\n", slave->ring_position);
    }

    if (!slave->base_fmmu_count) { // skip FMMU configuration
        ec_fsm_slave_config_enter_clear_sync(fsm);
        return;
    }

    if (master->debug_level)
        EC_DBG("Clearing FMMU configurations of slave %u...\n",
               slave->ring_position);

    // clear FMMU configurations
    ec_datagram_fpwr(datagram, slave->station_address,
            0x0600, EC_FMMU_PAGE_SIZE * slave->base_fmmu_count);
    ec_datagram_zero(datagram);
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_config_state_clear_fmmus;
}

/*****************************************************************************/

/** Slave configuration state: CLEAR FMMU.
 */
void ec_fsm_slave_config_state_clear_fmmus(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    ec_datagram_t *datagram = fsm->datagram;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_config_state_error;
        EC_ERR("Failed receive FMMU clearing datagram for slave %u.\n",
               fsm->slave->ring_position);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        EC_ERR("Failed to clear FMMUs of slave %u: ",
               fsm->slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    ec_fsm_slave_config_enter_clear_sync(fsm);
}

/*****************************************************************************/

/** Clear the sync manager configurations.
 */
void ec_fsm_slave_config_enter_clear_sync(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = fsm->datagram;
    size_t sync_size;

    if (!slave->sii.sync_count) {
        // no mailbox protocols supported
        ec_fsm_slave_config_enter_mbox_sync(fsm);
        return;
    }

    if (slave->master->debug_level)
        EC_DBG("Clearing sync manager configurations of slave %u...\n",
                slave->ring_position);

    sync_size = EC_SYNC_PAGE_SIZE * slave->sii.sync_count;

    // clear sync manager configurations
    ec_datagram_fpwr(datagram, slave->station_address, 0x0800, sync_size);
    memset(datagram->data, 0x00, sync_size);
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_config_state_clear_sync;
}

/*****************************************************************************/

/** Slave configuration state: CLEAR SYNC.
 */
void ec_fsm_slave_config_state_clear_sync(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    ec_datagram_t *datagram = fsm->datagram;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_config_state_error;
        EC_ERR("Failed receive sync manager clearing datagram"
                " for slave %u.\n", fsm->slave->ring_position);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        EC_ERR("Failed to clear sync manager configurations of slave %u: ",
               fsm->slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    ec_fsm_slave_config_enter_mbox_sync(fsm);
}

/*****************************************************************************/

/** Check for mailbox sync managers to be configured.
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
            EC_DBG("Finished configuration of slave %u.\n",
                   slave->ring_position);
        }
        return;
    }

    if (!slave->sii.mailbox_protocols) {
        // no mailbox protocols supported
        if (master->debug_level)
            EC_DBG("Slave %u does not support mailbox communication.\n",
                    slave->ring_position);
        ec_fsm_slave_config_enter_boot_preop(fsm);
        return;
    }

    if (master->debug_level) {
        EC_DBG("Configuring mailbox sync managers of slave %u.\n",
               slave->ring_position);
    }

    if (slave->requested_state == EC_SLAVE_STATE_BOOT) {
        ec_sync_t sync;

        ec_datagram_fpwr(datagram, slave->station_address, 0x0800,
                EC_SYNC_PAGE_SIZE * 2);
        memset(datagram->data, 0x00, EC_SYNC_PAGE_SIZE * 2);

        ec_sync_init(&sync, slave);
        sync.physical_start_address = slave->sii.boot_rx_mailbox_offset;
        sync.control_register = 0x26;
        sync.enable = 1;
        ec_sync_page(&sync, 0, slave->sii.boot_rx_mailbox_size,
                EC_DIR_INVALID, // use default direction
                datagram->data);
        slave->configured_rx_mailbox_offset =
            slave->sii.boot_rx_mailbox_offset;
        slave->configured_rx_mailbox_size =
            slave->sii.boot_rx_mailbox_size;

        ec_sync_init(&sync, slave);
        sync.physical_start_address = slave->sii.boot_tx_mailbox_offset;
        sync.control_register = 0x22;
        sync.enable = 1;
        ec_sync_page(&sync, 1, slave->sii.boot_tx_mailbox_size,
                EC_DIR_INVALID, // use default direction
                datagram->data + EC_SYNC_PAGE_SIZE);
        slave->configured_tx_mailbox_offset =
            slave->sii.boot_tx_mailbox_offset;
        slave->configured_tx_mailbox_size =
            slave->sii.boot_tx_mailbox_size;

    } else if (slave->sii.sync_count >= 2) { // mailbox configuration provided
        ec_datagram_fpwr(datagram, slave->station_address, 0x0800,
                EC_SYNC_PAGE_SIZE * slave->sii.sync_count);
        ec_datagram_zero(datagram);

        for (i = 0; i < 2; i++) {
            ec_sync_page(&slave->sii.syncs[i], i,
                    slave->sii.syncs[i].default_length,
                    EC_DIR_INVALID, // use default direction
                    datagram->data + EC_SYNC_PAGE_SIZE * i);
        }

        slave->configured_rx_mailbox_offset =
            slave->sii.syncs[0].physical_start_address;
        slave->configured_rx_mailbox_size =
            slave->sii.syncs[0].default_length;
        slave->configured_tx_mailbox_offset =
            slave->sii.syncs[1].physical_start_address;
        slave->configured_tx_mailbox_size =
            slave->sii.syncs[1].default_length;
    } else { // no mailbox sync manager configurations provided
        ec_sync_t sync;

        if (master->debug_level)
            EC_DBG("Slave %u does not provide"
                    " mailbox sync manager configurations.\n",
                    slave->ring_position);

        ec_datagram_fpwr(datagram, slave->station_address, 0x0800,
                EC_SYNC_PAGE_SIZE * 2);
        ec_datagram_zero(datagram);

        ec_sync_init(&sync, slave);
        sync.physical_start_address = slave->sii.std_rx_mailbox_offset;
        sync.control_register = 0x26;
        sync.enable = 1;
        ec_sync_page(&sync, 0, slave->sii.std_rx_mailbox_size,
                EC_DIR_INVALID, // use default direction
                datagram->data);
        slave->configured_rx_mailbox_offset =
            slave->sii.std_rx_mailbox_offset;
        slave->configured_rx_mailbox_size =
            slave->sii.std_rx_mailbox_size;

        ec_sync_init(&sync, slave);
        sync.physical_start_address = slave->sii.std_tx_mailbox_offset;
        sync.control_register = 0x22;
        sync.enable = 1;
        ec_sync_page(&sync, 1, slave->sii.std_tx_mailbox_size,
                EC_DIR_INVALID, // use default direction
                datagram->data + EC_SYNC_PAGE_SIZE);
        slave->configured_tx_mailbox_offset =
            slave->sii.boot_tx_mailbox_offset;
        slave->configured_tx_mailbox_size =
            slave->sii.boot_tx_mailbox_size;
    }

    fsm->take_time = 1;

    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_config_state_mbox_sync;
}

/*****************************************************************************/

/** Slave configuration state: SYNC.
 *
 * \todo Timeout for response.
 */
void ec_fsm_slave_config_state_mbox_sync(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_config_state_error;
        EC_ERR("Failed to receive sync manager configuration datagram for"
               " slave %u (datagram state %u).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (fsm->take_time) {
        fsm->take_time = 0;
        fsm->jiffies_start = datagram->jiffies_sent;
    }

    /* Because the sync manager configurations are cleared during the last
     * cycle, some slaves do not immediately respond to the mailbox sync
     * manager configuration datagram. Therefore, resend the datagram for
     * a certain time, if the slave does not respond.
     */
    if (datagram->working_counter == 0) {
        unsigned long diff = datagram->jiffies_received - fsm->jiffies_start;

        if (diff >= HZ) {
            slave->error_flag = 1;
            fsm->state = ec_fsm_slave_config_state_error;
            EC_ERR("Timeout while configuring mailbox sync managers of"
                    " slave %u.\n", slave->ring_position);
            return;
        }
        else if (slave->master->debug_level) {
            EC_DBG("Resending after %u ms...\n",
                    (unsigned int) diff * 1000 / HZ);
        }

        // send configuration datagram again
        fsm->retries = EC_FSM_RETRIES;
        return;
    }
    else if (datagram->working_counter != 1) {
        slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        EC_ERR("Failed to set sync managers of slave %u: ",
               slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    ec_fsm_slave_config_enter_boot_preop(fsm);
}

/*****************************************************************************/

/** Request PREOP state.
 */
void ec_fsm_slave_config_enter_boot_preop(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    fsm->state = ec_fsm_slave_config_state_boot_preop;

    if (fsm->slave->requested_state != EC_SLAVE_STATE_BOOT) {
        ec_fsm_change_start(fsm->fsm_change, fsm->slave, EC_SLAVE_STATE_PREOP);
    } else { // BOOT
        ec_fsm_change_start(fsm->fsm_change, fsm->slave, EC_SLAVE_STATE_BOOT);
    }

    ec_fsm_change_exec(fsm->fsm_change); // execute immediately
}

/*****************************************************************************/

/** Slave configuration state: BOOT/PREOP.
 */
void ec_fsm_slave_config_state_boot_preop(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = fsm->slave->master;

    if (ec_fsm_change_exec(fsm->fsm_change)) return;

    if (!ec_fsm_change_success(fsm->fsm_change)) {
        if (!fsm->fsm_change->spontaneous_change)
            slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        return;
    }

    // slave is now in BOOT/PREOP
    slave->jiffies_preop = fsm->datagram->jiffies_received;

    if (master->debug_level) {
        EC_DBG("Slave %u is now in %s.\n", slave->ring_position,
                slave->requested_state != EC_SLAVE_STATE_BOOT
                ? "PREOP" : "BOOT");
    }

    if (slave->current_state == slave->requested_state) {
        fsm->state = ec_fsm_slave_config_state_end; // successful
        if (master->debug_level) {
            EC_DBG("Finished configuration of slave %u.\n",
                   slave->ring_position);
        }
        return;
    }

    if (!slave->config) {
        EC_DBG("Slave %u is not configured.\n", slave->ring_position);
        ec_fsm_slave_config_enter_safeop(fsm);
        return;
    }

    ec_fsm_slave_config_enter_sdo_conf(fsm);
}

/*****************************************************************************/

/** Check for SDO configurations to be applied.
 */
void ec_fsm_slave_config_enter_sdo_conf(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    ec_slave_t *slave = fsm->slave;

    // No CoE configuration to be applied?
    if (list_empty(&slave->config->sdo_configs)) { // skip SDO configuration
        ec_fsm_slave_config_enter_pdo_conf(fsm);
        return;
    }

    // start SDO configuration
    fsm->state = ec_fsm_slave_config_state_sdo_conf;
    fsm->request = list_entry(fsm->slave->config->sdo_configs.next,
            ec_sdo_request_t, list);
    ec_sdo_request_copy(&fsm->request_copy, fsm->request);
    ecrt_sdo_request_write(&fsm->request_copy);
    ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request_copy);
    ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
}

/*****************************************************************************/

/** Slave configuration state: SDO_CONF.
 */
void ec_fsm_slave_config_state_sdo_conf(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    if (ec_fsm_coe_exec(fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(fsm->fsm_coe)) {
        EC_ERR("SDO configuration failed for slave %u.\n",
                fsm->slave->ring_position);
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        return;
    }

    if (!fsm->slave->config) { // config removed in the meantime
        ec_fsm_slave_config_reconfigure(fsm);
        return;
    }

    // Another SDO to configure?
    if (fsm->request->list.next != &fsm->slave->config->sdo_configs) {
        fsm->request = list_entry(fsm->request->list.next,
                ec_sdo_request_t, list);
        ec_sdo_request_copy(&fsm->request_copy, fsm->request);
        ecrt_sdo_request_write(&fsm->request_copy);
        ec_fsm_coe_transfer(fsm->fsm_coe, fsm->slave, &fsm->request_copy);
        ec_fsm_coe_exec(fsm->fsm_coe); // execute immediately
        return;
    }

    // All SDOs are now configured.
    ec_fsm_slave_config_enter_pdo_conf(fsm);
}

/*****************************************************************************/

/** PDO_CONF entry function.
 */
void ec_fsm_slave_config_enter_pdo_conf(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    // Start configuring PDOs
    ec_fsm_pdo_start_configuration(fsm->fsm_pdo, fsm->slave);
    fsm->state = ec_fsm_slave_config_state_pdo_conf;
    fsm->state(fsm); // execute immediately
}

/*****************************************************************************/

/** Slave configuration state: PDO_CONF.
 */
void ec_fsm_slave_config_state_pdo_conf(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    if (ec_fsm_pdo_exec(fsm->fsm_pdo))
        return;

    if (!fsm->slave->config) { // config removed in the meantime
        ec_fsm_slave_config_reconfigure(fsm);
        return;
    }

    if (!ec_fsm_pdo_success(fsm->fsm_pdo)) {
        EC_WARN("PDO configuration failed on slave %u.\n",
                fsm->slave->ring_position);
    }

    ec_fsm_slave_config_enter_pdo_sync(fsm);
}

/*****************************************************************************/

/** Check for PDO sync managers to be configured.
 */
void ec_fsm_slave_config_enter_pdo_sync(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = fsm->datagram;
    unsigned int i, offset, num_pdo_syncs;
    uint8_t sync_index;
    const ec_sync_t *sync;
    const ec_sync_config_t *sync_config;
    uint16_t size;

    if (slave->sii.mailbox_protocols) {
        offset = 2; // slave has mailboxes
    } else {
        offset = 0;
    }

    if (slave->sii.sync_count <= offset) {
        // no PDO sync managers to configure
        ec_fsm_slave_config_enter_fmmu(fsm);
        return;
    }

    num_pdo_syncs = slave->sii.sync_count - offset;

    // configure sync managers for process data
    ec_datagram_fpwr(datagram, slave->station_address,
            0x0800 + EC_SYNC_PAGE_SIZE * offset,
            EC_SYNC_PAGE_SIZE * num_pdo_syncs);
    ec_datagram_zero(datagram);

    for (i = 0; i < num_pdo_syncs; i++) {
        sync_index = i + offset;
        sync = &slave->sii.syncs[sync_index];
        sync_config = &slave->config->sync_configs[sync_index];
        size = ec_pdo_list_total_size(&sync_config->pdos);
        ec_sync_page(sync, sync_index, size, sync_config->dir,
                datagram->data + EC_SYNC_PAGE_SIZE * i);
    }

    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_config_state_pdo_sync;
}

/*****************************************************************************/

/** Configure PDO sync managers.
 */
void ec_fsm_slave_config_state_pdo_sync(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_config_state_error;
        EC_ERR("Failed to receive process data sync manager configuration"
               " datagram for slave %u (datagram state %u).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        EC_ERR("Failed to set process data sync managers of slave %u: ",
                slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    if (!fsm->slave->config) { // config removed in the meantime
        ec_fsm_slave_config_reconfigure(fsm);
        return;
    }

    ec_fsm_slave_config_enter_fmmu(fsm);
}

/*****************************************************************************/

/** Check for FMMUs to be configured.
 */
void ec_fsm_slave_config_enter_fmmu(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
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
    ec_datagram_zero(datagram);
    for (i = 0; i < slave->config->used_fmmus; i++) {
        fmmu = &slave->config->fmmu_configs[i];
        if (!(sync = ec_slave_get_sync(slave, fmmu->sync_index))) {
            slave->error_flag = 1;
            fsm->state = ec_fsm_slave_config_state_error;
            EC_ERR("Failed to determine PDO sync manager for FMMU on slave"
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

/** Slave configuration state: FMMU.
 */
void ec_fsm_slave_config_state_fmmu(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_config_state_error;
        EC_ERR("Failed to receive FMMUs datagram for slave %u"
                " (datagram state %u).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        EC_ERR("Failed to set FMMUs of slave %u: ",
               slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    ec_fsm_slave_config_enter_safeop(fsm);
}

/*****************************************************************************/

/** Request SAFEOP state.
 */
void ec_fsm_slave_config_enter_safeop(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    fsm->state = ec_fsm_slave_config_state_safeop;
    ec_fsm_change_start(fsm->fsm_change, fsm->slave, EC_SLAVE_STATE_SAFEOP);
    ec_fsm_change_exec(fsm->fsm_change); // execute immediately
}

/*****************************************************************************/

/** Slave configuration state: SAFEOP.
 */
void ec_fsm_slave_config_state_safeop(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    ec_master_t *master = fsm->slave->master;
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_change_exec(fsm->fsm_change)) return;

    if (!ec_fsm_change_success(fsm->fsm_change)) {
        if (!fsm->fsm_change->spontaneous_change)
            fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        return;
    }

    // slave is now in SAFEOP

    if (master->debug_level) {
        EC_DBG("Slave %u is now in SAFEOP.\n", slave->ring_position);
    }

    if (fsm->slave->current_state == fsm->slave->requested_state) {
        fsm->state = ec_fsm_slave_config_state_end; // successful
        if (master->debug_level) {
            EC_DBG("Finished configuration of slave %u.\n",
                   slave->ring_position);
        }
        return;
    }

    // set state to OP
    fsm->state = ec_fsm_slave_config_state_op;
    ec_fsm_change_start(fsm->fsm_change, slave, EC_SLAVE_STATE_OP);
    ec_fsm_change_exec(fsm->fsm_change); // execute immediately
}

/*****************************************************************************/

/** Slave configuration state: OP
 */
void ec_fsm_slave_config_state_op(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    ec_master_t *master = fsm->slave->master;
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_change_exec(fsm->fsm_change)) return;

    if (!ec_fsm_change_success(fsm->fsm_change)) {
        if (!fsm->fsm_change->spontaneous_change)
            slave->error_flag = 1;
        fsm->state = ec_fsm_slave_config_state_error;
        return;
    }

    // slave is now in OP

    if (master->debug_level) {
        EC_DBG("Slave %u is now in OP.\n", slave->ring_position);
        EC_DBG("Finished configuration of slave %u.\n", slave->ring_position);
    }

    fsm->state = ec_fsm_slave_config_state_end; // successful
}

/*****************************************************************************/

/** Reconfigure the slave starting at INIT.
 */
void ec_fsm_slave_config_reconfigure(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
    if (fsm->slave->master->debug_level) {
        EC_DBG("Slave configuration for slave %u detached during "
                "configuration. Reconfiguring.", fsm->slave->ring_position);
    }

    ec_fsm_slave_config_enter_init(fsm); // reconfigure
}

/******************************************************************************
 *  Common state functions
 *****************************************************************************/

/** State: ERROR.
 */
void ec_fsm_slave_config_state_error(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
}

/*****************************************************************************/

/** State: END.
 */
void ec_fsm_slave_config_state_end(
        ec_fsm_slave_config_t *fsm /**< slave state machine */
        )
{
}

/*****************************************************************************/
