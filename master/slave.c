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

int ec_slave_fetch_categories(ec_slave_t *);
ssize_t ec_show_slave_attribute(struct kobject *, struct attribute *, char *);
ssize_t ec_store_slave_attribute(struct kobject *, struct attribute *,
                                 const char *, size_t);

/*****************************************************************************/

/** \cond */

EC_SYSFS_READ_ATTR(ring_position);
EC_SYSFS_READ_ATTR(coupler_address);
EC_SYSFS_READ_ATTR(vendor_name);
EC_SYSFS_READ_ATTR(product_name);
EC_SYSFS_READ_ATTR(product_desc);
EC_SYSFS_READ_ATTR(sii_name);
EC_SYSFS_READ_ATTR(type);
EC_SYSFS_READ_WRITE_ATTR(state);
EC_SYSFS_READ_WRITE_ATTR(eeprom);

static struct attribute *def_attrs[] = {
    &attr_ring_position,
    &attr_coupler_address,
    &attr_vendor_name,
    &attr_product_name,
    &attr_product_desc,
    &attr_sii_name,
    &attr_type,
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
    slave->coupler_index = 0;
    slave->coupler_subindex = 0xFFFF;
    slave->base_type = 0;
    slave->base_revision = 0;
    slave->base_build = 0;
    slave->base_fmmu_count = 0;
    slave->base_sync_count = 0;
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
    slave->type = NULL;
    slave->registered = 0;
    slave->fmmu_count = 0;
    slave->eeprom_data = NULL;
    slave->eeprom_size = 0;
    slave->eeprom_group = NULL;
    slave->eeprom_image = NULL;
    slave->eeprom_order = NULL;
    slave->eeprom_name = NULL;
    slave->requested_state = EC_SLAVE_STATE_UNKNOWN;
    slave->current_state = EC_SLAVE_STATE_UNKNOWN;
    slave->error_flag = 0;
    slave->online = 1;
    slave->new_eeprom_data = NULL;
    slave->new_eeprom_size = 0;

    INIT_LIST_HEAD(&slave->eeprom_strings);
    INIT_LIST_HEAD(&slave->eeprom_syncs);
    INIT_LIST_HEAD(&slave->eeprom_pdos);
    INIT_LIST_HEAD(&slave->sdo_dictionary);
    INIT_LIST_HEAD(&slave->varsize_fields);

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
    ec_eeprom_string_t *string, *next_str;
    ec_eeprom_sync_t *sync, *next_sync;
    ec_eeprom_pdo_t *pdo, *next_pdo;
    ec_eeprom_pdo_entry_t *entry, *next_ent;
    ec_sdo_t *sdo, *next_sdo;
    ec_sdo_entry_t *en, *next_en;
    ec_varsize_t *var, *next_var;

    slave = container_of(kobj, ec_slave_t, kobj);

    // free all string objects
    list_for_each_entry_safe(string, next_str, &slave->eeprom_strings, list) {
        list_del(&string->list);
        kfree(string);
    }

    // free all sync managers
    list_for_each_entry_safe(sync, next_sync, &slave->eeprom_syncs, list) {
        list_del(&sync->list);
        kfree(sync);
    }

    // free all PDOs
    list_for_each_entry_safe(pdo, next_pdo, &slave->eeprom_pdos, list) {
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

    if (slave->eeprom_group) kfree(slave->eeprom_group);
    if (slave->eeprom_image) kfree(slave->eeprom_image);
    if (slave->eeprom_order) kfree(slave->eeprom_order);
    if (slave->eeprom_name) kfree(slave->eeprom_name);

    // free all SDOs
    list_for_each_entry_safe(sdo, next_sdo, &slave->sdo_dictionary, list) {
        list_del(&sdo->list);
        if (sdo->name) kfree(sdo->name);

        // free all SDO entries
        list_for_each_entry_safe(en, next_en, &sdo->entries, list) {
            list_del(&en->list);
            kfree(en);
        }
        kfree(sdo);
    }

    // free information about variable sized data fields
    list_for_each_entry_safe(var, next_var, &slave->varsize_fields, list) {
        list_del(&var->list);
        kfree(var);
    }

    if (slave->eeprom_data) kfree(slave->eeprom_data);
    if (slave->new_eeprom_data) kfree(slave->new_eeprom_data);
}

/*****************************************************************************/

/**
   Reads all necessary information from a slave.
   \return 0 in case of success, else < 0
*/

int ec_slave_fetch(ec_slave_t *slave /**< EtherCAT slave */)
{
    ec_datagram_t *datagram;
    unsigned int i;
    uint16_t dl_status;

    datagram = &slave->master->simple_datagram;

    // read base data
    if (ec_datagram_nprd(datagram, slave->station_address, 0x0000, 6))
        return -1;
    if (unlikely(ec_master_simple_io(slave->master, datagram))) {
        EC_ERR("Reading base data from slave %i failed!\n",
               slave->ring_position);
        return -1;
    }

    slave->base_type =       EC_READ_U8 (datagram->data);
    slave->base_revision =   EC_READ_U8 (datagram->data + 1);
    slave->base_build =      EC_READ_U16(datagram->data + 2);
    slave->base_fmmu_count = EC_READ_U8 (datagram->data + 4);
    slave->base_sync_count = EC_READ_U8 (datagram->data + 5);

    if (slave->base_fmmu_count > EC_MAX_FMMUS)
        slave->base_fmmu_count = EC_MAX_FMMUS;

    // read data link status
    if (ec_datagram_nprd(datagram, slave->station_address, 0x0110, 2))
        return -1;
    if (unlikely(ec_master_simple_io(slave->master, datagram))) {
        EC_ERR("Reading DL status from slave %i failed!\n",
               slave->ring_position);
        return -1;
    }

    dl_status = EC_READ_U16(datagram->data);
    for (i = 0; i < 4; i++) {
        slave->dl_link[i] = dl_status & (1 << (4 + i)) ? 1 : 0;
        slave->dl_loop[i] = dl_status & (1 << (8 + i * 2)) ? 1 : 0;
        slave->dl_signal[i] = dl_status & (1 << (9 + i * 2)) ? 1 : 0;
    }

    // read EEPROM data
    if (ec_slave_sii_read16(slave, 0x0004, &slave->sii_alias))
        return -1;
    if (ec_slave_sii_read32(slave, 0x0008, &slave->sii_vendor_id))
        return -1;
    if (ec_slave_sii_read32(slave, 0x000A, &slave->sii_product_code))
        return -1;
    if (ec_slave_sii_read32(slave, 0x000C, &slave->sii_revision_number))
        return -1;
    if (ec_slave_sii_read32(slave, 0x000E, &slave->sii_serial_number))
        return -1;
    if (ec_slave_sii_read16(slave, 0x0018, &slave->sii_rx_mailbox_offset))
        return -1;
    if (ec_slave_sii_read16(slave, 0x0019, &slave->sii_rx_mailbox_size))
        return -1;
    if (ec_slave_sii_read16(slave, 0x001A, &slave->sii_tx_mailbox_offset))
        return -1;
    if (ec_slave_sii_read16(slave, 0x001B, &slave->sii_tx_mailbox_size))
        return -1;
    if (ec_slave_sii_read16(slave, 0x001C, &slave->sii_mailbox_protocols))
        return -1;

    if (unlikely(ec_slave_fetch_categories(slave))) {
        EC_ERR("Failed to fetch category data!\n");
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Reads 16 bit from the slave information interface (SII).
   \return 0 in case of success, else < 0
*/

int ec_slave_sii_read16(ec_slave_t *slave,
                        /**< EtherCAT slave */
                        uint16_t offset,
                        /**< address of the SII register to read */
                        uint16_t *target
                        /**< target memory */
                        )
{
    ec_datagram_t *datagram;
    cycles_t start, end, timeout;

    datagram = &slave->master->simple_datagram;

    // initiate read operation
    if (ec_datagram_npwr(datagram, slave->station_address, 0x502, 6))
        return -1;
    EC_WRITE_U8 (datagram->data,     0x00); // read-only access
    EC_WRITE_U8 (datagram->data + 1, 0x01); // request read operation
    EC_WRITE_U32(datagram->data + 2, offset);
    if (unlikely(ec_master_simple_io(slave->master, datagram))) {
        EC_ERR("SII-read failed on slave %i!\n", slave->ring_position);
        return -1;
    }

    start = get_cycles();
    timeout = (cycles_t) 100 * cpu_khz; // 100ms

    while (1)
    {
        udelay(10);

        if (ec_datagram_nprd(datagram, slave->station_address, 0x502, 10))
            return -1;
        if (unlikely(ec_master_simple_io(slave->master, datagram))) {
            EC_ERR("Getting SII-read status failed on slave %i!\n",
                   slave->ring_position);
            return -1;
        }

        end = get_cycles();

        // check for "busy bit"
        if (likely((EC_READ_U8(datagram->data + 1) & 0x81) == 0)) {
            *target = EC_READ_U16(datagram->data + 6);
            return 0;
        }

        if (unlikely((end - start) >= timeout)) {
            EC_ERR("SII-read. Slave %i timed out!\n", slave->ring_position);
            return -1;
        }
    }
}

/*****************************************************************************/

/**
   Reads 32 bit from the slave information interface (SII).
   \return 0 in case of success, else < 0
*/

int ec_slave_sii_read32(ec_slave_t *slave,
                        /**< EtherCAT slave */
                        uint16_t offset,
                        /**< address of the SII register to read */
                        uint32_t *target
                        /**< target memory */
                        )
{
    ec_datagram_t *datagram;
    cycles_t start, end, timeout;

    datagram = &slave->master->simple_datagram;

    // initiate read operation
    if (ec_datagram_npwr(datagram, slave->station_address, 0x502, 6))
        return -1;
    EC_WRITE_U8 (datagram->data,     0x00); // read-only access
    EC_WRITE_U8 (datagram->data + 1, 0x01); // request read operation
    EC_WRITE_U32(datagram->data + 2, offset);
    if (unlikely(ec_master_simple_io(slave->master, datagram))) {
        EC_ERR("SII-read failed on slave %i!\n", slave->ring_position);
        return -1;
    }

    start = get_cycles();
    timeout = (cycles_t) 100 * cpu_khz; // 100ms

    while (1)
    {
        udelay(10);

        if (ec_datagram_nprd(datagram, slave->station_address, 0x502, 10))
            return -1;
        if (unlikely(ec_master_simple_io(slave->master, datagram))) {
            EC_ERR("Getting SII-read status failed on slave %i!\n",
                   slave->ring_position);
            return -1;
        }

        end = get_cycles();

        // check "busy bit"
        if (likely((EC_READ_U8(datagram->data + 1) & 0x81) == 0)) {
            *target = EC_READ_U32(datagram->data + 6);
            return 0;
        }

        if (unlikely((end - start) >= timeout)) {
            EC_ERR("SII-read. Slave %i timed out!\n", slave->ring_position);
            return -1;
        }
    }
}

/*****************************************************************************/

/**
   Writes 16 bit of data to the slave information interface (SII).
   \return 0 in case of success, else < 0
*/

int ec_slave_sii_write16(ec_slave_t *slave,
                         /**< EtherCAT slave */
                         uint16_t offset,
                         /**< address of the SII register to write */
                         uint16_t value
                         /**< new value */
                         )
{
    ec_datagram_t *datagram;
    cycles_t start, end, timeout;

    datagram = &slave->master->simple_datagram;

    EC_INFO("SII-write (slave %i, offset 0x%04X, value 0x%04X)\n",
            slave->ring_position, offset, value);

    // initiate write operation
    if (ec_datagram_npwr(datagram, slave->station_address, 0x502, 8))
        return -1;
    EC_WRITE_U8 (datagram->data,     0x01); // enable write access
    EC_WRITE_U8 (datagram->data + 1, 0x02); // request write operation
    EC_WRITE_U32(datagram->data + 2, offset);
    EC_WRITE_U16(datagram->data + 6, value);
    if (unlikely(ec_master_simple_io(slave->master, datagram))) {
        EC_ERR("SII-write failed on slave %i!\n", slave->ring_position);
        return -1;
    }

    start = get_cycles();
    timeout = (cycles_t) 100 * cpu_khz; // 100ms

    while (1)
    {
        udelay(10);

        if (ec_datagram_nprd(datagram, slave->station_address, 0x502, 2))
            return -1;
        if (unlikely(ec_master_simple_io(slave->master, datagram))) {
            EC_ERR("Getting SII-write status failed on slave %i!\n",
                   slave->ring_position);
            return -1;
        }

        end = get_cycles();

        // check "busy bit"
        if (likely((EC_READ_U8(datagram->data + 1) & 0x82) == 0)) {
            if (EC_READ_U8(datagram->data + 1) & 0x40) {
                EC_ERR("SII-write failed!\n");
                return -1;
            }
            else {
                EC_INFO("SII-write succeeded!\n");
                return 0;
            }
        }

        if (unlikely((end - start) >= timeout)) {
            EC_ERR("SII-write: Slave %i timed out!\n", slave->ring_position);
            return -1;
        }
    }
}

/*****************************************************************************/

/**
   Fetches data from slave's EEPROM.
   \return 0 in case of success, else < 0
   \todo memory allocation
*/

int ec_slave_fetch_categories(ec_slave_t *slave /**< EtherCAT slave */)
{
    uint16_t word_offset, cat_type, word_count;
    uint32_t value;
    uint8_t *cat_data;
    unsigned int i;

    word_offset = 0x0040;

    if (!(cat_data = (uint8_t *) kmalloc(0x10000, GFP_KERNEL))) {
        EC_ERR("Failed to allocate 64k bytes for category data.\n");
        return -1;
    }

    while (1) {
        // read category type
        if (ec_slave_sii_read32(slave, word_offset, &value)) {
            EC_ERR("Unable to read category header.\n");
            goto out_free;
        }

        // last category?
        if ((value & 0xFFFF) == 0xFFFF) break;

        cat_type = value & 0x7FFF;
        word_count = (value >> 16) & 0xFFFF;

        // fetch category data
        for (i = 0; i < word_count; i++) {
            if (ec_slave_sii_read32(slave, word_offset + 2 + i, &value)) {
                EC_ERR("Unable to read category data word %i.\n", i);
                goto out_free;
            }

            cat_data[i * 2]     = (value >> 0) & 0xFF;
            cat_data[i * 2 + 1] = (value >> 8) & 0xFF;

            // read second word "on the fly"
            if (i + 1 < word_count) {
                i++;
                cat_data[i * 2]     = (value >> 16) & 0xFF;
                cat_data[i * 2 + 1] = (value >> 24) & 0xFF;
            }
        }

        switch (cat_type)
        {
            case 0x000A:
                if (ec_slave_fetch_strings(slave, cat_data))
                    goto out_free;
                break;
            case 0x001E:
                if (ec_slave_fetch_general(slave, cat_data))
                    goto out_free;
                break;
            case 0x0028:
                break;
            case 0x0029:
                if (ec_slave_fetch_sync(slave, cat_data, word_count))
                    goto out_free;
                break;
            case 0x0032:
                if (ec_slave_fetch_pdo(slave, cat_data, word_count, EC_TX_PDO))
                    goto out_free;
                break;
            case 0x0033:
                if (ec_slave_fetch_pdo(slave, cat_data, word_count, EC_RX_PDO))
                    goto out_free;
                break;
            default:
                EC_WARN("Unknown category type 0x%04X in slave %i.\n",
                        cat_type, slave->ring_position);
        }

        word_offset += 2 + word_count;
    }

    kfree(cat_data);
    return 0;

 out_free:
    kfree(cat_data);
    return -1;
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
    ec_eeprom_string_t *string;

    string_count = data[0];
    offset = 1;
    for (i = 0; i < string_count; i++) {
        size = data[offset];
        // allocate memory for string structure and data at a single blow
        if (!(string = (ec_eeprom_string_t *)
              kmalloc(sizeof(ec_eeprom_string_t) + size + 1, GFP_KERNEL))) {
            EC_ERR("Failed to allocate string memory.\n");
            return -1;
        }
        string->size = size;
        // string memory appended to string structure
        string->data = (char *) string + sizeof(ec_eeprom_string_t);
        memcpy(string->data, data + offset + 1, size);
        string->data[size] = 0x00;
        list_add_tail(&string->list, &slave->eeprom_strings);
        offset += 1 + size;
    }

    return 0;
}

/*****************************************************************************/

/**
   Fetches data from a GENERAL category.
   \return 0 in case of success, else < 0
*/

int ec_slave_fetch_general(ec_slave_t *slave, /**< EtherCAT slave */
                           const uint8_t *data /**< category data */
                           )
{
    unsigned int i;

    if (ec_slave_locate_string(slave, data[0], &slave->eeprom_group))
        return -1;
    if (ec_slave_locate_string(slave, data[1], &slave->eeprom_image))
        return -1;
    if (ec_slave_locate_string(slave, data[2], &slave->eeprom_order))
        return -1;
    if (ec_slave_locate_string(slave, data[3], &slave->eeprom_name))
        return -1;

    for (i = 0; i < 4; i++)
        slave->sii_physical_layer[i] =
            (data[4] & (0x03 << (i * 2))) >> (i * 2);

    return 0;
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
    ec_eeprom_sync_t *sync;

    sync_count = word_count / 4; // sync manager struct is 4 words long

    for (i = 0; i < sync_count; i++, data += 8) {
        if (!(sync = (ec_eeprom_sync_t *)
              kmalloc(sizeof(ec_eeprom_sync_t), GFP_KERNEL))) {
            EC_ERR("Failed to allocate Sync-Manager memory.\n");
            return -1;
        }

        sync->index = i;
        sync->physical_start_address = *((uint16_t *) (data + 0));
        sync->length                 = *((uint16_t *) (data + 2));
        sync->control_register       = data[4];
        sync->enable                 = data[6];

        list_add_tail(&sync->list, &slave->eeprom_syncs);
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
                       ec_pdo_type_t pdo_type /**< PDO type */
                       )
{
    ec_eeprom_pdo_t *pdo;
    ec_eeprom_pdo_entry_t *entry;
    unsigned int entry_count, i;

    while (word_count >= 4) {
        if (!(pdo = (ec_eeprom_pdo_t *)
              kmalloc(sizeof(ec_eeprom_pdo_t), GFP_KERNEL))) {
            EC_ERR("Failed to allocate PDO memory.\n");
            return -1;
        }

        INIT_LIST_HEAD(&pdo->entries);
        pdo->type = pdo_type;

        pdo->index = *((uint16_t *) data);
        entry_count = data[2];
        pdo->sync_manager = data[3];
        pdo->name = NULL;
        ec_slave_locate_string(slave, data[5], &pdo->name);

        list_add_tail(&pdo->list, &slave->eeprom_pdos);

        word_count -= 4;
        data += 8;

        for (i = 0; i < entry_count; i++) {
            if (!(entry = (ec_eeprom_pdo_entry_t *)
                  kmalloc(sizeof(ec_eeprom_pdo_entry_t), GFP_KERNEL))) {
                EC_ERR("Failed to allocate PDO entry memory.\n");
                return -1;
            }

            entry->index = *((uint16_t *) data);
            entry->subindex = data[2];
            entry->name = NULL;
            ec_slave_locate_string(slave, data[3], &entry->name);
            entry->bit_length = data[5];

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
    ec_eeprom_string_t *string;
    char *err_string;

    // Erst alten Speicher freigeben
    if (*ptr) {
        kfree(*ptr);
        *ptr = NULL;
    }

    // Index 0 bedeutet "nicht belegt"
    if (!index) return 0;

    // EEPROM-String mit Index finden und kopieren
    list_for_each_entry(string, &slave->eeprom_strings, list) {
        if (--index) continue;

        if (!(*ptr = (char *) kmalloc(string->size + 1, GFP_KERNEL))) {
            EC_ERR("Unable to allocate string memory.\n");
            return -1;
        }
        memcpy(*ptr, string->data, string->size + 1);
        return 0;
    }

    EC_WARN("String %i not found in slave %i.\n", index, slave->ring_position);

    err_string = "(string not found)";

    if (!(*ptr = (char *) kmalloc(strlen(err_string) + 1, GFP_KERNEL))) {
        EC_ERR("Unable to allocate string memory.\n");
        return -1;
    }

    memcpy(*ptr, err_string, strlen(err_string) + 1);
    return 0;
}

/*****************************************************************************/

/**
   Acknowledges an error after a state transition.
*/

void ec_slave_state_ack(ec_slave_t *slave, /**< EtherCAT slave */
                        uint8_t state /**< previous state */
                        )
{
    ec_datagram_t *datagram;
    cycles_t start, end, timeout;

    datagram = &slave->master->simple_datagram;

    if (ec_datagram_npwr(datagram, slave->station_address, 0x0120, 2)) return;
    EC_WRITE_U16(datagram->data, state | EC_ACK);
    if (unlikely(ec_master_simple_io(slave->master, datagram))) {
        EC_WARN("Acknowledge sending failed on slave %i!\n",
                slave->ring_position);
        return;
    }

    start = get_cycles();
    timeout = (cycles_t) 10 * cpu_khz; // 10ms

    while (1)
    {
        udelay(100); // wait a little bit...

        if (ec_datagram_nprd(datagram, slave->station_address, 0x0130, 2))
            return;
        if (unlikely(ec_master_simple_io(slave->master, datagram))) {
            slave->current_state = EC_SLAVE_STATE_UNKNOWN;
            EC_WARN("Acknowledge checking failed on slave %i!\n",
                    slave->ring_position);
            return;
        }

        end = get_cycles();

        if (likely(EC_READ_U8(datagram->data) == state)) {
            slave->current_state = state;
            EC_INFO("Acknowleged state 0x%02X on slave %i.\n", state,
                    slave->ring_position);
            return;
        }

        if (unlikely((end - start) >= timeout)) {
            slave->current_state = EC_SLAVE_STATE_UNKNOWN;
            EC_WARN("Failed to acknowledge state 0x%02X on slave %i"
                    " - Timeout!\n", state, slave->ring_position);
            return;
        }
    }
}

/*****************************************************************************/

/**
   Reads the AL status code of a slave and displays it.
   If the AL status code is not supported, or if no error occurred (both
   resulting in code = 0), nothing is displayed.
*/

void ec_slave_read_al_status_code(ec_slave_t *slave /**< EtherCAT slave */)
{
    ec_datagram_t *datagram;
    uint16_t code;
    const ec_code_msg_t *al_msg;

    datagram = &slave->master->simple_datagram;

    if (ec_datagram_nprd(datagram, slave->station_address, 0x0134, 2)) return;
    if (unlikely(ec_master_simple_io(slave->master, datagram))) {
        EC_WARN("Failed to read AL status code on slave %i!\n",
                slave->ring_position);
        return;
    }

    if (!(code = EC_READ_U16(datagram->data))) return;

    for (al_msg = al_status_messages; al_msg->code; al_msg++) {
        if (al_msg->code == code) {
            EC_ERR("AL status message 0x%04X: \"%s\".\n",
                   al_msg->code, al_msg->message);
            return;
        }
    }

    EC_ERR("Unknown AL status code 0x%04X.\n", code);
}

/*****************************************************************************/

/**
   Does a state transition.
   \return 0 in case of success, else < 0
*/

int ec_slave_state_change(ec_slave_t *slave, /**< EtherCAT slave */
                          uint8_t state /**< new state */
                          )
{
    ec_datagram_t *datagram;
    cycles_t start, end, timeout;

    datagram = &slave->master->simple_datagram;

    slave->requested_state = state;

    if (ec_datagram_npwr(datagram, slave->station_address, 0x0120, 2))
        return -1;
    EC_WRITE_U16(datagram->data, state);
    if (unlikely(ec_master_simple_io(slave->master, datagram))) {
        EC_ERR("Failed to set state 0x%02X on slave %i!\n",
               state, slave->ring_position);
        return -1;
    }

    start = get_cycles();
    timeout = (cycles_t) 10 * cpu_khz; // 10ms

    while (1)
    {
        udelay(100); // wait a little bit

        if (ec_datagram_nprd(datagram, slave->station_address, 0x0130, 2))
            return -1;
        if (unlikely(ec_master_simple_io(slave->master, datagram))) {
            slave->current_state = EC_SLAVE_STATE_UNKNOWN;
            EC_ERR("Failed to check state 0x%02X on slave %i!\n",
                   state, slave->ring_position);
            return -1;
        }

        end = get_cycles();

        if (unlikely(EC_READ_U8(datagram->data) & 0x10)) {
            // state change error
            EC_ERR("Failed to set state 0x%02X - Slave %i refused state change"
                   " (code 0x%02X)!\n", state, slave->ring_position,
                   EC_READ_U8(datagram->data));
            slave->current_state = EC_READ_U8(datagram->data);
            state = slave->current_state & 0x0F;
            ec_slave_read_al_status_code(slave);
            ec_slave_state_ack(slave, state);
            return -1;
        }

        if (likely(EC_READ_U8(datagram->data) == (state & 0x0F))) {
            slave->current_state = state;
            return 0; // state change successful
        }

        if (unlikely((end - start) >= timeout)) {
            slave->current_state = EC_SLAVE_STATE_UNKNOWN;
            EC_ERR("Failed to check state 0x%02X of slave %i - Timeout!\n",
                   state, slave->ring_position);
            return -1;
        }
    }
}

/*****************************************************************************/

/**
   Prepares an FMMU configuration.
   Configuration data for the FMMU is saved in the slave structure and is
   written to the slave in ecrt_master_activate().
   The FMMU configuration is done in a way, that the complete data range
   of the corresponding sync manager is covered. Seperate FMMUs arce configured
   for each domain.
   If the FMMU configuration is already prepared, the function returns with
   success.
   \return 0 in case of success, else < 0
*/

int ec_slave_prepare_fmmu(ec_slave_t *slave, /**< EtherCAT slave */
                          const ec_domain_t *domain, /**< domain */
                          const ec_sync_t *sync  /**< sync manager */
                          )
{
    unsigned int i;

    // FMMU configuration already prepared?
    for (i = 0; i < slave->fmmu_count; i++)
        if (slave->fmmus[i].domain == domain && slave->fmmus[i].sync == sync)
            return 0;

    // reserve new FMMU...

    if (slave->fmmu_count >= slave->base_fmmu_count) {
        EC_ERR("Slave %i FMMU limit reached!\n", slave->ring_position);
        return -1;
    }

    slave->fmmus[slave->fmmu_count].domain = domain;
    slave->fmmus[slave->fmmu_count].sync = sync;
    slave->fmmus[slave->fmmu_count].logical_start_address = 0;
    slave->fmmu_count++;
    slave->registered = 1;

    return 0;
}

/*****************************************************************************/

/**
   Outputs all information about a certain slave.
   Verbosity:
   - 0: Only slave types and addresses
   - 1: with EEPROM information
   - >1: with SDO dictionaries
*/

void ec_slave_print(const ec_slave_t *slave, /**< EtherCAT slave */
                    unsigned int verbosity /**< verbosity level */
                    )
{
    ec_eeprom_sync_t *sync;
    ec_eeprom_pdo_t *pdo;
    ec_eeprom_pdo_entry_t *pdo_entry;
    ec_sdo_t *sdo;
    ec_sdo_entry_t *sdo_entry;
    int first, i;

    if (slave->type) {
        EC_INFO("%i) %s %s: %s\n", slave->ring_position,
                slave->type->vendor_name, slave->type->product_name,
                slave->type->description);
    }
    else {
        EC_INFO("%i) UNKNOWN SLAVE: vendor 0x%08X, product 0x%08X\n",
                slave->ring_position, slave->sii_vendor_id,
                slave->sii_product_code);
    }

    if (!verbosity) return;

    EC_INFO("  Station address: 0x%04X\n", slave->station_address);

    EC_INFO("  Data link status:\n");
    for (i = 0; i < 4; i++) {
        EC_INFO("    Port %i (", i);
        switch (slave->sii_physical_layer[i]) {
            case 0x00:
                printk("EBUS");
                break;
            case 0x01:
                printk("100BASE-TX");
                break;
            case 0x02:
                printk("100BASE-FX");
                break;
            default:
                printk("unknown");
        }
        printk(")\n");
        EC_INFO("      link %s, loop %s, %s\n",
                slave->dl_link[i] ? "up" : "down",
                slave->dl_loop[i] ? "closed" : "open",
                slave->dl_signal[i] ? "signal detected" : "no signal");
    }

    EC_INFO("  Base information:\n");
    EC_INFO("    Type %u, revision %i, build %i\n",
            slave->base_type, slave->base_revision, slave->base_build);
    EC_INFO("    Supported FMMUs: %i, sync managers: %i\n",
            slave->base_fmmu_count, slave->base_sync_count);

    if (slave->sii_mailbox_protocols) {
        EC_INFO("  Mailbox communication:\n");
        EC_INFO("    RX mailbox: 0x%04X/%i, TX mailbox: 0x%04X/%i\n",
                slave->sii_rx_mailbox_offset, slave->sii_rx_mailbox_size,
                slave->sii_tx_mailbox_offset, slave->sii_tx_mailbox_size);
        EC_INFO("    Supported protocols: ");

        first = 1;
        if (slave->sii_mailbox_protocols & EC_MBOX_AOE) {
            printk("AoE");
            first = 0;
        }
        if (slave->sii_mailbox_protocols & EC_MBOX_EOE) {
            if (!first) printk(", ");
            printk("EoE");
            first = 0;
        }
        if (slave->sii_mailbox_protocols & EC_MBOX_COE) {
            if (!first) printk(", ");
            printk("CoE");
            first = 0;
        }
        if (slave->sii_mailbox_protocols & EC_MBOX_FOE) {
            if (!first) printk(", ");
            printk("FoE");
            first = 0;
        }
        if (slave->sii_mailbox_protocols & EC_MBOX_SOE) {
            if (!first) printk(", ");
            printk("SoE");
            first = 0;
        }
        if (slave->sii_mailbox_protocols & EC_MBOX_VOE) {
            if (!first) printk(", ");
            printk("VoE");
        }
        printk("\n");
    }

    EC_INFO("  EEPROM data:\n");

    EC_INFO("    EEPROM content size: %i Bytes\n", slave->eeprom_size);

    if (slave->sii_alias)
        EC_INFO("    Configured station alias: 0x%04X (%i)\n",
                slave->sii_alias, slave->sii_alias);

    EC_INFO("    Vendor-ID: 0x%08X, Product code: 0x%08X\n",
            slave->sii_vendor_id, slave->sii_product_code);
    EC_INFO("    Revision number: 0x%08X, Serial number: 0x%08X\n",
            slave->sii_revision_number, slave->sii_serial_number);

    if (slave->eeprom_group)
        EC_INFO("    Group: %s\n", slave->eeprom_group);
    if (slave->eeprom_image)
        EC_INFO("    Image: %s\n", slave->eeprom_image);
    if (slave->eeprom_order)
        EC_INFO("    Order#: %s\n", slave->eeprom_order);
    if (slave->eeprom_name)
        EC_INFO("    Name: %s\n", slave->eeprom_name);

    if (!list_empty(&slave->eeprom_syncs)) {
        EC_INFO("    Sync-Managers:\n");
        list_for_each_entry(sync, &slave->eeprom_syncs, list) {
            EC_INFO("      %i: 0x%04X, length %i, control 0x%02X, %s\n",
                    sync->index, sync->physical_start_address,
                    sync->length, sync->control_register,
                    sync->enable ? "enable" : "disable");
        }
    }

    list_for_each_entry(pdo, &slave->eeprom_pdos, list) {
        EC_INFO("    %s \"%s\" (0x%04X), -> Sync-Manager %i\n",
                pdo->type == EC_RX_PDO ? "RXPDO" : "TXPDO",
                pdo->name ? pdo->name : "???",
                pdo->index, pdo->sync_manager);

        list_for_each_entry(pdo_entry, &pdo->entries, list) {
            EC_INFO("      \"%s\" 0x%04X:%X, %i Bit\n",
                    pdo_entry->name ? pdo_entry->name : "???",
                    pdo_entry->index, pdo_entry->subindex,
                    pdo_entry->bit_length);
        }
    }

    if (verbosity < 2) return;

    if (!list_empty(&slave->sdo_dictionary)) {
        EC_INFO("    SDO-Dictionary:\n");
        list_for_each_entry(sdo, &slave->sdo_dictionary, list) {
            EC_INFO("      0x%04X \"%s\"\n", sdo->index,
                    sdo->name ? sdo->name : "");
            EC_INFO("        Object code: 0x%02X\n", sdo->object_code);
            list_for_each_entry(sdo_entry, &sdo->entries, list) {
                EC_INFO("        0x%04X:%i \"%s\", type 0x%04X, %i bits\n",
                        sdo->index, sdo_entry->subindex,
                        sdo_entry->name ? sdo_entry->name : "",
                        sdo_entry->data_type, sdo_entry->bit_length);
            }
        }
    }
}

/*****************************************************************************/

/**
   Outputs the values of the CRC faoult counters and resets them.
   \return 0 in case of success, else < 0
*/

int ec_slave_check_crc(ec_slave_t *slave /**< EtherCAT slave */)
{
    ec_datagram_t *datagram;

    datagram = &slave->master->simple_datagram;

    if (ec_datagram_nprd(datagram, slave->station_address, 0x0300, 4))
        return -1;
    if (unlikely(ec_master_simple_io(slave->master, datagram))) {
        EC_WARN("Reading CRC fault counters failed on slave %i!\n",
                slave->ring_position);
        return -1;
    }

    if (!EC_READ_U32(datagram->data)) return 0; // no CRC faults

    if (EC_READ_U8(datagram->data))
        EC_WARN("%3i RX-error%s on slave %i, channel A.\n",
                EC_READ_U8(datagram->data),
                EC_READ_U8(datagram->data) == 1 ? "" : "s",
                slave->ring_position);
    if (EC_READ_U8(datagram->data + 1))
        EC_WARN("%3i invalid frame%s on slave %i, channel A.\n",
                EC_READ_U8(datagram->data + 1),
                EC_READ_U8(datagram->data + 1) == 1 ? "" : "s",
                slave->ring_position);
    if (EC_READ_U8(datagram->data + 2))
        EC_WARN("%3i RX-error%s on slave %i, channel B.\n",
                EC_READ_U8(datagram->data + 2),
                EC_READ_U8(datagram->data + 2) == 1 ? "" : "s",
                slave->ring_position);
    if (EC_READ_U8(datagram->data + 3))
        EC_WARN("%3i invalid frame%s on slave %i, channel B.\n",
                EC_READ_U8(datagram->data + 3),
                EC_READ_U8(datagram->data + 3) == 1 ? "" : "s",
                slave->ring_position);

    // reset CRC counters
    if (ec_datagram_npwr(datagram, slave->station_address, 0x0300, 4))
        return -1;
    EC_WRITE_U32(datagram->data, 0x00000000);
    if (unlikely(ec_master_simple_io(slave->master, datagram))) {
        EC_WARN("Resetting CRC fault counters failed on slave %i!\n",
                slave->ring_position);
        return -1;
    }

    return 0;
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

    if (slave->master->mode != EC_MASTER_MODE_FREERUN) {
        EC_ERR("Writing EEPROMs only allowed in freerun mode!\n");
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

    if (attr == &attr_ring_position) {
        return sprintf(buffer, "%i\n", slave->ring_position);
    }
    else if (attr == &attr_coupler_address) {
        return sprintf(buffer, "%i:%i\n", slave->coupler_index,
                       slave->coupler_subindex);
    }
    else if (attr == &attr_vendor_name) {
        if (slave->type)
            return sprintf(buffer, "%s\n", slave->type->vendor_name);
    }
    else if (attr == &attr_product_name) {
        if (slave->type)
            return sprintf(buffer, "%s\n", slave->type->product_name);
    }
    else if (attr == &attr_product_desc) {
        if (slave->type)
            return sprintf(buffer, "%s\n", slave->type->description);
    }
    else if (attr == &attr_sii_name) {
        if (slave->eeprom_name)
            return sprintf(buffer, "%s\n", slave->eeprom_name);
    }
    else if (attr == &attr_type) {
        if (slave->type) {
            if (slave->type->special == EC_TYPE_BUS_COUPLER)
                return sprintf(buffer, "coupler\n");
	    else if (slave->type->special == EC_TYPE_INFRA)
                return sprintf(buffer, "infrastructure\n");
            else
                return sprintf(buffer, "normal\n");
        }
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

        EC_INFO("Accepted new state %s for slave %i.\n",
                buffer, slave->ring_position);
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
   \return size of sync manager contents
*/

size_t ec_slave_calc_sync_size(const ec_slave_t *slave, /**< EtherCAT slave */
                               const ec_sync_t *sync /**< sync manager */
                               )
{
    unsigned int i, found;
    const ec_field_t *field;
    const ec_varsize_t *var;
    size_t size;

    // if size is specified, return size
    if (sync->size) return sync->size;

    // sync manager has variable size (size == 0).

    size = 0;
    for (i = 0; (field = sync->fields[i]); i++) {
        found = 0;
        list_for_each_entry(var, &slave->varsize_fields, list) {
            if (var->field != field) continue;
            size += var->size;
            found = 1;
        }

        if (!found) {
            EC_WARN("Variable data field \"%s\" of slave %i has no size"
                    " information!\n", field->name, slave->ring_position);
        }
    }
    return size;
}

/******************************************************************************
 *  Realtime interface
 *****************************************************************************/

/**
   Writes the "configured station alias" to the slave's EEPROM.
   \return 0 in case of success, else < 0
   \ingroup RealtimeInterface
*/

int ecrt_slave_write_alias(ec_slave_t *slave, /**< EtherCAT slave */
                           uint16_t alias /**< new alias */
                           )
{
    return ec_slave_sii_write16(slave, 0x0004, alias);
}

/*****************************************************************************/

/**
   \return 0 in case of success, else < 0
   \ingroup RealtimeInterface
*/

int ecrt_slave_field_size(ec_slave_t *slave, /**< EtherCAT slave */
                          const char *field_name, /**< data field name */
                          unsigned int field_index, /**< data field index */
                          size_t size /**< new data field size */
                          )
{
    unsigned int i, j, field_counter;
    const ec_sync_t *sync;
    const ec_field_t *field;
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
}

/*****************************************************************************/

/**< \cond */

EXPORT_SYMBOL(ecrt_slave_write_alias);
EXPORT_SYMBOL(ecrt_slave_field_size);

/**< \endcond */

/*****************************************************************************/
