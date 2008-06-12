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
   EtherCAT slave methods.
*/

/*****************************************************************************/

#include <linux/module.h>
#include <linux/delay.h>

#include "globals.h"
#include "datagram.h"
#include "master.h"
#include "slave_config.h"

#include "slave.h"

/*****************************************************************************/

extern const ec_code_msg_t al_status_messages[];

/*****************************************************************************/

char *ec_slave_sii_string(ec_slave_t *, unsigned int);

/*****************************************************************************/

/**
   Slave constructor.
   \return 0 in case of success, else < 0
*/

void ec_slave_init(
        ec_slave_t *slave, /**< EtherCAT slave */
        ec_master_t *master, /**< EtherCAT master */
        uint16_t ring_position, /**< ring position */
        uint16_t station_address /**< station address to configure */
        )
{
    unsigned int i;

    slave->master = master;
    slave->ring_position = ring_position;
    slave->station_address = station_address;

    slave->config = NULL;
    slave->requested_state = EC_SLAVE_STATE_PREOP;
    slave->current_state = EC_SLAVE_STATE_UNKNOWN;
    slave->error_flag = 0;
    slave->force_config = 0;

    slave->base_type = 0;
    slave->base_revision = 0;
    slave->base_build = 0;
    slave->base_fmmu_count = 0;

    for (i = 0; i < EC_SLAVE_MAX_PORTS; i++) {
        slave->ports[i].dl_link = 0;
        slave->ports[i].dl_loop = 0;
        slave->ports[i].dl_signal = 0;
        slave->sii.physical_layer[i] = 0xFF;
    }

    slave->sii_words = NULL;
    slave->sii_nwords = 0;

    slave->sii.alias = 0;
    slave->sii.vendor_id = 0;
    slave->sii.product_code = 0;
    slave->sii.revision_number = 0;
    slave->sii.serial_number = 0;
    slave->sii.rx_mailbox_offset = 0;
    slave->sii.rx_mailbox_size = 0;
    slave->sii.tx_mailbox_offset = 0;
    slave->sii.tx_mailbox_size = 0;
    slave->sii.mailbox_protocols = 0;

    slave->sii.strings = NULL;
    slave->sii.string_count = 0;

    slave->sii.has_general = 0;
    slave->sii.group = NULL;
    slave->sii.image = NULL;
    slave->sii.order = NULL;
    slave->sii.name = NULL;
    memset(&slave->sii.coe_details, 0x00, sizeof(ec_sii_coe_details_t));
    memset(&slave->sii.general_flags, 0x00, sizeof(ec_sii_general_flags_t));
    slave->sii.current_on_ebus = 0;

    slave->sii.syncs = NULL;
    slave->sii.sync_count = 0;

    INIT_LIST_HEAD(&slave->sii.pdos);

    INIT_LIST_HEAD(&slave->sdo_dictionary);

    slave->sdo_dictionary_fetched = 0;
    slave->jiffies_preop = 0;
}

/*****************************************************************************/

/**
   Slave destructor.
   Clears and frees a slave object.
*/

void ec_slave_clear(ec_slave_t *slave /**< EtherCAT slave */)
{
    ec_sdo_t *sdo, *next_sdo;
    unsigned int i;
    ec_pdo_t *pdo, *next_pdo;

    if (slave->config)
        ec_slave_config_detach(slave->config);

    // free all Sdos
    list_for_each_entry_safe(sdo, next_sdo, &slave->sdo_dictionary, list) {
        list_del(&sdo->list);
        ec_sdo_clear(sdo);
        kfree(sdo);
    }

    // free all strings
    if (slave->sii.strings) {
        for (i = 0; i < slave->sii.string_count; i++)
            kfree(slave->sii.strings[i]);
        kfree(slave->sii.strings);
    }

    // free all sync managers
    ec_slave_clear_sync_managers(slave);

    // free all SII Pdos
    list_for_each_entry_safe(pdo, next_pdo, &slave->sii.pdos, list) {
        list_del(&pdo->list);
        ec_pdo_clear(pdo);
        kfree(pdo);
    }

    if (slave->sii_words)
        kfree(slave->sii_words);
}

/*****************************************************************************/

/** Clear the sync manager array. 
 */
void ec_slave_clear_sync_managers(ec_slave_t *slave /**< EtherCAT slave. */)
{
    unsigned int i;

    if (slave->sii.syncs) {
        for (i = 0; i < slave->sii.sync_count; i++) {
            ec_sync_clear(&slave->sii.syncs[i]);
        }
        kfree(slave->sii.syncs);
        slave->sii.syncs = NULL;
    }
}

/*****************************************************************************/

/**
 * Sets the application state of a slave.
 */

void ec_slave_set_state(ec_slave_t *slave, /**< EtherCAT slave */
        ec_slave_state_t new_state /**< new application state */
        )
{
    if (new_state != slave->current_state) {
        if (slave->master->debug_level) {
            char old_state[EC_STATE_STRING_SIZE],
                cur_state[EC_STATE_STRING_SIZE];
            ec_state_string(slave->current_state, old_state);
            ec_state_string(new_state, cur_state);
            EC_DBG("Slave %u: %s -> %s.\n",
                   slave->ring_position, old_state, cur_state);
        }
        slave->current_state = new_state;
    }
}

/*****************************************************************************/

/**
 * Request a slave state and resets the error flag.
 */

void ec_slave_request_state(ec_slave_t *slave, /**< EtherCAT slave */
                            ec_slave_state_t state /**< new state */
                            )
{
    slave->requested_state = state;
    slave->error_flag = 0;
}

/*****************************************************************************/

/**
   Fetches data from a STRING category.
   \todo range checking
   \return 0 in case of success, else < 0
*/

int ec_slave_fetch_sii_strings(
        ec_slave_t *slave, /**< EtherCAT slave */
        const uint8_t *data, /**< category data */
        size_t data_size /**< number of bytes */
        )
{
    int i;
    size_t size;
    off_t offset;

    slave->sii.string_count = data[0];

    if (!slave->sii.string_count)
        return 0;

    if (!(slave->sii.strings =
                kmalloc(sizeof(char *) * slave->sii.string_count,
                    GFP_KERNEL))) {
        EC_ERR("Failed to allocate string array memory.\n");
        goto out_zero;
    }

    offset = 1;
    for (i = 0; i < slave->sii.string_count; i++) {
        size = data[offset];
        // allocate memory for string structure and data at a single blow
        if (!(slave->sii.strings[i] =
                    kmalloc(sizeof(char) * size + 1, GFP_KERNEL))) {
            EC_ERR("Failed to allocate string memory.\n");
            goto out_free;
        }
        memcpy(slave->sii.strings[i], data + offset + 1, size);
        slave->sii.strings[i][size] = 0x00; // append binary zero
        offset += 1 + size;
    }

    return 0;

out_free:
    for (i--; i >= 0; i--) kfree(slave->sii.strings[i]);
    kfree(slave->sii.strings);
    slave->sii.strings = NULL;
out_zero:
    slave->sii.string_count = 0;
    return -1;
}

/*****************************************************************************/

/**
   Fetches data from a GENERAL category.
   \return 0 in case of success, else < 0
*/

int ec_slave_fetch_sii_general(
        ec_slave_t *slave, /**< EtherCAT slave */
        const uint8_t *data, /**< category data */
        size_t data_size /**< size in bytes */
        )
{
    unsigned int i;
    uint8_t flags;

    if (data_size != 32) {
        EC_ERR("Wrong size of general category (%u/32) in slave %u.\n",
                data_size, slave->ring_position);
        return -1;
    }

    slave->sii.group = ec_slave_sii_string(slave, data[0]);
    slave->sii.image = ec_slave_sii_string(slave, data[1]);
    slave->sii.order = ec_slave_sii_string(slave, data[2]);
    slave->sii.name = ec_slave_sii_string(slave, data[3]);

    for (i = 0; i < 4; i++)
        slave->sii.physical_layer[i] =
            (data[4] & (0x03 << (i * 2))) >> (i * 2);

    // read CoE details
    flags = EC_READ_U8(data + 5);
    slave->sii.coe_details.enable_sdo =                 (flags >> 0) & 0x01;
    slave->sii.coe_details.enable_sdo_info =            (flags >> 1) & 0x01;
    slave->sii.coe_details.enable_pdo_assign =          (flags >> 2) & 0x01;
    slave->sii.coe_details.enable_pdo_configuration =   (flags >> 3) & 0x01;
    slave->sii.coe_details.enable_upload_at_startup =   (flags >> 4) & 0x01;
    slave->sii.coe_details.enable_sdo_complete_access = (flags >> 5) & 0x01;

    // read general flags
    flags = EC_READ_U8(data + 0x000B);
    slave->sii.general_flags.enable_safeop =  (flags >> 0) & 0x01;
    slave->sii.general_flags.enable_not_lrw = (flags >> 1) & 0x01;

    slave->sii.current_on_ebus = EC_READ_S16(data + 0x0C);
    slave->sii.has_general = 1;
    return 0;
}

/*****************************************************************************/

/** Fetches data from a SYNC MANAGER category.
 *
 * Appends the sync managers described in the category to the existing ones.
 *
 * \return 0 in case of success, else < 0
 */
int ec_slave_fetch_sii_syncs(
        ec_slave_t *slave, /**< EtherCAT slave. */
        const uint8_t *data, /**< Category data. */
        size_t data_size /**< Number of bytes. */
        )
{
    unsigned int i, count, total_count;
    ec_sync_t *sync;
    size_t memsize;
    ec_sync_t *syncs;
    uint8_t index;

    // one sync manager struct is 4 words long
    if (data_size % 8) {
        EC_ERR("Invalid SII sync manager category size %u in slave %u.\n",
                data_size, slave->ring_position);
        return -1;
    }

    count = data_size / 8;

    if (count) {
        total_count = count + slave->sii.sync_count;
        memsize = sizeof(ec_sync_t) * total_count;
        if (!(syncs = kmalloc(memsize, GFP_KERNEL))) {
            EC_ERR("Failed to allocate %u bytes for sync managers.\n",
                    memsize);
            return -1;
        }

        for (i = 0; i < slave->sii.sync_count; i++)
            ec_sync_init_copy(syncs + i, slave->sii.syncs + i);

        // initialize new sync managers
        for (i = 0; i < count; i++, data += 8) {
            index = i + slave->sii.sync_count;
            sync = &syncs[index];

            ec_sync_init(sync, slave, index);
            sync->physical_start_address = EC_READ_U16(data);
            sync->length = EC_READ_U16(data + 2);
            sync->control_register = EC_READ_U8(data + 4);
            sync->enable = EC_READ_U8(data + 6);
        }

        if (slave->sii.syncs)
            kfree(slave->sii.syncs);
        slave->sii.syncs = syncs;
        slave->sii.sync_count = total_count;
    }

    return 0;
}

/*****************************************************************************/

/**
   Fetches data from a [RT]XPdo category.
   \return 0 in case of success, else < 0
*/

int ec_slave_fetch_sii_pdos(
        ec_slave_t *slave, /**< EtherCAT slave */
        const uint8_t *data, /**< category data */
        size_t data_size, /**< number of bytes */
        ec_direction_t dir /**< Pdo direction. */
        )
{
    ec_pdo_t *pdo;
    ec_pdo_entry_t *entry;
    unsigned int entry_count, i;

    while (data_size >= 8) {
        if (!(pdo = kmalloc(sizeof(ec_pdo_t), GFP_KERNEL))) {
            EC_ERR("Failed to allocate Pdo memory.\n");
            return -1;
        }

        ec_pdo_init(pdo);
        pdo->dir = dir;
        pdo->index = EC_READ_U16(data);
        entry_count = EC_READ_U8(data + 2);
        pdo->sync_index = EC_READ_U8(data + 3);
        if (ec_pdo_set_name(pdo,
                ec_slave_sii_string(slave, EC_READ_U8(data + 5)))) {
            ec_pdo_clear(pdo);
            kfree(pdo);
            return -1;
        }
        list_add_tail(&pdo->list, &slave->sii.pdos);

        data_size -= 8;
        data += 8;

        for (i = 0; i < entry_count; i++) {
            if (!(entry = kmalloc(sizeof(ec_pdo_entry_t), GFP_KERNEL))) {
                EC_ERR("Failed to allocate Pdo entry memory.\n");
                return -1;
            }

            ec_pdo_entry_init(entry);
            entry->index = EC_READ_U16(data);
            entry->subindex = EC_READ_U8(data + 2);
            if (ec_pdo_entry_set_name(entry,
                    ec_slave_sii_string(slave, EC_READ_U8(data + 3)))) {
                ec_pdo_entry_clear(entry);
                kfree(entry);
                return -1;
            }
            entry->bit_length = EC_READ_U8(data + 5);
            list_add_tail(&entry->list, &pdo->entries);

            data_size -= 8;
            data += 8;
        }

        // if sync manager index is positive, the Pdo is mapped by default
        if (pdo->sync_index >= 0) {
            ec_sync_t *sync;

            if (pdo->sync_index >= slave->sii.sync_count) {
                EC_ERR("Invalid SM index %i for Pdo 0x%04X in slave %u.",
                        pdo->sync_index, pdo->index, slave->ring_position);
                return -1;
            }
            sync = &slave->sii.syncs[pdo->sync_index];

            if (ec_pdo_list_add_pdo_copy(&sync->pdos, pdo))
                return -1;

            sync->assign_source = EC_ASSIGN_SII;
        }
    }

    return 0;
}

/*****************************************************************************/

/**
   Searches the string list for an index.
   \return 0 in case of success, else < 0
*/

char *ec_slave_sii_string(
        ec_slave_t *slave, /**< EtherCAT slave */
        unsigned int index /**< string index */
        )
{
    if (!index--) 
        return NULL;

    if (index >= slave->sii.string_count) {
        if (slave->master->debug_level)
            EC_WARN("String %u not found in slave %u.\n",
                    index, slave->ring_position);
        return NULL;
    }

    return slave->sii.strings[index];
}

/*****************************************************************************/

/**
 * Writes SII contents to a slave.
 * \return Zero on success, otherwise error code.
 */

int ec_slave_write_sii(
        ec_slave_t *slave, /**< EtherCAT slave */
        uint16_t offset, /**< SII word offset. */
        unsigned int nwords, /**< Number of words. */
        const uint16_t *words /**< New SII data. */
        )
{
    ec_master_t *master = slave->master;
    ec_sii_write_request_t request;

    // init SII write request
    INIT_LIST_HEAD(&request.list);
    request.slave = slave;
    request.words = words;
    request.offset = offset;
    request.nwords = nwords;
    request.state = EC_REQUEST_QUEUED;

    // schedule SII write request.
    down(&master->sii_sem);
    list_add_tail(&request.list, &master->sii_requests);
    up(&master->sii_sem);

    // wait for processing through FSM
    if (wait_event_interruptible(master->sii_queue,
                request.state != EC_REQUEST_QUEUED)) {
        // interrupted by signal
        down(&master->sii_sem);
        if (request.state == EC_REQUEST_QUEUED) {
            list_del(&request.list);
            up(&master->sii_sem);
            return -EINTR;
        }
        // request already processing: interrupt not possible.
        up(&master->sii_sem);
    }

    // wait until master FSM has finished processing
    wait_event(master->sii_queue,
            request.state != EC_REQUEST_BUSY);

    if (request.state == EC_REQUEST_SUCCESS) {
        if (offset <= 4 && offset + nwords > 4) { // alias was written
            slave->sii.alias = EC_READ_U16(words + 4);
        }
        return 0;
    } else {
        return -EIO;
    }
}

/*****************************************************************************/

/** Get the sync manager for either Rx- or Tx-Pdos.
 *
 * \todo This seems not to be correct in every case...
 * \return pointer to sync manager, or NULL.
 */
ec_sync_t *ec_slave_get_pdo_sync(
        ec_slave_t *slave, /**< EtherCAT slave. */
        ec_direction_t dir /**< Input or output. */
        )
{
    unsigned int sync_index;

    if (dir != EC_DIR_INPUT && dir != EC_DIR_OUTPUT) {
        EC_ERR("Invalid direction!\n");
        return NULL;
    }

    if (slave->sii.sync_count != 1) {
        sync_index = (unsigned int) dir;
        if (slave->sii.mailbox_protocols) sync_index += 2;

        if (sync_index >= slave->sii.sync_count)
            return NULL;
    } else { // sync_count == 1
        // A single sync manager may be used for inputs OR outputs!
        if (ec_sync_direction(&slave->sii.syncs[0]) != dir)
            return NULL;
        sync_index = 0;
    }

    return &slave->sii.syncs[sync_index];
}

/*****************************************************************************/

/**
   Counts the total number of Sdos and entries in the dictionary.
*/

void ec_slave_sdo_dict_info(const ec_slave_t *slave, /**< EtherCAT slave */
                            unsigned int *sdo_count, /**< number of Sdos */
                            unsigned int *entry_count /**< total number of
                                                         entries */
                            )
{
    unsigned int sdos = 0, entries = 0;
    ec_sdo_t *sdo;
    ec_sdo_entry_t *entry;

    list_for_each_entry(sdo, &slave->sdo_dictionary, list) {
        sdos++;
        list_for_each_entry(entry, &sdo->entries, list) {
            entries++;
        }
    }

    *sdo_count = sdos;
    *entry_count = entries;
}

/*****************************************************************************/

/**
 * Get an Sdo from the dictionary.
 * \returns The desired Sdo, or NULL.
 */

ec_sdo_t *ec_slave_get_sdo(
        ec_slave_t *slave, /**< EtherCAT slave */
        uint16_t index /**< Sdo index */
        )
{
    ec_sdo_t *sdo;

    list_for_each_entry(sdo, &slave->sdo_dictionary, list) {
        if (sdo->index != index)
            continue;
        return sdo;
    }

    return NULL;
}

/*****************************************************************************/

/**
 * Get an Sdo from the dictionary.
 *
 * const version.
 *
 * \returns The desired Sdo, or NULL.
 */

const ec_sdo_t *ec_slave_get_sdo_const(
        const ec_slave_t *slave, /**< EtherCAT slave */
        uint16_t index /**< Sdo index */
        )
{
    const ec_sdo_t *sdo;

    list_for_each_entry(sdo, &slave->sdo_dictionary, list) {
        if (sdo->index != index)
            continue;
        return sdo;
    }

    return NULL;
}

/*****************************************************************************/

/** Get an Sdo from the dictionary, given its position in the list.
 * \returns The desired Sdo, or NULL.
 */

const ec_sdo_t *ec_slave_get_sdo_by_pos_const(
        const ec_slave_t *slave, /**< EtherCAT slave. */
        uint16_t sdo_position /**< Sdo list position. */
        )
{
    const ec_sdo_t *sdo;

    list_for_each_entry(sdo, &slave->sdo_dictionary, list) {
        if (sdo_position--)
            continue;
        return sdo;
    }

    return NULL;
}

/*****************************************************************************/

/** Get the number of Sdos in the dictionary.
 * \returns Sdo count.
 */

uint16_t ec_slave_sdo_count(
        const ec_slave_t *slave /**< EtherCAT slave. */
        )
{
    const ec_sdo_t *sdo;
    uint16_t count = 0;

    list_for_each_entry(sdo, &slave->sdo_dictionary, list) {
        count++;
    }

    return count;
}

/*****************************************************************************/

/** Finds a mapped Pdo.
 * \returns The desired Pdo object, or NULL.
 */
const ec_pdo_t *ec_slave_find_pdo(
        const ec_slave_t *slave, /**< Slave. */
        uint16_t index /**< Pdo index to find. */
        )
{
    unsigned int i;
    const ec_sync_t *sync;
    const ec_pdo_t *pdo;

    for (i = 0; i < slave->sii.sync_count; i++) {
        sync = &slave->sii.syncs[i];

        if (!(pdo = ec_pdo_list_find_pdo_const(&sync->pdos, index)))
            continue;

        return pdo;
    }

    return NULL;
}

/*****************************************************************************/
