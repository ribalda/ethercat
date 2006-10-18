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
#include "slave.h"
#include "datagram.h"
#include "master.h"

/*****************************************************************************/

extern const ec_code_msg_t al_status_messages[];

/*****************************************************************************/

void ec_slave_clear(struct kobject *);
ssize_t ec_show_slave_attribute(struct kobject *, struct attribute *, char *);
ssize_t ec_store_slave_attribute(struct kobject *, struct attribute *,
                                 const char *, size_t);

/*****************************************************************************/

/** \cond */

EC_SYSFS_READ_ATTR(info);
EC_SYSFS_READ_WRITE_ATTR(state);
EC_SYSFS_READ_WRITE_ATTR(eeprom);

static struct attribute *def_attrs[] = {
    &attr_info,
    &attr_state,
    &attr_eeprom,
    NULL,
};

static struct sysfs_ops sysfs_ops = {
    .show = ec_show_slave_attribute,
    .store = ec_store_slave_attribute
};

static struct kobj_type ktype_ec_slave = {
    .release = ec_slave_clear,
    .sysfs_ops = &sysfs_ops,
    .default_attrs = def_attrs
};

/** \endcond */

/*****************************************************************************/

/**
   Slave constructor.
   \return 0 in case of success, else < 0
*/

int ec_slave_init(ec_slave_t *slave, /**< EtherCAT slave */
                  ec_master_t *master, /**< EtherCAT master */
                  uint16_t ring_position, /**< ring position */
                  uint16_t station_address /**< station address to configure */
                  )
{
    unsigned int i;

    slave->ring_position = ring_position;
    slave->station_address = station_address;

    // init kobject and add it to the hierarchy
    memset(&slave->kobj, 0x00, sizeof(struct kobject));
    kobject_init(&slave->kobj);
    slave->kobj.ktype = &ktype_ec_slave;
    slave->kobj.parent = &master->kobj;
    if (kobject_set_name(&slave->kobj, "slave%03i", slave->ring_position)) {
        EC_ERR("Failed to set kobject name.\n");
        kobject_put(&slave->kobj);
        return -1;
    }

    slave->master = master;

    slave->requested_state = EC_SLAVE_STATE_UNKNOWN;
    slave->current_state = EC_SLAVE_STATE_UNKNOWN;
    slave->error_flag = 0;
    slave->online = 1;
    slave->fmmu_count = 0;
    slave->registered = 0;

    slave->coupler_index = 0;
    slave->coupler_subindex = 0xFFFF;

    slave->base_type = 0;
    slave->base_revision = 0;
    slave->base_build = 0;
    slave->base_fmmu_count = 0;
    slave->base_sync_count = 0;

    slave->eeprom_data = NULL;
    slave->eeprom_size = 0;
    slave->new_eeprom_data = NULL;
    slave->new_eeprom_size = 0;

    slave->sii_alias = 0;
    slave->sii_vendor_id = 0;
    slave->sii_product_code = 0;
    slave->sii_revision_number = 0;
    slave->sii_serial_number = 0;
    slave->sii_rx_mailbox_offset = 0;
    slave->sii_rx_mailbox_size = 0;
    slave->sii_tx_mailbox_offset = 0;
    slave->sii_tx_mailbox_size = 0;
    slave->sii_mailbox_protocols = 0;
    slave->sii_group = NULL;
    slave->sii_image = NULL;
    slave->sii_order = NULL;
    slave->sii_name = NULL;

    INIT_LIST_HEAD(&slave->sii_strings);
    INIT_LIST_HEAD(&slave->sii_syncs);
    INIT_LIST_HEAD(&slave->sii_pdos);
    INIT_LIST_HEAD(&slave->sdo_dictionary);
    INIT_LIST_HEAD(&slave->sdo_confs);

    for (i = 0; i < 4; i++) {
        slave->dl_link[i] = 0;
        slave->dl_loop[i] = 0;
        slave->dl_signal[i] = 0;
        slave->sii_physical_layer[i] = 0xFF;
    }

    return 0;
}

/*****************************************************************************/

/**
   Slave destructor.
*/

void ec_slave_clear(struct kobject *kobj /**< kobject of the slave */)
{
    ec_slave_t *slave;
    ec_sii_string_t *string, *next_str;
    ec_sii_sync_t *sync, *next_sync;
    ec_sii_pdo_t *pdo, *next_pdo;
    ec_sii_pdo_entry_t *entry, *next_ent;
    ec_sdo_t *sdo, *next_sdo;
    ec_sdo_data_t *sdodata, *next_sdodata;

    slave = container_of(kobj, ec_slave_t, kobj);

    // free all string objects
    list_for_each_entry_safe(string, next_str, &slave->sii_strings, list) {
        list_del(&string->list);
        kfree(string);
    }

    // free all sync managers
    list_for_each_entry_safe(sync, next_sync, &slave->sii_syncs, list) {
        list_del(&sync->list);
        kfree(sync);
    }

    // free all PDOs
    list_for_each_entry_safe(pdo, next_pdo, &slave->sii_pdos, list) {
        list_del(&pdo->list);
        if (pdo->name) kfree(pdo->name);

        // free all PDO entries
        list_for_each_entry_safe(entry, next_ent, &pdo->entries, list) {
            list_del(&entry->list);
            if (entry->name) kfree(entry->name);
            kfree(entry);
        }

        kfree(pdo);
    }

    if (slave->sii_group) kfree(slave->sii_group);
    if (slave->sii_image) kfree(slave->sii_image);
    if (slave->sii_order) kfree(slave->sii_order);
    if (slave->sii_name) kfree(slave->sii_name);

    // free all SDOs
    list_for_each_entry_safe(sdo, next_sdo, &slave->sdo_dictionary, list) {
        list_del(&sdo->list);
        kobject_del(&sdo->kobj);
        kobject_put(&sdo->kobj);
    }

    // free all SDO configurations
    list_for_each_entry_safe(sdodata, next_sdodata, &slave->sdo_confs, list) {
        list_del(&sdodata->list);
        kfree(sdodata->data);
        kfree(sdodata);
    }

    if (slave->eeprom_data) kfree(slave->eeprom_data);
    if (slave->new_eeprom_data) kfree(slave->new_eeprom_data);
}

/*****************************************************************************/

/**
   Fetches data from a STRING category.
   \return 0 in case of success, else < 0
*/

int ec_slave_fetch_strings(ec_slave_t *slave, /**< EtherCAT slave */
                           const uint8_t *data /**< category data */
                           )
{
    unsigned int string_count, i;
    size_t size;
    off_t offset;
    ec_sii_string_t *string;

    string_count = data[0];
    offset = 1;
    for (i = 0; i < string_count; i++) {
        size = data[offset];
        // allocate memory for string structure and data at a single blow
        if (!(string = (ec_sii_string_t *)
              kmalloc(sizeof(ec_sii_string_t) + size + 1, GFP_ATOMIC))) {
            EC_ERR("Failed to allocate string memory.\n");
            return -1;
        }
        string->size = size;
        // string memory appended to string structure
        string->data = (char *) string + sizeof(ec_sii_string_t);
        memcpy(string->data, data + offset + 1, size);
        string->data[size] = 0x00;
        list_add_tail(&string->list, &slave->sii_strings);
        offset += 1 + size;
    }

    return 0;
}

/*****************************************************************************/

/**
   Fetches data from a GENERAL category.
   \return 0 in case of success, else < 0
*/

void ec_slave_fetch_general(ec_slave_t *slave, /**< EtherCAT slave */
                            const uint8_t *data /**< category data */
                            )
{
    unsigned int i;

    ec_slave_locate_string(slave, data[0], &slave->sii_group);
    ec_slave_locate_string(slave, data[1], &slave->sii_image);
    ec_slave_locate_string(slave, data[2], &slave->sii_order);
    ec_slave_locate_string(slave, data[3], &slave->sii_name);

    for (i = 0; i < 4; i++)
        slave->sii_physical_layer[i] =
            (data[4] & (0x03 << (i * 2))) >> (i * 2);
}

/*****************************************************************************/

/**
   Fetches data from a SYNC MANAGER category.
   \return 0 in case of success, else < 0
*/

int ec_slave_fetch_sync(ec_slave_t *slave, /**< EtherCAT slave */
                        const uint8_t *data, /**< category data */
                        size_t word_count /**< number of words */
                        )
{
    unsigned int sync_count, i;
    ec_sii_sync_t *sync;

    sync_count = word_count / 4; // sync manager struct is 4 words long

    for (i = 0; i < sync_count; i++, data += 8) {
        if (!(sync = (ec_sii_sync_t *)
              kmalloc(sizeof(ec_sii_sync_t), GFP_ATOMIC))) {
            EC_ERR("Failed to allocate Sync-Manager memory.\n");
            return -1;
        }

        sync->index = i;
        sync->physical_start_address = EC_READ_U16(data);
        sync->length                 = EC_READ_U16(data + 2);
        sync->control_register       = EC_READ_U8 (data + 4);
        sync->enable                 = EC_READ_U8 (data + 6);

        list_add_tail(&sync->list, &slave->sii_syncs);
    }

    return 0;
}

/*****************************************************************************/

/**
   Fetches data from a [RT]XPDO category.
   \return 0 in case of success, else < 0
*/

int ec_slave_fetch_pdo(ec_slave_t *slave, /**< EtherCAT slave */
                       const uint8_t *data, /**< category data */
                       size_t word_count, /**< number of words */
                       ec_sii_pdo_type_t pdo_type /**< PDO type */
                       )
{
    ec_sii_pdo_t *pdo;
    ec_sii_pdo_entry_t *entry;
    unsigned int entry_count, i;

    while (word_count >= 4) {
        if (!(pdo = (ec_sii_pdo_t *)
              kmalloc(sizeof(ec_sii_pdo_t), GFP_ATOMIC))) {
            EC_ERR("Failed to allocate PDO memory.\n");
            return -1;
        }

        INIT_LIST_HEAD(&pdo->entries);
        pdo->type = pdo_type;

        pdo->index = EC_READ_U16(data);
        entry_count = EC_READ_U8(data + 2);
        pdo->sync_index = EC_READ_U8(data + 3);
        pdo->name = NULL;
        ec_slave_locate_string(slave, EC_READ_U8(data + 5), &pdo->name);

        list_add_tail(&pdo->list, &slave->sii_pdos);

        word_count -= 4;
        data += 8;

        for (i = 0; i < entry_count; i++) {
            if (!(entry = (ec_sii_pdo_entry_t *)
                  kmalloc(sizeof(ec_sii_pdo_entry_t), GFP_ATOMIC))) {
                EC_ERR("Failed to allocate PDO entry memory.\n");
                return -1;
            }

            entry->index = EC_READ_U16(data);
            entry->subindex = EC_READ_U8(data + 2);
            entry->name = NULL;
            ec_slave_locate_string(slave, EC_READ_U8(data + 3), &entry->name);
            entry->bit_length = EC_READ_U8(data + 5);

            list_add_tail(&entry->list, &pdo->entries);

            word_count -= 4;
            data += 8;
        }
    }

    return 0;
}

/*****************************************************************************/

/**
   Searches the string list for an index and allocates a new string.
   \return 0 in case of success, else < 0
   \todo documentation
*/

int ec_slave_locate_string(ec_slave_t *slave, /**< EtherCAT slave */
                           unsigned int index, /**< string index */
                           char **ptr /**< Address of the string pointer */
                           )
{
    ec_sii_string_t *string;
    char *err_string;

    // Erst alten Speicher freigeben
    if (*ptr) {
        kfree(*ptr);
        *ptr = NULL;
    }

    // Index 0 bedeutet "nicht belegt"
    if (!index) return 0;

    // EEPROM-String mit Index finden und kopieren
    list_for_each_entry(string, &slave->sii_strings, list) {
        if (--index) continue;

        if (!(*ptr = (char *) kmalloc(string->size + 1, GFP_ATOMIC))) {
            EC_ERR("Unable to allocate string memory.\n");
            return -1;
        }
        memcpy(*ptr, string->data, string->size + 1);
        return 0;
    }

    EC_WARN("String %i not found in slave %i.\n", index, slave->ring_position);

    err_string = "(string not found)";

    if (!(*ptr = (char *) kmalloc(strlen(err_string) + 1, GFP_ATOMIC))) {
        EC_WARN("Unable to allocate string memory.\n");
        return -1;
    }

    memcpy(*ptr, err_string, strlen(err_string) + 1);
    return 0;
}

/*****************************************************************************/

/**
   Prepares an FMMU configuration.
   Configuration data for the FMMU is saved in the slave structure and is
   written to the slave in ecrt_master_activate().
   The FMMU configuration is done in a way, that the complete data range
   of the corresponding sync manager is covered. Seperate FMMUs are configured
   for each domain.
   If the FMMU configuration is already prepared, the function returns with
   success.
   \return 0 in case of success, else < 0
*/

int ec_slave_prepare_fmmu(ec_slave_t *slave, /**< EtherCAT slave */
                          const ec_domain_t *domain, /**< domain */
                          const ec_sii_sync_t *sync  /**< sync manager */
                          )
{
    unsigned int i;
    ec_fmmu_t *fmmu;

    // FMMU configuration already prepared?
    for (i = 0; i < slave->fmmu_count; i++) {
        fmmu = &slave->fmmus[i];
        if (fmmu->domain == domain && fmmu->sync == sync)
            return 0;
    }

    // reserve new FMMU...

    if (slave->fmmu_count >= slave->base_fmmu_count) {
        EC_ERR("Slave %i FMMU limit reached!\n", slave->ring_position);
        return -1;
    }

    fmmu = &slave->fmmus[slave->fmmu_count];

    fmmu->index = slave->fmmu_count;
    fmmu->domain = domain;
    fmmu->sync = sync;
    fmmu->logical_start_address = 0;

    slave->fmmu_count++;
    slave->registered = 1;

    return 0;
}

/*****************************************************************************/

/**
   Outputs all information about a certain slave.
*/

size_t ec_slave_info(const ec_slave_t *slave, /**< EtherCAT slave */
                     char *buffer /**< Output buffer */
                     )
{
    off_t off = 0;
    ec_sii_sync_t *sync;
    ec_sii_pdo_t *pdo;
    ec_sii_pdo_entry_t *pdo_entry;
    int first, i;

    off += sprintf(buffer + off, "\nName: ");

    if (slave->sii_name)
        off += sprintf(buffer + off, "%s", slave->sii_name);

    off += sprintf(buffer + off, "\nVendor ID: 0x%08X\n",
                   slave->sii_vendor_id);
    off += sprintf(buffer + off, "Product code: 0x%08X\n\n",
                   slave->sii_product_code);

    off += sprintf(buffer + off, "State: ");
    off += ec_state_string(slave->current_state, buffer + off);
    off += sprintf(buffer + off, "\nRing position: %i\n",
                   slave->ring_position);
    off += sprintf(buffer + off, "Advanced position: %i:%i\n",
                   slave->coupler_index, slave->coupler_subindex);
    off += sprintf(buffer + off, "Coupler: %s\n\n",
                   ec_slave_is_coupler(slave) ? "yes" : "no");

    off += sprintf(buffer + off, "Data link status:\n");
    for (i = 0; i < 4; i++) {
        off += sprintf(buffer + off, "  Port %i (", i);
        switch (slave->sii_physical_layer[i]) {
            case 0x00:
                off += sprintf(buffer + off, "EBUS");
                break;
            case 0x01:
                off += sprintf(buffer + off, "100BASE-TX");
                break;
            case 0x02:
                off += sprintf(buffer + off, "100BASE-FX");
                break;
            default:
                off += sprintf(buffer + off, "unknown (%i)",
                               slave->sii_physical_layer[i]);
        }
        off += sprintf(buffer + off, ") Link %s, Loop %s, %s\n",
                       slave->dl_link[i] ? "up" : "down",
                       slave->dl_loop[i] ? "closed" : "open",
                       slave->dl_signal[i] ? "Signal detected" : "No signal");
    }

    if (slave->sii_mailbox_protocols) {
        off += sprintf(buffer + off, "\nMailboxes:\n");
        off += sprintf(buffer + off, "  RX mailbox: 0x%04X/%i,"
                       " TX mailbox: 0x%04X/%i\n",
                       slave->sii_rx_mailbox_offset,
                       slave->sii_rx_mailbox_size,
                       slave->sii_tx_mailbox_offset,
                       slave->sii_tx_mailbox_size);
        off += sprintf(buffer + off, "  Supported protocols: ");

        first = 1;
        if (slave->sii_mailbox_protocols & EC_MBOX_AOE) {
            off += sprintf(buffer + off, "AoE");
            first = 0;
        }
        if (slave->sii_mailbox_protocols & EC_MBOX_EOE) {
            if (!first) off += sprintf(buffer + off, ", ");
            off += sprintf(buffer + off, "EoE");
            first = 0;
        }
        if (slave->sii_mailbox_protocols & EC_MBOX_COE) {
            if (!first) off += sprintf(buffer + off, ", ");
            off += sprintf(buffer + off, "CoE");
            first = 0;
        }
        if (slave->sii_mailbox_protocols & EC_MBOX_FOE) {
            if (!first) off += sprintf(buffer + off, ", ");
            off += sprintf(buffer + off, "FoE");
            first = 0;
        }
        if (slave->sii_mailbox_protocols & EC_MBOX_SOE) {
            if (!first) off += sprintf(buffer + off, ", ");
            off += sprintf(buffer + off, "SoE");
            first = 0;
        }
        if (slave->sii_mailbox_protocols & EC_MBOX_VOE) {
            if (!first) off += sprintf(buffer + off, ", ");
            off += sprintf(buffer + off, "VoE");
        }
        off += sprintf(buffer + off, "\n");
    }

    if (slave->sii_alias || slave->sii_group
        || slave->sii_image || slave->sii_order)
        off += sprintf(buffer + off, "\nSII data:\n");

    if (slave->sii_alias)
        off += sprintf(buffer + off, "  Configured station alias:"
                       " 0x%04X (%i)\n", slave->sii_alias, slave->sii_alias);
    if (slave->sii_group)
        off += sprintf(buffer + off, "  Group: %s\n", slave->sii_group);
    if (slave->sii_image)
        off += sprintf(buffer + off, "  Image: %s\n", slave->sii_image);
    if (slave->sii_order)
        off += sprintf(buffer + off, "  Order number: %s\n", slave->sii_order);

    if (!list_empty(&slave->sii_syncs))
        off += sprintf(buffer + off, "\nSync-Managers:\n");

    list_for_each_entry(sync, &slave->sii_syncs, list) {
        off += sprintf(buffer + off, "  %i: 0x%04X, length %i,"
                       " control 0x%02X, %s\n",
                       sync->index, sync->physical_start_address,
                       sync->length, sync->control_register,
                       sync->enable ? "enable" : "disable");
    }

    if (!list_empty(&slave->sii_pdos))
        off += sprintf(buffer + off, "\nPDOs:\n");

    list_for_each_entry(pdo, &slave->sii_pdos, list) {
        off += sprintf(buffer + off,
                       "  %s \"%s\" (0x%04X), Sync-Manager %i\n",
                       pdo->type == EC_RX_PDO ? "RXPDO" : "TXPDO",
                       pdo->name ? pdo->name : "???",
                       pdo->index, pdo->sync_index);

        list_for_each_entry(pdo_entry, &pdo->entries, list) {
            off += sprintf(buffer + off, "    \"%s\" 0x%04X:%X, %i bit\n",
                           pdo_entry->name ? pdo_entry->name : "???",
                           pdo_entry->index, pdo_entry->subindex,
                           pdo_entry->bit_length);
        }
    }

    off += sprintf(buffer + off, "\n");

    return off;
}

/*****************************************************************************/

/**
   Schedules an EEPROM write operation.
   \return 0 in case of success, else < 0
*/

ssize_t ec_slave_write_eeprom(ec_slave_t *slave, /**< EtherCAT slave */
                              const uint8_t *data, /**< new EEPROM data */
                              size_t size /**< size of data in bytes */
                              )
{
    uint16_t word_size, cat_type, cat_size;
    const uint16_t *data_words, *next_header;
    uint16_t *new_data;

    if (!slave->master->eeprom_write_enable) {
        EC_ERR("Writing EEPROMs not allowed! Enable via"
               " eeprom_write_enable SysFS entry.\n");
        return -1;
    }

    if (slave->master->mode != EC_MASTER_MODE_IDLE) {
        EC_ERR("Writing EEPROMs only allowed in idle mode!\n");
        return -1;
    }

    if (slave->new_eeprom_data) {
        EC_ERR("Slave %i already has a pending EEPROM write operation!\n",
               slave->ring_position);
        return -1;
    }

    // coarse check of the data

    if (size % 2) {
        EC_ERR("EEPROM size is odd! Dropping.\n");
        return -1;
    }

    data_words = (const uint16_t *) data;
    word_size = size / 2;

    if (word_size < 0x0041) {
        EC_ERR("EEPROM data too short! Dropping.\n");
        return -1;
    }

    next_header = data_words + 0x0040;
    cat_type = EC_READ_U16(next_header);
    while (cat_type != 0xFFFF) {
        cat_type = EC_READ_U16(next_header);
        cat_size = EC_READ_U16(next_header + 1);
        if ((next_header + cat_size + 2) - data_words >= word_size) {
            EC_ERR("EEPROM data seems to be corrupted! Dropping.\n");
            return -1;
        }
        next_header += cat_size + 2;
        cat_type = EC_READ_U16(next_header);
    }

    // data ok!

    if (!(new_data = (uint16_t *) kmalloc(word_size * 2, GFP_KERNEL))) {
        EC_ERR("Unable to allocate memory for new EEPROM data!\n");
        return -1;
    }
    memcpy(new_data, data, size);

    slave->new_eeprom_size = word_size;
    slave->new_eeprom_data = new_data;

    EC_INFO("EEPROM writing scheduled for slave %i, %i words.\n",
            slave->ring_position, word_size);
    return 0;
}

/*****************************************************************************/

/**
   Formats attribute data for SysFS read access.
   \return number of bytes to read
*/

ssize_t ec_show_slave_attribute(struct kobject *kobj, /**< slave's kobject */
                                struct attribute *attr, /**< attribute */
                                char *buffer /**< memory to store data */
                                )
{
    ec_slave_t *slave = container_of(kobj, ec_slave_t, kobj);

    if (attr == &attr_info) {
        return ec_slave_info(slave, buffer);
    }
    else if (attr == &attr_state) {
        switch (slave->current_state) {
            case EC_SLAVE_STATE_INIT:
                return sprintf(buffer, "INIT\n");
            case EC_SLAVE_STATE_PREOP:
                return sprintf(buffer, "PREOP\n");
            case EC_SLAVE_STATE_SAVEOP:
                return sprintf(buffer, "SAVEOP\n");
            case EC_SLAVE_STATE_OP:
                return sprintf(buffer, "OP\n");
            default:
                return sprintf(buffer, "UNKNOWN\n");
        }
    }
    else if (attr == &attr_eeprom) {
        if (slave->eeprom_data) {
            if (slave->eeprom_size > PAGE_SIZE) {
                EC_ERR("EEPROM contents of slave %i exceed 1 page (%i/%i).\n",
                       slave->ring_position, slave->eeprom_size,
                       (int) PAGE_SIZE);
            }
            else {
                memcpy(buffer, slave->eeprom_data, slave->eeprom_size);
                return slave->eeprom_size;
            }
        }
    }

    return 0;
}

/*****************************************************************************/

/**
   Formats attribute data for SysFS write access.
   \return number of bytes processed, or negative error code
*/

ssize_t ec_store_slave_attribute(struct kobject *kobj, /**< slave's kobject */
                                 struct attribute *attr, /**< attribute */
                                 const char *buffer, /**< memory with data */
                                 size_t size /**< size of data to store */
                                 )
{
    ec_slave_t *slave = container_of(kobj, ec_slave_t, kobj);

    if (attr == &attr_state) {
        char state[EC_STATE_STRING_SIZE];
        if (!strcmp(buffer, "INIT\n"))
            slave->requested_state = EC_SLAVE_STATE_INIT;
        else if (!strcmp(buffer, "PREOP\n"))
            slave->requested_state = EC_SLAVE_STATE_PREOP;
        else if (!strcmp(buffer, "SAVEOP\n"))
            slave->requested_state = EC_SLAVE_STATE_SAVEOP;
        else if (!strcmp(buffer, "OP\n"))
            slave->requested_state = EC_SLAVE_STATE_OP;
        else {
            EC_ERR("Invalid slave state \"%s\"!\n", buffer);
            return -EINVAL;
        }

        ec_state_string(slave->requested_state, state);
        EC_INFO("Accepted new state %s for slave %i.\n",
                state, slave->ring_position);
        slave->error_flag = 0;
        return size;
    }
    else if (attr == &attr_eeprom) {
        if (!ec_slave_write_eeprom(slave, buffer, size))
            return size;
    }

    return -EINVAL;
}

/*****************************************************************************/

/**
   Calculates the size of a sync manager by evaluating PDO sizes.
   \return sync manager size
*/

uint16_t ec_slave_calc_sync_size(const ec_slave_t *slave,
                                 /**< EtherCAT slave */
                                 const ec_sii_sync_t *sync
                                 /**< sync manager */
                                 )
{
    ec_sii_pdo_t *pdo;
    ec_sii_pdo_entry_t *pdo_entry;
    unsigned int bit_size;

    if (sync->length) return sync->length;

    bit_size = 0;
    list_for_each_entry(pdo, &slave->sii_pdos, list) {
        if (pdo->sync_index != sync->index) continue;

        list_for_each_entry(pdo_entry, &pdo->entries, list) {
            bit_size += pdo_entry->bit_length;
        }
    }

    if (bit_size % 8) // round up to full bytes
        return bit_size / 8 + 1;
    else
        return bit_size / 8;
}

/*****************************************************************************/

/**
   \return non-zero if slave is a bus coupler
*/

int ec_slave_is_coupler(const ec_slave_t *slave /**< EtherCAT slave */)
{
    // TODO: Better bus coupler criterion
    return slave->sii_vendor_id == 0x00000002
        && slave->sii_product_code == 0x044C2C52;
}

/*****************************************************************************/

/**
   \return non-zero if slave is a bus coupler
*/

int ec_slave_has_subbus(const ec_slave_t *slave /**< EtherCAT slave */)
{
    return slave->sii_vendor_id == 0x00000002
        && slave->sii_product_code == 0x04602c22;
}

/*****************************************************************************/

/**
   \return 0 in case of success, else < 0
*/

int ec_slave_conf_sdo(ec_slave_t *slave, /**< EtherCAT slave */
                      uint16_t sdo_index, /**< SDO index */
                      uint8_t sdo_subindex, /**< SDO subindex */
                      const uint8_t *data, /**< SDO data */
                      size_t size /**< SDO size in bytes */
                      )
{
    ec_sdo_data_t *sdodata;

    if (!(slave->sii_mailbox_protocols & EC_MBOX_COE)) {
        EC_ERR("Slave %i does not support CoE!\n", slave->ring_position);
        return -1;
    }

    if (!(sdodata = (ec_sdo_data_t *)
          kmalloc(sizeof(ec_sdo_data_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate memory for SDO configuration object!\n");
        return -1;
    }

    if (!(sdodata->data = (uint8_t *) kmalloc(size, GFP_KERNEL))) {
        EC_ERR("Failed to allocate memory for SDO configuration data!\n");
        kfree(sdodata);
        return -1;
    }

    sdodata->index = sdo_index;
    sdodata->subindex = sdo_subindex;
    memcpy(sdodata->data, data, size);
    sdodata->size = size;

    list_add_tail(&sdodata->list, &slave->sdo_confs);
    return 0;
}

/*****************************************************************************/

/**
   \return 0 in case of success, else < 0
*/

int ec_slave_validate(const ec_slave_t *slave, /**< EtherCAT slave */
                      uint32_t vendor_id, /**< vendor ID */
                      uint32_t product_code /**< product code */
                      )
{
    if (vendor_id != slave->sii_vendor_id ||
        product_code != slave->sii_product_code) {
        EC_ERR("Invalid slave type at position %i - Requested: 0x%08X 0x%08X,"
               " found: 0x%08X 0x%08X\".\n", slave->ring_position, vendor_id,
               product_code, slave->sii_vendor_id, slave->sii_product_code);
        return -1;
    }
    return 0;
}

/******************************************************************************
 *  Realtime interface
 *****************************************************************************/

/**
   \return 0 in case of success, else < 0
   \ingroup RealtimeInterface
*/

int ecrt_slave_conf_sdo8(ec_slave_t *slave, /**< EtherCAT slave */
                         uint16_t sdo_index, /**< SDO index */
                         uint8_t sdo_subindex, /**< SDO subindex */
                         uint8_t value /**< new SDO value */
                         )
{
    uint8_t data[1];
    EC_WRITE_U8(data, value);
    return ec_slave_conf_sdo(slave, sdo_index, sdo_subindex, data, 1);
}

/*****************************************************************************/

/**
   \return 0 in case of success, else < 0
   \ingroup RealtimeInterface
*/

int ecrt_slave_conf_sdo16(ec_slave_t *slave, /**< EtherCAT slave */
                          uint16_t sdo_index, /**< SDO index */
                          uint8_t sdo_subindex, /**< SDO subindex */
                          uint16_t value /**< new SDO value */
                          )
{
    uint8_t data[2];
    EC_WRITE_U16(data, value);
    return ec_slave_conf_sdo(slave, sdo_index, sdo_subindex, data, 2);
}

/*****************************************************************************/

/**
   \return 0 in case of success, else < 0
   \ingroup RealtimeInterface
*/

int ecrt_slave_conf_sdo32(ec_slave_t *slave, /**< EtherCAT slave */
                          uint16_t sdo_index, /**< SDO index */
                          uint8_t sdo_subindex, /**< SDO subindex */
                          uint32_t value /**< new SDO value */
                          )
{
    uint8_t data[4];
    EC_WRITE_U32(data, value);
    return ec_slave_conf_sdo(slave, sdo_index, sdo_subindex, data, 4);
}

/*****************************************************************************/

/**
   \return 0 in case of success, else < 0
   \ingroup RealtimeInterface
*/

int ecrt_slave_pdo_size(ec_slave_t *slave, /**< EtherCAT slave */
                        uint16_t pdo_index, /**< PDO index */
                        uint8_t pdo_subindex, /**< PDO subindex */
                        size_t size /**< new PDO size */
                        )
{
    EC_WARN("ecrt_slave_pdo_size() currently not available.\n");
    return -1;

#if 0
    unsigned int i, j, field_counter;
    const ec_sii_sync_t *sync;
    const ec_pdo_t *pdo;
    ec_varsize_t *var;

    if (!slave->type) {
        EC_ERR("Slave %i has no type information!\n", slave->ring_position);
        return -1;
    }

    field_counter = 0;
    for (i = 0; (sync = slave->type->sync_managers[i]); i++) {
        for (j = 0; (field = sync->fields[j]); j++) {
            if (!strcmp(field->name, field_name)) {
                if (field_counter++ == field_index) {
                    // is the size of this field variable?
                    if (field->size) {
                        EC_ERR("Field \"%s\"[%i] of slave %i has no variable"
                               " size!\n", field->name, field_index,
                               slave->ring_position);
                        return -1;
                    }
                    // does a size specification already exist?
                    list_for_each_entry(var, &slave->varsize_fields, list) {
                        if (var->field == field) {
                            EC_WARN("Resizing field \"%s\"[%i] of slave %i.\n",
                                    field->name, field_index,
                                    slave->ring_position);
                            var->size = size;
                            return 0;
                        }
                    }
                    // create a new size specification...
                    if (!(var = kmalloc(sizeof(ec_varsize_t), GFP_KERNEL))) {
                        EC_ERR("Failed to allocate memory for varsize_t!\n");
                        return -1;
                    }
                    var->field = field;
                    var->size = size;
                    list_add_tail(&var->list, &slave->varsize_fields);
                    return 0;
                }
            }
        }
    }

    EC_ERR("Slave %i (\"%s %s\") has no field \"%s\"[%i]!\n",
           slave->ring_position, slave->type->vendor_name,
           slave->type->product_name, field_name, field_index);
    return -1;
#endif
}

/*****************************************************************************/

/**< \cond */

EXPORT_SYMBOL(ecrt_slave_conf_sdo8);
EXPORT_SYMBOL(ecrt_slave_conf_sdo16);
EXPORT_SYMBOL(ecrt_slave_conf_sdo32);
EXPORT_SYMBOL(ecrt_slave_pdo_size);

/**< \endcond */

/*****************************************************************************/
