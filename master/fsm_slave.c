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
   EtherCAT slave state machines.
*/

/*****************************************************************************/

#include "globals.h"
#include "master.h"
#include "mailbox.h"
#include "fsm_slave.h"
#include "fsm_mapping.h"

/*****************************************************************************/

void ec_fsm_slave_scan_state_start(ec_fsm_slave_t *);
void ec_fsm_slave_scan_state_address(ec_fsm_slave_t *);
void ec_fsm_slave_scan_state_state(ec_fsm_slave_t *);
void ec_fsm_slave_scan_state_base(ec_fsm_slave_t *);
void ec_fsm_slave_scan_state_datalink(ec_fsm_slave_t *);
void ec_fsm_slave_scan_state_eeprom_size(ec_fsm_slave_t *);
void ec_fsm_slave_scan_state_eeprom_data(ec_fsm_slave_t *);

void ec_fsm_slave_conf_state_start(ec_fsm_slave_t *);
void ec_fsm_slave_conf_state_init(ec_fsm_slave_t *);
void ec_fsm_slave_conf_state_clear_fmmus(ec_fsm_slave_t *);
void ec_fsm_slave_conf_state_mbox_sync(ec_fsm_slave_t *);
void ec_fsm_slave_conf_state_preop(ec_fsm_slave_t *);
void ec_fsm_slave_conf_state_pdo_sync(ec_fsm_slave_t *);
void ec_fsm_slave_conf_state_fmmu(ec_fsm_slave_t *);
void ec_fsm_slave_conf_state_sdoconf(ec_fsm_slave_t *);
void ec_fsm_slave_conf_state_mapconf(ec_fsm_slave_t *);
void ec_fsm_slave_conf_state_saveop(ec_fsm_slave_t *);
void ec_fsm_slave_conf_state_op(ec_fsm_slave_t *);

void ec_fsm_slave_conf_enter_mbox_sync(ec_fsm_slave_t *);
void ec_fsm_slave_conf_enter_preop(ec_fsm_slave_t *);
void ec_fsm_slave_conf_enter_pdo_sync(ec_fsm_slave_t *);
void ec_fsm_slave_conf_enter_fmmu(ec_fsm_slave_t *);
void ec_fsm_slave_conf_enter_sdoconf(ec_fsm_slave_t *);
void ec_fsm_slave_conf_enter_mapconf(ec_fsm_slave_t *);
void ec_fsm_slave_conf_enter_saveop(ec_fsm_slave_t *);

void ec_fsm_slave_state_end(ec_fsm_slave_t *);
void ec_fsm_slave_state_error(ec_fsm_slave_t *);

/*****************************************************************************/

/**
   Constructor.
*/

void ec_fsm_slave_init(ec_fsm_slave_t *fsm, /**< slave state machine */
        ec_datagram_t *datagram /**< datagram structure to use */
        )
{
    fsm->datagram = datagram;

    // init sub state machines
    ec_fsm_sii_init(&fsm->fsm_sii, fsm->datagram);
    ec_fsm_change_init(&fsm->fsm_change, fsm->datagram);
    ec_fsm_coe_init(&fsm->fsm_coe, fsm->datagram);
    ec_fsm_mapping_init(&fsm->fsm_map, &fsm->fsm_coe);
}

/*****************************************************************************/

/**
   Destructor.
*/

void ec_fsm_slave_clear(ec_fsm_slave_t *fsm /**< slave state machine */)
{
    // clear sub state machines
    ec_fsm_sii_clear(&fsm->fsm_sii);
    ec_fsm_change_clear(&fsm->fsm_change);
    ec_fsm_coe_clear(&fsm->fsm_coe);
    ec_fsm_mapping_clear(&fsm->fsm_map);
}

/*****************************************************************************/

/**
 * Start slave scan state machine.
 */

void ec_fsm_slave_start_scan(ec_fsm_slave_t *fsm, /**< slave state machine */
        ec_slave_t *slave /**< slave to configure */
        )
{
    fsm->slave = slave;
    fsm->state = ec_fsm_slave_scan_state_start;
}

/*****************************************************************************/

/**
 * Start slave configuration state machine.
 */

void ec_fsm_slave_start_conf(ec_fsm_slave_t *fsm, /**< slave state machine */
        ec_slave_t *slave /**< slave to configure */
        )
{
    fsm->slave = slave;
    fsm->state = ec_fsm_slave_conf_state_start;
}

/*****************************************************************************/

/**
   \return false, if state machine has terminated
*/

int ec_fsm_slave_running(const ec_fsm_slave_t *fsm /**< slave state machine */)
{
    return fsm->state != ec_fsm_slave_state_end
        && fsm->state != ec_fsm_slave_state_error;
}

/*****************************************************************************/

/**
   Executes the current state of the state machine.
   If the state machine's datagram is not sent or received yet, the execution
   of the state machine is delayed to the next cycle.
   \return false, if state machine has terminated
*/

int ec_fsm_slave_exec(ec_fsm_slave_t *fsm /**< slave state machine */)
{
    if (fsm->datagram->state == EC_DATAGRAM_SENT
        || fsm->datagram->state == EC_DATAGRAM_QUEUED) {
        // datagram was not sent or received yet.
        return ec_fsm_slave_running(fsm);
    }

    fsm->state(fsm);
    return ec_fsm_slave_running(fsm);
}

/*****************************************************************************/

/**
   \return true, if the state machine terminated gracefully
*/

int ec_fsm_slave_success(const ec_fsm_slave_t *fsm /**< slave state machine */)
{
    return fsm->state == ec_fsm_slave_state_end;
}

/******************************************************************************
 *  slave scan state machine
 *****************************************************************************/

/**
   Slave scan state: START.
   First state of the slave state machine. Writes the station address to the
   slave, according to its ring position.
*/

void ec_fsm_slave_scan_state_start(ec_fsm_slave_t *fsm /**< slave state machine */)
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

void ec_fsm_slave_scan_state_address(ec_fsm_slave_t *fsm /**< slave state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_state_error;
        EC_ERR("Failed to receive station address datagram for slave %i"
                " (datagram state %i)\n",
                fsm->slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_state_error;
        EC_ERR("Failed to write station address on slave %i: ",
               fsm->slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    // Read AL state
    ec_datagram_nprd(datagram, fsm->slave->station_address, 0x0130, 2);
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_scan_state_state;
}

/*****************************************************************************/

/**
   Slave scan state: STATE.
*/

void ec_fsm_slave_scan_state_state(
        ec_fsm_slave_t *fsm /**< slave state machine */
        )
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_state_error;
        EC_ERR("Failed to receive AL state datagram from slave %i"
                " (datagram state %i).\n",
               fsm->slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_state_error;
        EC_ERR("Failed to read AL state of slave %i: ",
               fsm->slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    slave->current_state = EC_READ_U8(datagram->data);
    if (slave->current_state & EC_SLAVE_STATE_ACK_ERR) {
        char state_str[EC_STATE_STRING_SIZE];
        ec_state_string(slave->current_state, state_str);
        EC_WARN("Slave %i has state error bit set (%s)!\n",
                slave->ring_position, state_str);
    }

    // read base data
    ec_datagram_nprd(datagram, fsm->slave->station_address, 0x0000, 6);
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_scan_state_base;
}

/*****************************************************************************/

/**
   Slave scan state: BASE.
*/

void ec_fsm_slave_scan_state_base(ec_fsm_slave_t *fsm /**< slave state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_state_error;
        EC_ERR("Failed to receive base data datagram for slave %i"
                " (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_state_error;
        EC_ERR("Failed to read base data from slave %i: ",
               slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    slave->base_type       = EC_READ_U8 (datagram->data);
    slave->base_revision   = EC_READ_U8 (datagram->data + 1);
    slave->base_build      = EC_READ_U16(datagram->data + 2);
    slave->base_fmmu_count = EC_READ_U8 (datagram->data + 4);

    if (slave->base_fmmu_count > EC_MAX_FMMUS)
        slave->base_fmmu_count = EC_MAX_FMMUS;

    // read data link status
    ec_datagram_nprd(datagram, slave->station_address, 0x0110, 2);
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_scan_state_datalink;
}

/*****************************************************************************/

/**
   Slave scan state: DATALINK.
*/

void ec_fsm_slave_scan_state_datalink(ec_fsm_slave_t *fsm /**< slave state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    uint16_t dl_status;
    unsigned int i;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_state_error;
        EC_ERR("Failed to receive DL status datagram from slave %i"
                " (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_state_error;
        EC_ERR("Failed to read DL status from slave %i: ",
               slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    dl_status = EC_READ_U16(datagram->data);
    for (i = 0; i < 4; i++) {
        slave->dl_link[i] = dl_status & (1 << (4 + i)) ? 1 : 0;
        slave->dl_loop[i] = dl_status & (1 << (8 + i * 2)) ? 1 : 0;
        slave->dl_signal[i] = dl_status & (1 << (9 + i * 2)) ? 1 : 0;
    }

    // Start fetching EEPROM size

    fsm->sii_offset = EC_FIRST_EEPROM_CATEGORY_OFFSET; // first category header
    ec_fsm_sii_read(&fsm->fsm_sii, slave, fsm->sii_offset, EC_FSM_SII_NODE);
    fsm->state = ec_fsm_slave_scan_state_eeprom_size;
    fsm->state(fsm); // execute state immediately
}

/*****************************************************************************/

/**
   Slave scan state: EEPROM SIZE.
*/

void ec_fsm_slave_scan_state_eeprom_size(ec_fsm_slave_t *fsm /**< slave state machine */)
{
    ec_slave_t *slave = fsm->slave;
    uint16_t cat_type, cat_size;

    if (ec_fsm_sii_exec(&fsm->fsm_sii)) return;

    if (!ec_fsm_sii_success(&fsm->fsm_sii)) {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_state_error;
        EC_ERR("Failed to read EEPROM size of slave %i.\n",
               slave->ring_position);
        return;
    }

    cat_type = EC_READ_U16(fsm->fsm_sii.value);
    cat_size = EC_READ_U16(fsm->fsm_sii.value + 2);

    if (cat_type != 0xFFFF) { // not the last category
        off_t next_offset = 2UL + fsm->sii_offset + cat_size;
        if (next_offset >= EC_MAX_EEPROM_SIZE) {
            EC_WARN("EEPROM size of slave %i exceeds"
                    " %u words (0xffff limiter missing?).\n",
                    slave->ring_position, EC_MAX_EEPROM_SIZE);
            slave->eeprom_size = EC_FIRST_EEPROM_CATEGORY_OFFSET * 2;
            goto alloc_eeprom;
        }
        fsm->sii_offset = next_offset;
        ec_fsm_sii_read(&fsm->fsm_sii, slave, fsm->sii_offset,
                        EC_FSM_SII_NODE);
        ec_fsm_sii_exec(&fsm->fsm_sii); // execute state immediately
        return;
    }

    slave->eeprom_size = (fsm->sii_offset + 1) * 2;

alloc_eeprom:
    if (slave->eeprom_data) {
        EC_WARN("Freeing old EEPROM data on slave %i...\n",
                slave->ring_position);
        kfree(slave->eeprom_data);
    }

    if (!(slave->eeprom_data =
                (uint8_t *) kmalloc(slave->eeprom_size, GFP_ATOMIC))) {
        EC_ERR("Failed to allocate %u bytes of EEPROM data for slave %u.\n",
               slave->eeprom_size, slave->ring_position);
        slave->eeprom_size = 0;
        slave->error_flag = 1;
        fsm->state = ec_fsm_slave_state_error;
        return;
    }

    // Start fetching EEPROM contents

    fsm->state = ec_fsm_slave_scan_state_eeprom_data;
    fsm->sii_offset = 0x0000;
    ec_fsm_sii_read(&fsm->fsm_sii, slave, fsm->sii_offset, EC_FSM_SII_NODE);
    ec_fsm_sii_exec(&fsm->fsm_sii); // execute state immediately
}

/*****************************************************************************/

/**
   Slave scan state: EEPROM DATA.
*/

void ec_fsm_slave_scan_state_eeprom_data(ec_fsm_slave_t *fsm /**< slave state machine */)
{
    ec_slave_t *slave = fsm->slave;
    uint16_t *cat_word, cat_type, cat_size, eeprom_word_size = slave->eeprom_size / 2;

    if (ec_fsm_sii_exec(&fsm->fsm_sii)) return;

    if (!ec_fsm_sii_success(&fsm->fsm_sii)) {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_state_error;
        EC_ERR("Failed to fetch EEPROM contents of slave %i.\n",
               slave->ring_position);
        return;
    }

    // 2 words fetched

    if (fsm->sii_offset + 2 <= eeprom_word_size) { // 2 words fit
        memcpy(slave->eeprom_data + fsm->sii_offset * 2,
               fsm->fsm_sii.value, 4);
    }
    else { // copy the last word
        memcpy(slave->eeprom_data + fsm->sii_offset * 2,
               fsm->fsm_sii.value, 2);
    }

    if (fsm->sii_offset + 2 < eeprom_word_size) {
        // fetch the next 2 words
        fsm->sii_offset += 2;
        ec_fsm_sii_read(&fsm->fsm_sii, slave, fsm->sii_offset,
                        EC_FSM_SII_NODE);
        ec_fsm_sii_exec(&fsm->fsm_sii); // execute state immediately
        return;
    }

    // Evaluate EEPROM contents

    slave->sii_alias =
        EC_READ_U16(slave->eeprom_data + 2 * 0x0004);
    slave->sii_vendor_id =
        EC_READ_U32(slave->eeprom_data + 2 * 0x0008);
    slave->sii_product_code =
        EC_READ_U32(slave->eeprom_data + 2 * 0x000A);
    slave->sii_revision_number =
        EC_READ_U32(slave->eeprom_data + 2 * 0x000C);
    slave->sii_serial_number =
        EC_READ_U32(slave->eeprom_data + 2 * 0x000E);
    slave->sii_rx_mailbox_offset =
        EC_READ_U16(slave->eeprom_data + 2 * 0x0018);
    slave->sii_rx_mailbox_size =
        EC_READ_U16(slave->eeprom_data + 2 * 0x0019);
    slave->sii_tx_mailbox_offset =
        EC_READ_U16(slave->eeprom_data + 2 * 0x001A);
    slave->sii_tx_mailbox_size =
        EC_READ_U16(slave->eeprom_data + 2 * 0x001B);
    slave->sii_mailbox_protocols =
        EC_READ_U16(slave->eeprom_data + 2 * 0x001C);

    if (eeprom_word_size < EC_FIRST_EEPROM_CATEGORY_OFFSET + 1) {
        EC_ERR("Unexpected end of EEPROM data in slave %u:"
                " First category header missing.\n",
                slave->ring_position);
        goto end;
    }

    // evaluate category data
    cat_word =
        (uint16_t *) slave->eeprom_data + EC_FIRST_EEPROM_CATEGORY_OFFSET;
    while (EC_READ_U16(cat_word) != 0xFFFF) {

        // type and size words must fit
        if (cat_word + 2 - (uint16_t *) slave->eeprom_data
                > eeprom_word_size) {
            EC_ERR("Unexpected end of EEPROM data in slave %u:"
                    " Category header incomplete.\n",
                    slave->ring_position);
            goto end;
        }

        cat_type = EC_READ_U16(cat_word) & 0x7FFF;
        cat_size = EC_READ_U16(cat_word + 1);
        cat_word += 2;

        if (cat_word + cat_size - (uint16_t *) slave->eeprom_data
                > eeprom_word_size) {
            EC_WARN("Unexpected end of EEPROM data in slave %u:"
                    " Category data incomplete.\n",
                    slave->ring_position);
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
                            cat_size * 2, EC_TX_PDO))
                    goto end;
                break;
            case 0x0033:
                if (ec_slave_fetch_sii_pdos( slave, (uint8_t *) cat_word,
                            cat_size * 2, EC_RX_PDO))
                    goto end;
                break;
            default:
                if (fsm->slave->master->debug_level)
                    EC_WARN("Unknown category type 0x%04X in slave %i.\n",
                            cat_type, slave->ring_position);
        }

        cat_word += cat_size;
        if (cat_word - (uint16_t *) slave->eeprom_data >= eeprom_word_size) {
            EC_WARN("Unexpected end of EEPROM data in slave %u:"
                    " Next category header missing.\n",
                    slave->ring_position);
            goto end;
        }
    }

    fsm->state = ec_fsm_slave_state_end;
    return;

end:
    EC_ERR("Failed to analyze category data.\n");
    fsm->slave->error_flag = 1;
    fsm->state = ec_fsm_slave_state_error;
}

/******************************************************************************
 *  slave configuration state machine
 *****************************************************************************/

/**
   Slave configuration state: START.
*/

void ec_fsm_slave_conf_state_start(ec_fsm_slave_t *fsm /**< slave state machine */)
{
    if (fsm->slave->master->debug_level) {
        EC_DBG("Configuring slave %i...\n", fsm->slave->ring_position);
    }

    ec_fsm_change_start(&fsm->fsm_change, fsm->slave, EC_SLAVE_STATE_INIT);
    ec_fsm_change_exec(&fsm->fsm_change);
    fsm->state = ec_fsm_slave_conf_state_init;
}

/*****************************************************************************/

/**
   Slave configuration state: INIT.
*/

void ec_fsm_slave_conf_state_init(ec_fsm_slave_t *fsm /**< slave state machine */)
{
    ec_master_t *master = fsm->slave->master;
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = fsm->datagram;

    if (ec_fsm_change_exec(&fsm->fsm_change)) return;

    if (!ec_fsm_change_success(&fsm->fsm_change)) {
        if (!fsm->fsm_change.spontaneous_change)
            slave->error_flag = 1;
        fsm->state = ec_fsm_slave_state_error;
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
        ec_fsm_slave_conf_enter_mbox_sync(fsm);
        return;
    }

    if (master->debug_level)
        EC_DBG("Clearing FMMU configurations of slave %i...\n",
               slave->ring_position);

    // clear FMMU configurations
    ec_datagram_npwr(datagram, slave->station_address,
                     0x0600, EC_FMMU_SIZE * slave->base_fmmu_count);
    memset(datagram->data, 0x00, EC_FMMU_SIZE * slave->base_fmmu_count);
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_conf_state_clear_fmmus;
}

/*****************************************************************************/

/**
   Slave configuration state: CLEAR FMMU.
*/

void ec_fsm_slave_conf_state_clear_fmmus(ec_fsm_slave_t *fsm
                                        /**< slave state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_state_error;
        EC_ERR("Failed receive FMMU clearing datagram for slave %i.\n",
               fsm->slave->ring_position);
        return;
    }

    if (datagram->working_counter != 1) {
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_state_error;
        EC_ERR("Failed to clear FMMUs of slave %i: ",
               fsm->slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    ec_fsm_slave_conf_enter_mbox_sync(fsm);
}

/*****************************************************************************/

/**
 */

void ec_fsm_slave_conf_enter_mbox_sync(
        ec_fsm_slave_t *fsm /**< slave state machine */
        )
{
    ec_master_t *master = fsm->slave->master;
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = fsm->datagram;
    unsigned int i;

    // slave is now in INIT
    if (slave->current_state == slave->requested_state) {
        fsm->state = ec_fsm_slave_state_end; // successful
        if (master->debug_level) {
            EC_DBG("Finished configuration of slave %i.\n",
                   slave->ring_position);
        }
        return;
    }

    if (!slave->sii_mailbox_protocols) {
        // no mailbox protocols supported
        if (master->debug_level)
            EC_DBG("Slave %i does not support mailbox communication.\n",
                    slave->ring_position);
        ec_fsm_slave_conf_enter_preop(fsm);
        return;
    }

    if (master->debug_level) {
        EC_DBG("Configuring mailbox sync managers of slave %i.\n",
               slave->ring_position);
    }

    if (slave->sii_sync_count >= 2) {
        // configure sync managers
        ec_datagram_npwr(datagram, slave->station_address, 0x0800,
                EC_SYNC_SIZE * slave->sii_sync_count);
        memset(datagram->data, 0x00, EC_SYNC_SIZE * slave->sii_sync_count);

        for (i = 0; i < 2; i++) {
            ec_sync_config(&slave->sii_syncs[i],
                    datagram->data + EC_SYNC_SIZE * i);
        }
    } else { // no mailbox sync manager configurations provided
        ec_sync_t sync;

        if (master->debug_level)
            EC_DBG("Slave %i does not provide"
                    " mailbox sync manager configurations.\n",
                    slave->ring_position);

        ec_datagram_npwr(datagram, slave->station_address, 0x0800,
                EC_SYNC_SIZE * 2);
        memset(datagram->data, 0x00, EC_SYNC_SIZE * 2);

        ec_sync_init(&sync, slave, 0);
        sync.physical_start_address = slave->sii_rx_mailbox_offset;
        sync.length = slave->sii_rx_mailbox_size;
        sync.control_register = 0x26;
        sync.enable = 1;
        ec_sync_config(&sync, datagram->data + EC_SYNC_SIZE * sync.index);

        ec_sync_init(&sync, slave, 1);
        sync.physical_start_address = slave->sii_tx_mailbox_offset;
        sync.length = slave->sii_tx_mailbox_size;
        sync.control_register = 0x22;
        sync.enable = 1;
        ec_sync_config(&sync, datagram->data + EC_SYNC_SIZE * sync.index);
    }

    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_conf_state_mbox_sync;
}

/*****************************************************************************/

/**
   Slave configuration state: SYNC.
*/

void ec_fsm_slave_conf_state_mbox_sync(ec_fsm_slave_t *fsm /**< slave state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_state_error;
        EC_ERR("Failed to receive sync manager configuration datagram for"
               " slave %i (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        slave->error_flag = 1;
        fsm->state = ec_fsm_slave_state_error;
        EC_ERR("Failed to set sync managers of slave %i: ",
               slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    ec_fsm_slave_conf_enter_preop(fsm);
}

/*****************************************************************************/

/**
 */

void ec_fsm_slave_conf_enter_preop(ec_fsm_slave_t *fsm /**< slave state machine */)
{
    fsm->state = ec_fsm_slave_conf_state_preop;
    ec_fsm_change_start(&fsm->fsm_change, fsm->slave, EC_SLAVE_STATE_PREOP);
    ec_fsm_change_exec(&fsm->fsm_change); // execute immediately
}

/*****************************************************************************/

/**
   Slave configuration state: PREOP.
*/

void ec_fsm_slave_conf_state_preop(ec_fsm_slave_t *fsm /**< slave state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_master_t *master = fsm->slave->master;

    if (ec_fsm_change_exec(&fsm->fsm_change)) return;

    if (!ec_fsm_change_success(&fsm->fsm_change)) {
        if (!fsm->fsm_change.spontaneous_change)
            slave->error_flag = 1;
        fsm->state = ec_fsm_slave_state_error;
        return;
    }

    // slave is now in PREOP
    slave->jiffies_preop = fsm->datagram->jiffies_received;

    if (master->debug_level) {
        EC_DBG("Slave %i is now in PREOP.\n", slave->ring_position);
    }

    if (slave->current_state == slave->requested_state) {
        fsm->state = ec_fsm_slave_state_end; // successful
        if (master->debug_level) {
            EC_DBG("Finished configuration of slave %i.\n",
                   slave->ring_position);
        }
        return;
    }

    ec_fsm_slave_conf_enter_sdoconf(fsm);
}

/*****************************************************************************/

/**
 */

void ec_fsm_slave_conf_enter_sdoconf(ec_fsm_slave_t *fsm /**< slave state machine */)
{
    ec_slave_t *slave = fsm->slave;

    // No CoE configuration to be applied?
    if (list_empty(&slave->sdo_confs)) { // skip SDO configuration
        ec_fsm_slave_conf_enter_mapconf(fsm);
        return;
    }

    // start SDO configuration
    fsm->state = ec_fsm_slave_conf_state_sdoconf;
    fsm->sdodata = list_entry(fsm->slave->sdo_confs.next, ec_sdo_data_t, list);
    ec_fsm_coe_download(&fsm->fsm_coe, fsm->slave, fsm->sdodata);
    ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
}

/*****************************************************************************/

/**
   Slave configuration state: SDOCONF.
*/

void ec_fsm_slave_conf_state_sdoconf(
        ec_fsm_slave_t *fsm /**< slave state machine */
        )
{
    if (ec_fsm_coe_exec(&fsm->fsm_coe)) return;

    if (!ec_fsm_coe_success(&fsm->fsm_coe)) {
        EC_ERR("SDO configuration failed for slave %u.\n",
                fsm->slave->ring_position);
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_state_error;
        return;
    }

    // Another SDO to configure?
    if (fsm->sdodata->list.next != &fsm->slave->sdo_confs) {
        fsm->sdodata = list_entry(fsm->sdodata->list.next,
                                  ec_sdo_data_t, list);
        ec_fsm_coe_download(&fsm->fsm_coe, fsm->slave, fsm->sdodata);
        ec_fsm_coe_exec(&fsm->fsm_coe); // execute immediately
        return;
    }

    // All SDOs are now configured.
    ec_fsm_slave_conf_enter_mapconf(fsm);
}

/*****************************************************************************/

/**
 */

void ec_fsm_slave_conf_enter_mapconf(
        ec_fsm_slave_t *fsm /**< slave state machine */
        )
{
    ec_slave_t *slave = fsm->slave;

    if (!(slave->sii_mailbox_protocols & EC_MBOX_COE)) {
        // Slave does not support CoE: no configuration of PDO mapping.
        ec_fsm_slave_conf_enter_pdo_sync(fsm);
        return;
    }

    // start configuring PDO mapping
    fsm->state = ec_fsm_slave_conf_state_mapconf;
    ec_fsm_mapping_start(&fsm->fsm_map, fsm->slave);
    ec_fsm_mapping_exec(&fsm->fsm_map); // execute immediately
}

/*****************************************************************************/

/**
   Slave configuration state: MAPCONF.
*/

void ec_fsm_slave_conf_state_mapconf(
        ec_fsm_slave_t *fsm /**< slave state machine */
        )
{
    if (ec_fsm_mapping_exec(&fsm->fsm_map)) return;

    if (!ec_fsm_mapping_success(&fsm->fsm_map)) {
        EC_ERR("PDO mapping configuration failed for slave %u.\n",
                fsm->slave->ring_position);
        fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_state_error;
        return;
    }

    ec_fsm_slave_conf_enter_pdo_sync(fsm);
}

/*****************************************************************************/

/**
 */

void ec_fsm_slave_conf_enter_pdo_sync(
        ec_fsm_slave_t *fsm /**< slave state machine */
        )
{
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = fsm->datagram;
    unsigned int i;

    if (!slave->sii_sync_count) {
        ec_fsm_slave_conf_enter_fmmu(fsm);
        return;
    }

    // configure sync managers for process data
    ec_datagram_npwr(datagram, slave->station_address, 0x0800,
                     EC_SYNC_SIZE * slave->sii_sync_count);
    memset(datagram->data, 0x00, EC_SYNC_SIZE * slave->sii_sync_count);

    for (i = 0; i < slave->sii_sync_count; i++) {
        ec_sync_config(&slave->sii_syncs[i],
                datagram->data + EC_SYNC_SIZE * i);
    }

    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_conf_state_pdo_sync;
}

/*****************************************************************************/

/**
 */

void ec_fsm_slave_conf_state_pdo_sync(ec_fsm_slave_t *fsm /**< slave state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_state_error;
        EC_ERR("Failed to receive process data sync manager configuration"
               " datagram for slave %i (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        slave->error_flag = 1;
        fsm->state = ec_fsm_slave_state_error;
        EC_ERR("Failed to set process data sync managers of slave %i: ",
                slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    ec_fsm_slave_conf_enter_fmmu(fsm);
}

/*****************************************************************************/

/**
*/

void ec_fsm_slave_conf_enter_fmmu(ec_fsm_slave_t *fsm /**< slave state machine */)
{
    ec_slave_t *slave = fsm->slave;
    ec_datagram_t *datagram = fsm->datagram;
    unsigned int j;

    if (!slave->base_fmmu_count) { // skip FMMU configuration
        ec_fsm_slave_conf_enter_saveop(fsm);
        return;
    }

    // configure FMMUs
    ec_datagram_npwr(datagram, slave->station_address,
                     0x0600, EC_FMMU_SIZE * slave->base_fmmu_count);
    memset(datagram->data, 0x00, EC_FMMU_SIZE * slave->base_fmmu_count);
    for (j = 0; j < slave->fmmu_count; j++) {
        ec_fmmu_config(&slave->fmmus[j], datagram->data + EC_FMMU_SIZE * j);
    }

    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_slave_conf_state_fmmu;
}

/*****************************************************************************/

/**
   Slave configuration state: FMMU.
*/

void ec_fsm_slave_conf_state_fmmu(ec_fsm_slave_t *fsm /**< slave state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_slave_state_error;
        EC_ERR("Failed to receive FMMUs datagram for slave %i"
                " (datagram state %i).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        slave->error_flag = 1;
        fsm->state = ec_fsm_slave_state_error;
        EC_ERR("Failed to set FMMUs of slave %i: ",
               slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    ec_fsm_slave_conf_enter_saveop(fsm);
}

/*****************************************************************************/

/**
 */

void ec_fsm_slave_conf_enter_saveop(ec_fsm_slave_t *fsm /**< slave state machine */)
{
    fsm->state = ec_fsm_slave_conf_state_saveop;
    ec_fsm_change_start(&fsm->fsm_change, fsm->slave, EC_SLAVE_STATE_SAVEOP);
    ec_fsm_change_exec(&fsm->fsm_change); // execute immediately
}

/*****************************************************************************/

/**
   Slave configuration state: SAVEOP.
*/

void ec_fsm_slave_conf_state_saveop(ec_fsm_slave_t *fsm /**< slave state machine */)
{
    ec_master_t *master = fsm->slave->master;
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_change_exec(&fsm->fsm_change)) return;

    if (!ec_fsm_change_success(&fsm->fsm_change)) {
        if (!fsm->fsm_change.spontaneous_change)
            fsm->slave->error_flag = 1;
        fsm->state = ec_fsm_slave_state_error;
        return;
    }

    // slave is now in SAVEOP

    if (master->debug_level) {
        EC_DBG("Slave %i is now in SAVEOP.\n", slave->ring_position);
    }

    if (fsm->slave->current_state == fsm->slave->requested_state) {
        fsm->state = ec_fsm_slave_state_end; // successful
        if (master->debug_level) {
            EC_DBG("Finished configuration of slave %i.\n",
                   slave->ring_position);
        }
        return;
    }

    // set state to OP
    fsm->state = ec_fsm_slave_conf_state_op;
    ec_fsm_change_start(&fsm->fsm_change, slave, EC_SLAVE_STATE_OP);
    ec_fsm_change_exec(&fsm->fsm_change); // execute immediately
}

/*****************************************************************************/

/**
   Slave configuration state: OP
*/

void ec_fsm_slave_conf_state_op(ec_fsm_slave_t *fsm /**< slave state machine */)
{
    ec_master_t *master = fsm->slave->master;
    ec_slave_t *slave = fsm->slave;

    if (ec_fsm_change_exec(&fsm->fsm_change)) return;

    if (!ec_fsm_change_success(&fsm->fsm_change)) {
        if (!fsm->fsm_change.spontaneous_change)
            slave->error_flag = 1;
        fsm->state = ec_fsm_slave_state_error;
        return;
    }

    // slave is now in OP

    if (master->debug_level) {
        EC_DBG("Slave %i is now in OP.\n", slave->ring_position);
        EC_DBG("Finished configuration of slave %i.\n", slave->ring_position);
    }

    fsm->state = ec_fsm_slave_state_end; // successful
}

/******************************************************************************
 *  Common state functions
 *****************************************************************************/

/**
   State: ERROR.
*/

void ec_fsm_slave_state_error(ec_fsm_slave_t *fsm /**< slave state machine */)
{
}

/*****************************************************************************/

/**
   State: END.
*/

void ec_fsm_slave_state_end(ec_fsm_slave_t *fsm /**< slave state machine */)
{
}

/*****************************************************************************/
