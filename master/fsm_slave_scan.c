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

/**
   \file
   EtherCAT slave state machines.
*/

/*****************************************************************************/

#include "globals.h"
#include "master.h"
#include "mailbox.h"
#include "sii_firmware.h"
#include "slave_config.h"

#include "fsm_slave_scan.h"

/*****************************************************************************/

void ec_fsm_slave_scan_state_start(ec_fsm_slave_scan_t *);
void ec_fsm_slave_scan_state_address(ec_fsm_slave_scan_t *);
void ec_fsm_slave_scan_state_state(ec_fsm_slave_scan_t *);
void ec_fsm_slave_scan_state_base(ec_fsm_slave_scan_t *);
void ec_fsm_slave_scan_state_dc_cap(ec_fsm_slave_scan_t *);
void ec_fsm_slave_scan_state_dc_times(ec_fsm_slave_scan_t *);
void ec_fsm_slave_scan_state_datalink(ec_fsm_slave_scan_t *);
#ifdef EC_SII_ASSIGN
void ec_fsm_slave_scan_state_assign_sii(ec_fsm_slave_scan_t *);
#endif
#ifdef EC_SII_CACHE
void ec_fsm_slave_scan_state_sii_identity(ec_fsm_slave_scan_t *);
#endif
#ifdef EC_SII_OVERRIDE
void ec_fsm_slave_scan_state_sii_device(ec_fsm_slave_scan_t *);
void ec_fsm_slave_scan_state_sii_request(ec_fsm_slave_scan_t *);
#endif
void ec_fsm_slave_scan_state_sii_size(ec_fsm_slave_scan_t *);
void ec_fsm_slave_scan_state_sii_data(ec_fsm_slave_scan_t *);
void ec_fsm_slave_scan_state_sii_parse(ec_fsm_slave_scan_t *);
void ec_fsm_slave_scan_state_mailbox_cleared(ec_fsm_slave_scan_t *);
#ifdef EC_REGALIAS
void ec_fsm_slave_scan_state_regalias(ec_fsm_slave_scan_t *);
#endif
void ec_fsm_slave_scan_state_preop(ec_fsm_slave_scan_t *);
void ec_fsm_slave_scan_state_sync(ec_fsm_slave_scan_t *);
void ec_fsm_slave_scan_state_pdos(ec_fsm_slave_scan_t *);

void ec_fsm_slave_scan_state_end(ec_fsm_slave_scan_t *);
void ec_fsm_slave_scan_state_error(ec_fsm_slave_scan_t *);

void ec_fsm_slave_scan_enter_datalink(ec_fsm_slave_scan_t *);
#ifdef EC_REGALIAS
void ec_fsm_slave_scan_enter_regalias(ec_fsm_slave_scan_t *);
#endif
#ifdef EC_SII_CACHE
void ec_fsm_slave_scan_enter_sii_identity(ec_fsm_slave_scan_t *);
#endif
#ifdef EC_SII_OVERRIDE
void ec_fsm_slave_scan_enter_sii_request(ec_fsm_slave_scan_t *);
#endif
void ec_fsm_slave_scan_enter_attach_sii(ec_fsm_slave_scan_t *);
void ec_fsm_slave_scan_enter_sii_size(ec_fsm_slave_scan_t *);
void ec_fsm_slave_scan_enter_preop(ec_fsm_slave_scan_t *);
void ec_fsm_slave_scan_enter_clear_mailbox(ec_fsm_slave_scan_t *);
void ec_fsm_slave_scan_enter_pdos(ec_fsm_slave_scan_t *);

/*****************************************************************************/

/** Constructor.
 */
void ec_fsm_slave_scan_init(
        ec_fsm_slave_scan_t *fsm, /**< Slave scanning state machine. */
        ec_datagram_t *datagram, /**< Datagram to use. */
        ec_fsm_slave_config_t *fsm_slave_config, /**< Slave configuration
                                                  state machine to use. */
        ec_fsm_pdo_t *fsm_pdo /**< PDO configuration machine to use. */
        )
{
    fsm->datagram = datagram;
    fsm->fsm_slave_config = fsm_slave_config;
    fsm->fsm_pdo = fsm_pdo;

    // init sub state machines
    ec_fsm_sii_init(&fsm->fsm_sii);
}

/*****************************************************************************/

/** Destructor.
 */
void ec_fsm_slave_scan_clear(ec_fsm_slave_scan_t *fsm /**< slave state machine */)
{
    // clear sub state machines
    ec_fsm_sii_clear(&fsm->fsm_sii);
}

/*****************************************************************************/

/**
 * Start slave scan state machine.
 */

void ec_fsm_slave_scan_start(
        ec_fsm_slave_scan_t *fsm, /**< slave state machine */
        ec_slave_t *slave /**< slave to configure */
        )
{
    fsm->slave = slave;
    fsm->state = ec_fsm_slave_scan_state_start;
}

/*****************************************************************************/

/**
   \return false, if state machine has terminated
*/

int ec_fsm_slave_scan_running(const ec_fsm_slave_scan_t *fsm /**< slave state machine */)
{
    return fsm->state != ec_fsm_slave_scan_state_end
        && fsm->state != ec_fsm_slave_scan_state_error;
}

/*****************************************************************************/

/**
   Executes the current state of the state machine.
   If the state machine's datagram is not sent or received yet, the execution
   of the state machine is delayed to the next cycle.
   \return false, if state machine has terminated
*/

int ec_fsm_slave_scan_exec(ec_fsm_slave_scan_t *fsm /**< slave state machine */)
{
    if (fsm->datagram->state == EC_DATAGRAM_SENT
        || fsm->datagram->state == EC_DATAGRAM_QUEUED) {
        // datagram was not sent or received yet.
        return ec_fsm_slave_scan_running(fsm);
    }

    fsm->state(fsm);
    return ec_fsm_slave_scan_running(fsm);
}

/*****************************************************************************/

/**
   \return true, if the state machine terminated gracefully
*/

int ec_fsm_slave_scan_success(const ec_fsm_slave_scan_t *fsm /**< slave state machine */)
{
    return fsm->state == ec_fsm_slave_scan_state_end;
}

/******************************************************************************
 *  slave scan state machine
 *****************************************************************************/

/**
   Slave scan state: START.
   First state of the slave state machine. Writes the station address to the
   slave, according to its ring position.
*/

void ec_fsm_slave_scan_state_start(ec_fsm_slave_scan_t *fsm /**< slave state machine */)
{
    // write station address
    ec_datagram_apwr(fsm->datagram, fsm->slave->ring_position, 0x0010, 2);
    EC_WRITE_U16(fsm->datagram->data, fsm->slave->station_address);
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_scan_state_address;
}

/*****************************************************************************/

/**
   Slave scan state: ADDRESS.
*/

void ec_fsm_slave_scan_state_address(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_datagram_t *datagram = fsm->datagram;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_scan_state_error;
        EC_SLAVE_ERR(fsm->slave,
                "Failed to receive station address datagram: ");
        ec_datagram_print_state(datagram);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_scan_state_error;
        EC_SLAVE_ERR(fsm->slave, "Failed to write station address: ");
        ec_datagram_print_wc_error(datagram);
        return;
    }

    // Read AL state
    ec_datagram_fprd(datagram, fsm->slave->station_address, 0x0130, 2);
    ec_datagram_zero(datagram);
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_scan_state_state;
}

/*****************************************************************************/

/**
   Slave scan state: STATE.
*/

void ec_fsm_slave_scan_state_state(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_scan_state_error;
        EC_SLAVE_ERR(slave, "Failed to receive AL state datagram: ");
        ec_datagram_print_state(datagram);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_scan_state_error;
        EC_SLAVE_ERR(slave, "Failed to read AL state: ");
        ec_datagram_print_wc_error(datagram);
        return;
    }

    slave->current_state = EC_READ_U8(datagram->data);
    if (slave->current_state & EC_SLAVE_STATE_ACK_ERR) {
        char state_str[EC_STATE_STRING_SIZE];
        ec_state_string(slave->current_state, state_str, 0);
        EC_SLAVE_WARN(slave, "Slave has state error bit set (%s)!\n",
                state_str);
    }

    // read base data
    ec_datagram_fprd(datagram, fsm->slave->station_address, 0x0000, 12);
    ec_datagram_zero(datagram);
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_scan_state_base;
}

/*****************************************************************************/

/** Slave scan state: BASE.
 */
void ec_fsm_slave_scan_state_base(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    u8 octet;
    int i;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_scan_state_error;
        EC_SLAVE_ERR(slave, "Failed to receive base data datagram: ");
        ec_datagram_print_state(datagram);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_scan_state_error;
        EC_SLAVE_ERR(slave, "Failed to read base data: ");
        ec_datagram_print_wc_error(datagram);
        return;
    }

    slave->base_type       = EC_READ_U8 (datagram->data);
    slave->base_revision   = EC_READ_U8 (datagram->data + 1);
    slave->base_build      = EC_READ_U16(datagram->data + 2);

    slave->base_fmmu_count = EC_READ_U8 (datagram->data + 4);
    if (slave->base_fmmu_count > EC_MAX_FMMUS) {
        EC_SLAVE_WARN(slave, "Slave has more FMMUs (%u) than the master can"
                " handle (%u).\n", slave->base_fmmu_count, EC_MAX_FMMUS);
        slave->base_fmmu_count = EC_MAX_FMMUS;
    }

    slave->base_sync_count = EC_READ_U8(datagram->data + 5);
    if (slave->base_sync_count > EC_MAX_SYNC_MANAGERS) {
        EC_SLAVE_WARN(slave, "Slave provides more sync managers (%u)"
                " than the master can handle (%u).\n",
                slave->base_sync_count, EC_MAX_SYNC_MANAGERS);
        slave->base_sync_count = EC_MAX_SYNC_MANAGERS;
    }

    octet = EC_READ_U8(datagram->data + 7);
    for (i = 0; i < EC_MAX_PORTS; i++) {
        slave->ports[i].desc = (octet >> (2 * i)) & 0x03;
    }

    octet = EC_READ_U8(datagram->data + 8);
    slave->base_fmmu_bit_operation = octet & 0x01;
    slave->base_dc_supported = (octet >> 2) & 0x01;
    slave->base_dc_range = ((octet >> 3) & 0x01) ? EC_DC_64 : EC_DC_32;

    if (slave->base_dc_supported) {
        // read DC capabilities
        ec_datagram_fprd(datagram, slave->station_address, 0x0910,
                slave->base_dc_range == EC_DC_64 ? 8 : 4);
        ec_datagram_zero(datagram);
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_slave_scan_state_dc_cap;
    } else {
        ec_fsm_slave_scan_enter_datalink(fsm);
    }
}

/*****************************************************************************/

/**
   Slave scan state: DC CAPABILITIES.
*/

void ec_fsm_slave_scan_state_dc_cap(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_scan_state_error;
        EC_SLAVE_ERR(slave, "Failed to receive system time datagram: ");
        ec_datagram_print_state(datagram);
        return;
    }

    if (datagram->working_counter == 1) {
        slave->has_dc_system_time = 1;
        EC_SLAVE_DBG(slave, 1, "Slave has the System Time register.\n");
    } else if (datagram->working_counter == 0) {
        EC_SLAVE_DBG(slave, 1, "Slave has no System Time register; delay "
                "measurement only.\n");
    } else {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_scan_state_error;
        EC_SLAVE_ERR(slave, "Failed to determine, if system time register is "
                "supported: ");
        ec_datagram_print_wc_error(datagram);
        return;
    }

    // read DC port receive times
    ec_datagram_fprd(datagram, slave->station_address, 0x0900, 16);
    ec_datagram_zero(datagram);
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_scan_state_dc_times;
}

/*****************************************************************************/

/**
   Slave scan state: DC TIMES.
*/

void ec_fsm_slave_scan_state_dc_times(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    int i;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_scan_state_error;
        EC_SLAVE_ERR(slave, "Failed to receive system time datagram: ");
        ec_datagram_print_state(datagram);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_scan_state_error;
        EC_SLAVE_ERR(slave, "Failed to get DC receive times: ");
        ec_datagram_print_wc_error(datagram);
        return;
    }

    for (i = 0; i < EC_MAX_PORTS; i++) {
        u32 new_time = EC_READ_U32(datagram->data + 4 * i);
        if (new_time == slave->ports[i].receive_time) {
            // time has not changed since initial scan; this port has not
            // processed the broadcast timing datagram.  this can occur
            // in certain redundancy scenarios.  it can also occur if the
            // port is closed, so at this stage we can't tell if it's an issue.
            slave->ports[i].link.bypassed = 1;
        }
        slave->ports[i].receive_time = new_time;
    }

    ec_fsm_slave_scan_enter_datalink(fsm);
}

/*****************************************************************************/

/**
   Slave scan entry function: DATALINK.
*/

void ec_fsm_slave_scan_enter_datalink(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    // read data link status
    ec_datagram_fprd(datagram, slave->station_address, 0x0110, 2);
    ec_datagram_zero(datagram);
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_scan_state_datalink;
}

#ifdef EC_SII_CACHE
/*****************************************************************************/

/** Enter slave scan state SII_IDENTITY.
 */
void ec_fsm_slave_scan_enter_sii_identity(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    // Start fetching SII serial number
    fsm->sii_offset = EC_ALIAS_SII_OFFSET;
    ec_fsm_sii_read(&fsm->fsm_sii, fsm->slave, fsm->sii_offset,
            EC_FSM_SII_USE_CONFIGURED_ADDRESS);
    fsm->state = ec_fsm_slave_scan_state_sii_identity;
    fsm->state(fsm); // execute state immediately
}
#endif

/*****************************************************************************/

/** Enter slave scan state ATTACH_SII.
 */
void ec_fsm_slave_scan_enter_attach_sii(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_sii_image_t *sii_image;
    ec_slave_t *slave = fsm->slave;

#ifdef EC_SII_CACHE
    unsigned int i = 0;
    unsigned int found = 0;

    if ((slave->effective_alias != 0) || (slave->effective_serial_number != 0)) {
        list_for_each_entry(sii_image, &slave->master->sii_images, list) {
            // Check if slave match a stored SII image with alias, serial number,
            // vendor id and product code.
            if ((slave->effective_alias != 0) &&
                    (slave->effective_alias == sii_image->sii.alias) &&
                    (slave->effective_revision_number == sii_image->sii.revision_number)) {
                EC_SLAVE_DBG(slave, 1, "Slave can re-use SII image data stored."
                        " Identified by alias %u.\n", (uint32_t)slave->effective_alias);
                found = 1;
                break;
            }
            else if ((slave->effective_vendor_id == sii_image->sii.vendor_id) &&
                     (slave->effective_product_code == sii_image->sii.product_code) &&
                     (slave->effective_revision_number == sii_image->sii.revision_number) &&
                     (slave->effective_serial_number == sii_image->sii.serial_number)) {
                EC_SLAVE_DBG(slave, 1, "Slave can re-use SII image data stored."
                        " Identified by vendor id 0x%08x,"
                        " product code 0x%08x, revision 0x%08x and serial 0x%08x.\n",
                        slave->effective_vendor_id,
                        slave->effective_product_code,
                        slave->effective_revision_number,
                        slave->effective_serial_number);
                found = 1;
                break;
            }
        }
    }
    else {
        EC_SLAVE_DBG(slave, 1, "Slave cannot be uniquely identified."
                " SII image data cannot be re-used!\n");
    }

    if (found) {
        // Update slave references lost during slave initialization
        slave->effective_vendor_id = sii_image->sii.vendor_id;
        slave->effective_product_code = sii_image->sii.product_code;
        slave->effective_revision_number = sii_image->sii.revision_number;
        slave->effective_serial_number = sii_image->sii.serial_number;
        slave->sii_image = sii_image;
        for (i = 0; i < slave->sii_image->sii.sync_count; i++) {
            slave->sii_image->sii.syncs[i].slave = slave;
        }
        // The SII image data is already available and we can enter PREOP
#ifdef EC_REGALIAS
        ec_fsm_slave_scan_enter_regalias(fsm);
#else
        if (slave->sii_image->sii.mailbox_protocols & EC_MBOX_COE) {
            ec_fsm_slave_scan_enter_preop(fsm);
        } else {
            fsm->state = ec_fsm_slave_scan_state_end;
        }
#endif
    }
    else
#endif
    {
        EC_MASTER_DBG(slave->master, 1, "Creating slave SII image for %u\n",
                fsm->slave->ring_position);

        if (!(sii_image = (ec_sii_image_t *) kmalloc(sizeof(ec_sii_image_t),
                        GFP_KERNEL))) {
            fsm->state = ec_fsm_slave_scan_state_error;
            EC_MASTER_ERR(fsm->slave->master, "Failed to allocate memory"
                    " for slave SII image.\n");
            return;
        }
        // Initialize SII image data
        ec_slave_sii_image_init(sii_image);
        // Attach SII image to the slave
        slave->sii_image = sii_image;
        // Store the SII image for later re-use
        list_add_tail(&sii_image->list, &slave->master->sii_images);

        ec_fsm_slave_scan_enter_sii_size(fsm);
    }
}

/*****************************************************************************/

/** Enter slave scan state SII_SIZE.
 */
void ec_fsm_slave_scan_enter_sii_size(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_slave_t *slave = fsm->slave;

#ifdef EC_SII_OVERRIDE
    if (!slave->vendor_words) {
        if (!(slave->vendor_words =
              (uint16_t *) kmalloc(32, GFP_KERNEL))) {
            EC_SLAVE_ERR(slave, "Failed to allocate 16 words of SII data.\n");
            slave->error_flag = 1;
            fsm->state = ec_fsm_slave_scan_state_error;
            return;
        }
    }

    // Start fetching device identity
    fsm->sii_offset = 0;
    fsm->state = ec_fsm_slave_scan_state_sii_device;
#else
    // Start fetching SII size
    fsm->sii_offset = EC_FIRST_SII_CATEGORY_OFFSET; // first category header
    fsm->state = ec_fsm_slave_scan_state_sii_size;
#endif

    ec_fsm_sii_read(&fsm->fsm_sii, slave, fsm->sii_offset,
            EC_FSM_SII_USE_CONFIGURED_ADDRESS);

    fsm->state(fsm); // execute state immediately
}

/*****************************************************************************/

#ifdef EC_SII_ASSIGN

/** Enter slave scan state ASSIGN_SII.
 */
void ec_fsm_slave_scan_enter_assign_sii(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    EC_SLAVE_DBG(slave, 1, "Assigning SII access to EtherCAT.\n");

    // assign SII to ECAT
    ec_datagram_fpwr(datagram, slave->station_address, 0x0500, 1);
    EC_WRITE_U8(datagram->data, 0x00); // EtherCAT
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_scan_state_assign_sii;
}

#endif

/*****************************************************************************/

/**
   Slave scan state: DATALINK.
*/

void ec_fsm_slave_scan_state_datalink(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_scan_state_error;
        EC_SLAVE_ERR(slave, "Failed to receive DL status datagram: ");
        ec_datagram_print_state(datagram);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_scan_state_error;
        EC_SLAVE_ERR(slave, "Failed to read DL status: ");
        ec_datagram_print_wc_error(datagram);
        return;
    }

    ec_slave_set_dl_status(slave, EC_READ_U16(datagram->data));

#ifdef EC_SII_ASSIGN
    ec_fsm_slave_scan_enter_assign_sii(fsm);
#elif defined(EC_SII_CACHE)
    ec_fsm_slave_scan_enter_sii_identity(fsm);
#else
    ec_fsm_slave_scan_enter_attach_sii(fsm);
#endif
}

/*****************************************************************************/

#ifdef EC_SII_ASSIGN

/**
   Slave scan state: ASSIGN_SII.
*/

void ec_fsm_slave_scan_state_assign_sii(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--) {
        return;
    }

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        EC_SLAVE_WARN(slave, "Failed to receive SII assignment datagram: ");
        ec_datagram_print_state(datagram);
        // Try to go on, probably assignment is correct
        goto continue_with_sii_size;
    }

    if (datagram->working_counter != 1) {
        EC_SLAVE_WARN(slave, "Failed to assign SII to EtherCAT: ");
        ec_datagram_print_wc_error(datagram);
        // Try to go on, probably assignment is correct
    }

continue_with_sii_size:
#ifdef EC_SII_CACHE
    ec_fsm_slave_scan_enter_sii_identity(fsm);
#else
    ec_fsm_slave_scan_enter_attach_sii(fsm);
#endif
}

#endif

#ifdef EC_SII_CACHE
/*****************************************************************************/

/**
   Slave scan state: SII IDENTITY.
*/

void ec_fsm_slave_scan_state_sii_identity(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_slave_t *slave = fsm->slave;

    while (1) {
        if (ec_fsm_sii_exec(&fsm->fsm_sii, fsm->datagram))
            return;

        if (!ec_fsm_sii_success(&fsm->fsm_sii)) {
            fsm->slave->error_flag = 1;
            fsm->state = ec_fsm_slave_scan_state_error;
            EC_SLAVE_ERR(slave, "Failed to determine SII identity\n");
            return;
        }

        switch (fsm->sii_offset) {
            case EC_ALIAS_SII_OFFSET:
                slave->effective_alias = EC_READ_U16(fsm->fsm_sii.value);
                EC_SLAVE_DBG(slave, 1, "Alias: %u\n",
                             (uint32_t)slave->effective_alias);
                if (slave->effective_alias) {
                    fsm->sii_offset = EC_REVISION_SII_OFFSET;
                } else {
                    fsm->sii_offset = EC_SERIAL_SII_OFFSET;
                }
                break;
            case EC_SERIAL_SII_OFFSET:
                slave->effective_serial_number = EC_READ_U32(fsm->fsm_sii.value);
                EC_SLAVE_DBG(slave, 1, "Serial Number: 0x%08x\n",
                             slave->effective_serial_number);
                if (!slave->effective_serial_number) {
                    ec_fsm_slave_scan_enter_attach_sii(fsm);
                    return;
                }
                fsm->sii_offset = EC_VENDOR_SII_OFFSET;
                break;
            case EC_VENDOR_SII_OFFSET:
                slave->effective_vendor_id = EC_READ_U32(fsm->fsm_sii.value);
                EC_SLAVE_DBG(slave, 1, "Vendor ID: 0x%08x\n",
                             slave->effective_vendor_id);
                fsm->sii_offset = EC_PRODUCT_SII_OFFSET;
                break;
            case EC_PRODUCT_SII_OFFSET:
                slave->effective_product_code = EC_READ_U32(fsm->fsm_sii.value);
                EC_SLAVE_DBG(slave, 1, "Product Code: 0x%08x\n",
                             slave->effective_product_code);
                fsm->sii_offset = EC_REVISION_SII_OFFSET;
                break;
            case EC_REVISION_SII_OFFSET:
                slave->effective_revision_number = EC_READ_U32(fsm->fsm_sii.value);
                EC_SLAVE_DBG(slave, 1, "Revision: 0x%08x\n",
                             slave->effective_revision_number);
                ec_fsm_slave_scan_enter_attach_sii(fsm);
                return;
            default:
                fsm->slave->error_flag = 1;
                fsm->state = ec_fsm_slave_scan_state_error;
                EC_SLAVE_ERR(slave, "Unexpected offset %u in identity scan.\n",
                             fsm->sii_offset);
                return;
        }

        ec_fsm_sii_read(&fsm->fsm_sii, fsm->slave, fsm->sii_offset,
                EC_FSM_SII_USE_CONFIGURED_ADDRESS);
    }
}
#endif

#ifdef EC_SII_OVERRIDE
/*****************************************************************************/

/**
   Slave scan state: SII DEVICE.
*/

void ec_fsm_slave_scan_state_sii_device(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_sii_exec(&fsm->fsm_sii, fsm->datagram))
        return;

    if (!ec_fsm_sii_success(&fsm->fsm_sii)) {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_scan_state_error;
        EC_SLAVE_ERR(slave, "Failed to determine product and vendor id."
                " Reading word offset 0x%04x failed.\n",
                fsm->sii_offset);
        return;
    }

    memcpy(slave->vendor_words + fsm->sii_offset, fsm->fsm_sii.value, 4);

    if (fsm->sii_offset + 2 < 16) {
        // fetch the next 2 words
        fsm->sii_offset += 2;
        ec_fsm_sii_read(&fsm->fsm_sii, slave, fsm->sii_offset,
                        EC_FSM_SII_USE_CONFIGURED_ADDRESS);
        ec_fsm_sii_exec(&fsm->fsm_sii, fsm->datagram); // execute state immediately
        return;
    }

    // Evaluate SII contents
    slave->sii_image->sii.alias           = EC_READ_U16(slave->vendor_words + EC_ALIAS_SII_OFFSET);
    slave->sii_image->sii.vendor_id       = EC_READ_U32(slave->vendor_words + EC_VENDOR_SII_OFFSET);
    slave->sii_image->sii.product_code    = EC_READ_U32(slave->vendor_words + EC_PRODUCT_SII_OFFSET);
    slave->sii_image->sii.revision_number = EC_READ_U32(slave->vendor_words + EC_REVISION_SII_OFFSET);
    slave->sii_image->sii.serial_number   = EC_READ_U32(slave->vendor_words + EC_SERIAL_SII_OFFSET);

    slave->effective_alias                = slave->sii_image->sii.alias;
#ifdef EC_SII_CACHE
    slave->effective_vendor_id            = slave->sii_image->sii.vendor_id;
    slave->effective_product_code         = slave->sii_image->sii.product_code;
    slave->effective_revision_number      = slave->sii_image->sii.revision_number;
    slave->effective_serial_number        = slave->sii_image->sii.serial_number;
#endif

    ec_fsm_slave_scan_enter_sii_request(fsm);
}

/*****************************************************************************/

struct firmware_request_context
{
    struct task_struct *fsm_task;
    ec_fsm_slave_scan_t *fsm;
    ec_slave_t *slave;
};

static const struct firmware no_sii_firmware;

static void firmware_request_complete(
        const struct firmware *firmware,
        void *context
        )
{
    struct firmware_request_context *ctx = context;
    ec_fsm_slave_scan_t *fsm = ctx->fsm;

    if (fsm->slave != ctx->slave) {
        printk(KERN_ERR "Aborting firmware request; FSM slave changed unexpectedly.\n");
        ec_release_sii_firmware(firmware);
    } else if (fsm->state != ec_fsm_slave_scan_state_sii_request) {
        EC_SLAVE_WARN(fsm->slave, "Aborting firmware request; FSM state changed unexpectedly.\n");
        ec_release_sii_firmware(firmware);
    } else {
        fsm->sii_firmware = firmware ? firmware : &no_sii_firmware;
    }

    kfree(ctx);
}

/*****************************************************************************/

/**
   Enter slave scan state: SII REQUEST.
*/

void ec_fsm_slave_scan_enter_sii_request(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_slave_t *slave = fsm->slave;
    struct firmware_request_context *ctx;

    if (!(ctx = kmalloc(sizeof(*ctx), GFP_KERNEL))) {
        EC_SLAVE_ERR(slave, "Unable to allocate firmware request context.\n");
        fsm->state = ec_fsm_slave_scan_state_error;
        return;
    }

    ctx->fsm_task = current;
    ctx->fsm = fsm;
    ctx->slave = slave;

    fsm->sii_firmware = NULL;
    fsm->state = ec_fsm_slave_scan_state_sii_request;
    ec_request_sii_firmware(slave, ctx, firmware_request_complete);
    fsm->state(fsm); // execute state immediately
}

/*****************************************************************************/

/**
   Slave scan state: SII REQUEST.
*/

void ec_fsm_slave_scan_state_sii_request(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_slave_t *slave = fsm->slave;
    const struct firmware *firmware = fsm->sii_firmware;
    
    if (firmware == &no_sii_firmware) {
        EC_SLAVE_DBG(slave, 1, "SII firmware file not found; reading SII data from slave.\n");
        fsm->sii_firmware = NULL;

        fsm->sii_offset = EC_FIRST_SII_CATEGORY_OFFSET; // first category header
        ec_fsm_sii_read(&fsm->fsm_sii, slave, fsm->sii_offset,
                EC_FSM_SII_USE_CONFIGURED_ADDRESS);
        fsm->state = ec_fsm_slave_scan_state_sii_size;
        fsm->state(fsm); // execute state immediately
    } else if (firmware) {
        EC_SLAVE_DBG(slave, 1, "Firmware file found, reading %zu bytes.\n", firmware->size);

        slave->sii_image->nwords = firmware->size / 2;

        if (slave->sii_image->words) {
            EC_SLAVE_WARN(slave, "Freeing old SII data...\n");
            kfree(slave->sii_image->words);
        }
        if (!(slave->sii_image->words =
              (uint16_t *) kmalloc(slave->sii_image->nwords * 2, GFP_KERNEL))) {
            EC_SLAVE_ERR(slave, "Failed to allocate %zu words of SII data.\n",
                slave->sii_image->nwords);
            slave->sii_image->nwords = 0;
            slave->error_flag = 1;
            ec_release_sii_firmware(firmware);
            fsm->sii_firmware = NULL;

            fsm->state = ec_fsm_slave_scan_state_error;
            return;
        }

        memcpy(slave->sii_image->words, firmware->data, slave->sii_image->nwords * 2);
        ec_release_sii_firmware(firmware);
        fsm->sii_firmware = NULL;

        fsm->state = ec_fsm_slave_scan_state_sii_parse;
        fsm->state(fsm); // execute state immediately
    } else {
        // do nothing while waiting for async request to complete
        fsm->datagram->state = EC_DATAGRAM_INVALID;
    }
}
#endif

/*****************************************************************************/

/**
   Slave scan state: SII SIZE.
*/

void ec_fsm_slave_scan_state_sii_size(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_slave_t *slave = fsm->slave;
    uint16_t cat_type, cat_size;

    if (ec_fsm_sii_exec(&fsm->fsm_sii, fsm->datagram))
        return;

    if (!slave->sii_image) {
        EC_SLAVE_ERR(slave, "Slave has no SII image attached!\n");
        slave->error_flag = 1;
        fsm->state = ec_fsm_slave_scan_state_error;
        return;
    }

    if (!ec_fsm_sii_success(&fsm->fsm_sii)) {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_scan_state_error;
        EC_SLAVE_ERR(slave, "Failed to determine SII content size:"
                " Reading word offset 0x%04x failed. Assuming %u words.\n",
                fsm->sii_offset, EC_FIRST_SII_CATEGORY_OFFSET);
        slave->sii_image->nwords = EC_FIRST_SII_CATEGORY_OFFSET;
        goto alloc_sii;
    }

    cat_type = EC_READ_U16(fsm->fsm_sii.value);
    cat_size = EC_READ_U16(fsm->fsm_sii.value + 2);

    if (cat_type != 0xFFFF) { // not the last category
        off_t next_offset = 2UL + fsm->sii_offset + cat_size;
        if (next_offset >= EC_MAX_SII_SIZE) {
            EC_SLAVE_WARN(slave, "SII size exceeds %u words"
                    " (0xffff limiter missing?).\n", EC_MAX_SII_SIZE);
            // cut off category data...
            slave->sii_image->nwords = EC_FIRST_SII_CATEGORY_OFFSET;
            goto alloc_sii;
        }
        fsm->sii_offset = next_offset;
        ec_fsm_sii_read(&fsm->fsm_sii, slave, fsm->sii_offset,
                        EC_FSM_SII_USE_CONFIGURED_ADDRESS);
        ec_fsm_sii_exec(&fsm->fsm_sii, fsm->datagram); // execute state immediately
        return;
    }

    slave->sii_image->nwords = fsm->sii_offset + 1;

alloc_sii:
    if (slave->sii_image->words) {
        EC_SLAVE_WARN(slave, "Freeing old SII data...\n");
        kfree(slave->sii_image->words);
    }

    if (!(slave->sii_image->words =
                (uint16_t *) kmalloc(slave->sii_image->nwords * 2, GFP_KERNEL))) {
        EC_SLAVE_ERR(slave, "Failed to allocate %zu words of SII data.\n",
               slave->sii_image->nwords);
        slave->sii_image->nwords = 0;
        slave->error_flag = 1;
        fsm->state = ec_fsm_slave_scan_state_error;
        return;
    }

#ifdef EC_SII_OVERRIDE
    // Copy vendor data to sii words
    memcpy(slave->sii_image->words, slave->vendor_words, 32);
    kfree(slave->vendor_words);
    slave->vendor_words = NULL;
    
    // Start fetching rest of SII contents
    fsm->sii_offset = 0x0010;
#else
    // Start fetching SII contents
    fsm->sii_offset = 0x0000;
#endif
    fsm->state = ec_fsm_slave_scan_state_sii_data;
    ec_fsm_sii_read(&fsm->fsm_sii, slave, fsm->sii_offset,
            EC_FSM_SII_USE_CONFIGURED_ADDRESS);
    ec_fsm_sii_exec(&fsm->fsm_sii, fsm->datagram); // execute state immediately
}

/*****************************************************************************/

/**
   Slave scan state: SII DATA.
*/

void ec_fsm_slave_scan_state_sii_data(ec_fsm_slave_scan_t *fsm /**< slave state machine */)
{
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_sii_exec(&fsm->fsm_sii, fsm->datagram)) return;

    if (!ec_fsm_sii_success(&fsm->fsm_sii)) {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_scan_state_error;
        EC_SLAVE_ERR(slave, "Failed to fetch SII contents.\n");
        return;
    }

    if (!slave->sii_image) {
        EC_SLAVE_ERR(slave, "Slave has no SII image attached!\n");
        slave->error_flag = 1;
        fsm->state = ec_fsm_slave_scan_state_error;
        return;
    }

    // 2 words fetched

    if (fsm->sii_offset + 2 <= slave->sii_image->nwords) { // 2 words fit
        memcpy(slave->sii_image->words + fsm->sii_offset, fsm->fsm_sii.value, 4);
    } else { // copy the last word
        memcpy(slave->sii_image->words + fsm->sii_offset, fsm->fsm_sii.value, 2);
    }

    if (fsm->sii_offset + 2 < slave->sii_image->nwords) {
        // fetch the next 2 words
        fsm->sii_offset += 2;
        ec_fsm_sii_read(&fsm->fsm_sii, slave, fsm->sii_offset,
                        EC_FSM_SII_USE_CONFIGURED_ADDRESS);
        ec_fsm_sii_exec(&fsm->fsm_sii, fsm->datagram); // execute state immediately
        return;
    }

    fsm->state = ec_fsm_slave_scan_state_sii_parse;
    fsm->state(fsm); // execute state immediately
}

/*****************************************************************************/

/**
   Slave scan state: SII PARSE.
*/

void ec_fsm_slave_scan_state_sii_parse(ec_fsm_slave_scan_t *fsm /**< slave state machine */)
{
    ec_slave_t *slave = fsm->slave;
    uint16_t *cat_word, cat_type, cat_size;

    // Evaluate SII contents

    ec_slave_clear_sync_managers(slave);

#ifndef EC_SII_OVERRIDE
    slave->sii_image->sii.alias =
        EC_READ_U16(slave->sii_image->words + 0x0004);
    slave->effective_alias = slave->sii_image->sii.alias;
    slave->sii_image->sii.vendor_id =
        EC_READ_U32(slave->sii_image->words + 0x0008);
    slave->sii_image->sii.product_code =
        EC_READ_U32(slave->sii_image->words + 0x000A);
    slave->sii_image->sii.revision_number =
        EC_READ_U32(slave->sii_image->words + 0x000C);
    slave->sii_image->sii.serial_number =
        EC_READ_U32(slave->sii_image->words + 0x000E);
#endif
    slave->sii_image->sii.boot_rx_mailbox_offset =
        EC_READ_U16(slave->sii_image->words + 0x0014);
    slave->sii_image->sii.boot_rx_mailbox_size =
        EC_READ_U16(slave->sii_image->words + 0x0015);
    slave->sii_image->sii.boot_tx_mailbox_offset =
        EC_READ_U16(slave->sii_image->words + 0x0016);
    slave->sii_image->sii.boot_tx_mailbox_size =
        EC_READ_U16(slave->sii_image->words + 0x0017);
    slave->sii_image->sii.std_rx_mailbox_offset =
        EC_READ_U16(slave->sii_image->words + 0x0018);
    slave->sii_image->sii.std_rx_mailbox_size =
        EC_READ_U16(slave->sii_image->words + 0x0019);
    slave->sii_image->sii.std_tx_mailbox_offset =
        EC_READ_U16(slave->sii_image->words + 0x001A);
    slave->sii_image->sii.std_tx_mailbox_size =
        EC_READ_U16(slave->sii_image->words + 0x001B);
    slave->sii_image->sii.mailbox_protocols =
        EC_READ_U16(slave->sii_image->words + 0x001C);

#if !defined(EC_SII_OVERRIDE) && defined(EC_SII_CACHE)
    slave->effective_vendor_id = slave->sii_image->sii.vendor_id;
    slave->effective_product_code = slave->sii_image->sii.product_code;
    slave->effective_revision_number = slave->sii_image->sii.revision_number;
    slave->effective_serial_number = slave->sii_image->sii.serial_number;
#endif

    // clear mailbox settings if invalid values due to invalid sii file
    if ((slave->sii_image->sii.boot_rx_mailbox_offset == 0xFFFF) ||
        (slave->sii_image->sii.boot_tx_mailbox_offset == 0xFFFF) ||
        (slave->sii_image->sii.std_rx_mailbox_offset  == 0xFFFF) ||
        (slave->sii_image->sii.std_tx_mailbox_offset  == 0xFFFF)) {
        slave->sii_image->sii.boot_rx_mailbox_offset = 0;
        slave->sii_image->sii.boot_tx_mailbox_offset = 0;
        slave->sii_image->sii.boot_rx_mailbox_size = 0;
        slave->sii_image->sii.boot_tx_mailbox_size = 0;
        slave->sii_image->sii.std_rx_mailbox_offset = 0;
        slave->sii_image->sii.std_tx_mailbox_offset = 0;
        slave->sii_image->sii.std_rx_mailbox_size = 0;
        slave->sii_image->sii.std_tx_mailbox_size = 0;
        slave->sii_image->sii.mailbox_protocols = 0;
        EC_SLAVE_ERR(slave, "Unexpected mailbox offset in SII data.\n");
    }

    if (slave->sii_image->nwords == EC_FIRST_SII_CATEGORY_OFFSET) {
        // sii does not contain category data
        fsm->state = ec_fsm_slave_scan_state_end;
        return;
    }

    if (slave->sii_image->nwords < EC_FIRST_SII_CATEGORY_OFFSET + 1) {
        EC_SLAVE_ERR(slave, "Unexpected end of SII data:"
                " First category header missing.\n");
        goto end;
    }

    // evaluate category data
    cat_word = slave->sii_image->words + EC_FIRST_SII_CATEGORY_OFFSET;
    while (EC_READ_U16(cat_word) != 0xFFFF) {

        // type and size words must fit
        if (cat_word + 2 - slave->sii_image->words > slave->sii_image->nwords) {
            EC_SLAVE_ERR(slave, "Unexpected end of SII data:"
                    " Category header incomplete.\n");
            goto end;
        }

        cat_type = EC_READ_U16(cat_word) & 0x7FFF;
        cat_size = EC_READ_U16(cat_word + 1);
        cat_word += 2;

        if (cat_word + cat_size - slave->sii_image->words > slave->sii_image->nwords) {
            EC_SLAVE_WARN(slave, "Unexpected end of SII data:"
                    " Category data incomplete.\n");
            goto end;
        }

        switch (cat_type) {
            case 0x000A:
                if (ec_slave_fetch_sii_strings(slave, (uint8_t *) cat_word,
                            cat_size * 2))
                    goto end;
                break;
            case 0x001E:
                if (ec_slave_fetch_sii_general(slave, (uint8_t *) cat_word,
                            cat_size * 2))
                    goto end;
                break;
            case 0x0028:
                break;
            case 0x0029:
                if (ec_slave_fetch_sii_syncs(slave, (uint8_t *) cat_word,
                            cat_size * 2))
                    goto end;
                break;
            case 0x0032:
                if (ec_slave_fetch_sii_pdos( slave, (uint8_t *) cat_word,
                            cat_size * 2, EC_DIR_INPUT)) // TxPDO
                    goto end;
                break;
            case 0x0033:
                if (ec_slave_fetch_sii_pdos( slave, (uint8_t *) cat_word,
                            cat_size * 2, EC_DIR_OUTPUT)) // RxPDO
                    goto end;
                break;
            default:
                EC_SLAVE_DBG(slave, 1, "Unknown category type 0x%04X.\n",
                        cat_type);
        }

        cat_word += cat_size;
        if (cat_word - slave->sii_image->words >= slave->sii_image->nwords) {
            EC_SLAVE_WARN(slave, "Unexpected end of SII data:"
                    " Next category header missing.\n");
            goto end;
        }
    }

#ifdef EC_REGALIAS
    ec_fsm_slave_scan_enter_regalias(fsm);
#else
    if (slave->sii_image->sii.mailbox_protocols & EC_MBOX_COE) {
        ec_fsm_slave_scan_enter_preop(fsm);
    } else {
        fsm->state = ec_fsm_slave_scan_state_end;
    }
#endif
    return;

end:
    EC_SLAVE_ERR(slave, "Failed to analyze category data.\n");
    fsm->slave->error_flag = 1;
    fsm->state = ec_fsm_slave_scan_state_error;
}

/*****************************************************************************/

#ifdef EC_REGALIAS

/** Slave scan entry function: REGALIAS.
 */
void ec_fsm_slave_scan_enter_regalias(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    // read alias from register
    EC_SLAVE_DBG(slave, 1, "Reading alias from register.\n");
    ec_datagram_fprd(datagram, slave->station_address, 0x0012, 2);
    ec_datagram_zero(datagram);
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_scan_state_regalias;
}

/*****************************************************************************/

/** Slave scan state: REGALIAS.
 */
void ec_fsm_slave_scan_state_regalias(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_scan_state_error;
        EC_SLAVE_ERR(slave, "Failed to receive register alias datagram: ");
        ec_datagram_print_state(datagram);
        return;
    }

    if (datagram->working_counter != 1) {
        EC_SLAVE_DBG(slave, 1, "Failed to read register alias.\n");
    } else {
        slave->effective_alias = EC_READ_U16(datagram->data);
        EC_SLAVE_DBG(slave, 1, "Read alias %u from register.\n",
                slave->effective_alias);
    }

    if (!slave->sii_image) {
        EC_SLAVE_ERR(slave, "Slave has no SII image attached!\n");
        slave->error_flag = 1;
        fsm->state = ec_fsm_slave_scan_state_error;
        return;
    }

    if (slave->sii_image->sii.mailbox_protocols & EC_MBOX_COE) {
        ec_fsm_slave_scan_enter_preop(fsm);
    } else {
        fsm->state = ec_fsm_slave_scan_state_end;
    }
}

#endif // defined EC_REGALIAS

/*****************************************************************************/

/** Enter slave scan state PREOP.
 */
void ec_fsm_slave_scan_enter_preop(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_slave_t *slave = fsm->slave;
    uint8_t current_state = slave->current_state & EC_SLAVE_STATE_MASK;

    if (current_state != EC_SLAVE_STATE_PREOP
            && current_state != EC_SLAVE_STATE_SAFEOP
            && current_state != EC_SLAVE_STATE_OP) {
        if (slave->master->debug_level) {
            char str[EC_STATE_STRING_SIZE];
            ec_state_string(current_state, str, 0);
            EC_SLAVE_DBG(slave, 0, "Slave is not in the state"
                    " to do mailbox com (%s), setting to PREOP.\n", str);
        }

        fsm->state = ec_fsm_slave_scan_state_preop;
        ec_slave_request_state(slave, EC_SLAVE_STATE_PREOP);
        ec_fsm_slave_config_start(fsm->fsm_slave_config, slave);
        ec_fsm_slave_config_exec(fsm->fsm_slave_config);
    } else {
        EC_SLAVE_DBG(slave, 1, "Reading mailbox"
                " sync manager configuration.\n");

        /* Scan current sync manager configuration to get configured mailbox
         * sizes. */
        ec_datagram_fprd(fsm->datagram, slave->station_address, 0x0800,
                EC_SYNC_PAGE_SIZE * 2);
        ec_datagram_zero(fsm->datagram);
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_slave_scan_state_sync;
    }
}

/*****************************************************************************/

/** Slave scan state: PREOP.
 */
void ec_fsm_slave_scan_state_preop(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    if (ec_fsm_slave_config_exec(fsm->fsm_slave_config))
        return;

    if (!ec_fsm_slave_config_success(fsm->fsm_slave_config)) {
        fsm->state = ec_fsm_slave_scan_state_error;
        return;
    }

    ec_fsm_slave_scan_enter_clear_mailbox(fsm);
}

/*****************************************************************************/

/** Slave scan state: SYNC.
 */
void ec_fsm_slave_scan_state_sync(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_scan_state_error;
        EC_SLAVE_ERR(slave, "Failed to receive sync manager"
                " configuration datagram: ");
        ec_datagram_print_state(datagram);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_scan_state_error;
        EC_SLAVE_ERR(slave, "Failed to read DL status: ");
        ec_datagram_print_wc_error(datagram);
        return;
    }

    slave->configured_rx_mailbox_offset = EC_READ_U16(datagram->data);
    slave->configured_rx_mailbox_size = EC_READ_U16(datagram->data + 2);
    slave->configured_tx_mailbox_offset = EC_READ_U16(datagram->data + 8);
    slave->configured_tx_mailbox_size = EC_READ_U16(datagram->data + 10);

    EC_SLAVE_DBG(slave, 1, "Mailbox configuration:\n");
    EC_SLAVE_DBG(slave, 1, " RX offset=0x%04x size=%u\n",
            slave->configured_rx_mailbox_offset,
            slave->configured_rx_mailbox_size);
    EC_SLAVE_DBG(slave, 1, " TX offset=0x%04x size=%u\n",
            slave->configured_tx_mailbox_offset,
            slave->configured_tx_mailbox_size);

    if (!slave->sii_image) {
        EC_SLAVE_ERR(slave, "Slave has no SII image attached!\n");
        slave->error_flag = 1;
        fsm->state = ec_fsm_slave_scan_state_error;
        return;
    }

    // allocate memory for mailbox response data for supported mailbox protocols
    ec_mbox_prot_data_prealloc(slave, slave->sii_image->sii.mailbox_protocols, slave->configured_tx_mailbox_size);

    ec_fsm_slave_scan_enter_clear_mailbox(fsm);
}

/*****************************************************************************/

/** Enter slave scan state: Clear Mailbox.
 */
void ec_fsm_slave_scan_enter_clear_mailbox(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_slave_t *slave = fsm->slave;

    // If there is some old data in the slave's mailbox, read it out and
    // discard it. We don't need to check the mailbox first, we just ignore
    // an error or empty mailbox response.
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_mbox_prepare_fetch(fsm->slave, datagram);
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_scan_state_mailbox_cleared;

    slave->valid_mbox_data = 0;
}

/*****************************************************************************/

/** Slave scan state: Mailbox cleared.
 */
void ec_fsm_slave_scan_state_mailbox_cleared(ec_fsm_slave_scan_t *fsm /**< slave state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = fsm->datagram;

#ifdef EC_SII_CACHE
    unsigned int i = 0;
    unsigned int fetch_pdos = 1;
#endif

    if (fsm->datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--) {
        ec_slave_mbox_prepare_fetch(fsm->slave, datagram);
        return;
    }

    if (unlikely(slave->master->debug_level > 0)
        && datagram->state == EC_DATAGRAM_RECEIVED
        && datagram->working_counter == 1)
        EC_SLAVE_INFO(slave, "Cleared old data from the mailbox\n");

    slave->valid_mbox_data = 1;

    if (!slave->sii_image) {
        EC_SLAVE_ERR(slave, "Slave has no SII image attached!\n");
        slave->error_flag = 1;
        fsm->state = ec_fsm_slave_scan_state_error;
        return;
    }

#ifdef EC_SII_CACHE
    if ((slave->effective_alias != 0) || (slave->effective_serial_number != 0)) {
        // SII data has been stored
        for (i = 0; i < slave->sii_image->sii.sync_count; i++) {
            if (!list_empty(&slave->sii_image->sii.syncs[i].pdos.list)) {
                fetch_pdos = 0; // PDOs already fetched
                break;
            }
        }
    }
    if (!fetch_pdos) {
        fsm->state = ec_fsm_slave_scan_state_end;
    }
    else
#endif
    {
        ec_fsm_slave_scan_enter_pdos(fsm);
    }
}

/*****************************************************************************/

/** Enter slave scan state PDOS.
 */
void ec_fsm_slave_scan_enter_pdos(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    ec_slave_t *slave = fsm->slave;

    EC_SLAVE_DBG(slave, 1, "Scanning PDO assignment and mapping.\n");
    fsm->state = ec_fsm_slave_scan_state_pdos;
    ec_fsm_pdo_start_reading(fsm->fsm_pdo, slave);
    ec_fsm_pdo_exec(fsm->fsm_pdo, fsm->datagram); // execute immediately
}

/*****************************************************************************/

/** Slave scan state: PDOS.
 */
void ec_fsm_slave_scan_state_pdos(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
    if (ec_fsm_pdo_exec(fsm->fsm_pdo, fsm->datagram)) {
        return;
    }

    if (!ec_fsm_pdo_success(fsm->fsm_pdo)) {
        fsm->state = ec_fsm_slave_scan_state_error;
        return;
    }

    // reading PDO configuration finished
    fsm->state = ec_fsm_slave_scan_state_end;
}

/******************************************************************************
 * Common state functions
 *****************************************************************************/

/** State: ERROR.
 */
void ec_fsm_slave_scan_state_error(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
}

/*****************************************************************************/

/** State: END.
 */
void ec_fsm_slave_scan_state_end(
        ec_fsm_slave_scan_t *fsm /**< slave state machine */
        )
{
}

/*****************************************************************************/
