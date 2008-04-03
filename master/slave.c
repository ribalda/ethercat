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

void ec_slave_clear(struct kobject *);
void ec_slave_sdos_clear(struct kobject *);
ssize_t ec_show_slave_attribute(struct kobject *, struct attribute *, char *);
ssize_t ec_store_slave_attribute(struct kobject *, struct attribute *,
                                 const char *, size_t);
char *ec_slave_sii_string(ec_slave_t *, unsigned int);

/*****************************************************************************/

/** \cond */

EC_SYSFS_READ_ATTR(info);
EC_SYSFS_READ_WRITE_ATTR(state);
EC_SYSFS_READ_WRITE_ATTR(sii);
EC_SYSFS_READ_WRITE_ATTR(alias);

static struct attribute *def_attrs[] = {
    &attr_info,
    &attr_state,
    &attr_sii,
    &attr_alias,
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

static struct kobj_type ktype_ec_slave_sdos = {
    .release = ec_slave_sdos_clear
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

    slave->master = master;

    slave->config = NULL;
    slave->requested_state = EC_SLAVE_STATE_PREOP;
    slave->current_state = EC_SLAVE_STATE_UNKNOWN;
    slave->online_state = EC_SLAVE_ONLINE;
    slave->self_configured = 0;
    slave->error_flag = 0;

    slave->base_type = 0;
    slave->base_revision = 0;
    slave->base_build = 0;
    slave->base_fmmu_count = 0;

    slave->sii_data = NULL;
    slave->sii_size = 0;

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

    for (i = 0; i < 4; i++) {
        slave->dl_link[i] = 0;
        slave->dl_loop[i] = 0;
        slave->dl_signal[i] = 0;
        slave->sii.physical_layer[i] = 0xFF;
    }

    // init kobject and add it to the hierarchy
    memset(&slave->kobj, 0x00, sizeof(struct kobject));
    kobject_init(&slave->kobj);
    slave->kobj.ktype = &ktype_ec_slave;
    slave->kobj.parent = &master->kobj;
    if (kobject_set_name(&slave->kobj, "slave%03i", slave->ring_position)) {
        EC_ERR("Failed to set kobject name.\n");
        goto out_slave_put;
    }
    if (kobject_add(&slave->kobj)) {
        EC_ERR("Failed to add slave's kobject.\n");
        goto out_slave_put;
    }

    // init Sdo kobject and add it to the hierarchy
    memset(&slave->sdo_kobj, 0x00, sizeof(struct kobject));
    kobject_init(&slave->sdo_kobj);
    slave->sdo_kobj.ktype = &ktype_ec_slave_sdos;
    slave->sdo_kobj.parent = &slave->kobj;
    if (kobject_set_name(&slave->sdo_kobj, "sdos")) {
        EC_ERR("Failed to set kobject name.\n");
        goto out_sdo_put;
    }
    if (kobject_add(&slave->sdo_kobj)) {
        EC_ERR("Failed to add Sdos kobject.\n");
        goto out_sdo_put;
    }

    return 0;

 out_sdo_put:
    kobject_put(&slave->sdo_kobj);
    kobject_del(&slave->kobj);
 out_slave_put:
    kobject_put(&slave->kobj);
    return -1;
}

/*****************************************************************************/

/**
   Slave destructor.
   Clears and frees a slave object.
*/

void ec_slave_destroy(ec_slave_t *slave /**< EtherCAT slave */)
{
    ec_sdo_t *sdo, *next_sdo;

    if (slave->config)
        ec_slave_config_detach(slave->config);

    // free all Sdos
    list_for_each_entry_safe(sdo, next_sdo, &slave->sdo_dictionary, list) {
        list_del(&sdo->list);
        ec_sdo_destroy(sdo);
    }

    // free Sdo kobject
    kobject_del(&slave->sdo_kobj);
    kobject_put(&slave->sdo_kobj);

    // destroy self
    kobject_del(&slave->kobj);
    kobject_put(&slave->kobj);
}

/*****************************************************************************/

/**
   Clear and free slave.
   This method is called by the kobject,
   once there are no more references to it.
*/

void ec_slave_clear(struct kobject *kobj /**< kobject of the slave */)
{
    ec_slave_t *slave;
    ec_pdo_t *pdo, *next_pdo;
    unsigned int i;

    slave = container_of(kobj, ec_slave_t, kobj);

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

    if (slave->sii_data)
        kfree(slave->sii_data);

    kfree(slave);
}

/*****************************************************************************/

/**
 * Sdo kobject clear method.
 */

void ec_slave_sdos_clear(struct kobject *kobj /**< kobject for Sdos */)
{
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
 * Sets the online state of a slave.
 */

void ec_slave_set_online_state(ec_slave_t *slave, /**< EtherCAT slave */
        ec_slave_online_state_t new_state /**< new online state */
        )
{
    if (new_state == EC_SLAVE_OFFLINE &&
            slave->online_state == EC_SLAVE_ONLINE) {
        if (slave->master->debug_level)
            EC_DBG("Slave %u: offline.\n", slave->ring_position);
    }
    else if (new_state == EC_SLAVE_ONLINE &&
            slave->online_state == EC_SLAVE_OFFLINE) {
        slave->error_flag = 0; // clear error flag
        if (slave->master->debug_level) {
            char cur_state[EC_STATE_STRING_SIZE];
            ec_state_string(slave->current_state, cur_state);
            EC_DBG("Slave %u: online (%s).\n",
                   slave->ring_position, cur_state);
        }
    }

    slave->online_state = new_state;
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

    if (slave->master->debug_level)
        EC_DBG("Found Sync manager category with %u sync managers.\n", count);
    
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

    if (slave->master->debug_level)
        EC_DBG("Total sync managers: %u.\n", slave->sii.sync_count);

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

/** Outputs all information about a certain slave.
 */
ssize_t ec_slave_info(const ec_slave_t *slave, /**< EtherCAT slave */
        char *buffer /**< Output buffer */
        )
{
    const ec_sync_t *sync;
    const ec_pdo_t *pdo;
    const ec_pdo_entry_t *pdo_entry;
    int first, i;
    char *large_buffer, *buf;
    unsigned int size;

    if (!(large_buffer = (char *) kmalloc(PAGE_SIZE * 2, GFP_KERNEL))) {
        return -ENOMEM;
    }

    buf = large_buffer;

    buf += sprintf(buf, "Ring position: %u\n",
                   slave->ring_position);
    buf += sprintf(buf, "State: ");
    buf += ec_state_string(slave->current_state, buf);
    buf += sprintf(buf, " (");
    buf += ec_state_string(slave->requested_state, buf);
    buf += sprintf(buf, ")\n");
    buf += sprintf(buf, "Flags: %s\n\n", slave->error_flag ? "ERROR" : "ok");

    buf += sprintf(buf, "Data link status:\n");
    for (i = 0; i < 4; i++) {
        buf += sprintf(buf, "  Port %u: Phy %u (",
                i, slave->sii.physical_layer[i]);
        switch (slave->sii.physical_layer[i]) {
            case 0x00:
                buf += sprintf(buf, "EBUS");
                break;
            case 0x01:
                buf += sprintf(buf, "100BASE-TX");
                break;
            case 0x02:
                buf += sprintf(buf, "100BASE-FX");
                break;
            default:
                buf += sprintf(buf, "unknown");
        }
        buf += sprintf(buf, "), Link %s, Loop %s, %s\n",
                       slave->dl_link[i] ? "up" : "down",
                       slave->dl_loop[i] ? "closed" : "open",
                       slave->dl_signal[i] ? "Signal detected" : "No signal");
    }
    buf += sprintf(buf, "\n");

    if (slave->sii.alias)
        buf += sprintf(buf, "Configured station alias:"
                       " 0x%04X (%u)\n\n", slave->sii.alias, slave->sii.alias);

    buf += sprintf(buf, "Identity:\n");
    buf += sprintf(buf, "  Vendor ID: 0x%08X (%u)\n",
                   slave->sii.vendor_id, slave->sii.vendor_id);
    buf += sprintf(buf, "  Product code: 0x%08X (%u)\n",
                   slave->sii.product_code, slave->sii.product_code);
    buf += sprintf(buf, "  Revision number: 0x%08X (%u)\n",
                   slave->sii.revision_number, slave->sii.revision_number);
    buf += sprintf(buf, "  Serial number: 0x%08X (%u)\n\n",
                   slave->sii.serial_number, slave->sii.serial_number);

    if (slave->sii.mailbox_protocols) {
        buf += sprintf(buf, "Mailboxes:\n");
        buf += sprintf(buf, "  RX: 0x%04X/%u, TX: 0x%04X/%u\n",
                slave->sii.rx_mailbox_offset, slave->sii.rx_mailbox_size,
                slave->sii.tx_mailbox_offset, slave->sii.tx_mailbox_size);
        buf += sprintf(buf, "  Supported protocols: ");

        first = 1;
        if (slave->sii.mailbox_protocols & EC_MBOX_AOE) {
            buf += sprintf(buf, "AoE");
            first = 0;
        }
        if (slave->sii.mailbox_protocols & EC_MBOX_EOE) {
            if (!first) buf += sprintf(buf, ", ");
            buf += sprintf(buf, "EoE");
            first = 0;
        }
        if (slave->sii.mailbox_protocols & EC_MBOX_COE) {
            if (!first) buf += sprintf(buf, ", ");
            buf += sprintf(buf, "CoE");
            first = 0;
        }
        if (slave->sii.mailbox_protocols & EC_MBOX_FOE) {
            if (!first) buf += sprintf(buf, ", ");
            buf += sprintf(buf, "FoE");
            first = 0;
        }
        if (slave->sii.mailbox_protocols & EC_MBOX_SOE) {
            if (!first) buf += sprintf(buf, ", ");
            buf += sprintf(buf, "SoE");
            first = 0;
        }
        if (slave->sii.mailbox_protocols & EC_MBOX_VOE) {
            if (!first) buf += sprintf(buf, ", ");
            buf += sprintf(buf, "VoE");
        }
        buf += sprintf(buf, "\n\n");
    }

    if (slave->sii.has_general) {
        buf += sprintf(buf, "General:\n");

        if (slave->sii.group)
            buf += sprintf(buf, "  Group: %s\n", slave->sii.group);
        if (slave->sii.image)
            buf += sprintf(buf, "  Image: %s\n", slave->sii.image);
        if (slave->sii.order)
            buf += sprintf(buf, "  Order number: %s\n",
                    slave->sii.order);
        if (slave->sii.name)
            buf += sprintf(buf, "  Name: %s\n", slave->sii.name);
        if (slave->sii.mailbox_protocols & EC_MBOX_COE) {
            buf += sprintf(buf, "  CoE details:\n");
            buf += sprintf(buf, "    Enable Sdo: %s\n",
                    slave->sii.coe_details.enable_sdo ? "yes" : "no");
            buf += sprintf(buf, "    Enable Sdo Info: %s\n",
                    slave->sii.coe_details.enable_sdo_info ? "yes" : "no");
            buf += sprintf(buf, "    Enable Pdo Assign: %s\n",
                    slave->sii.coe_details.enable_pdo_assign ? "yes" : "no");
            buf += sprintf(buf, "    Enable Pdo Configuration: %s\n",
                    slave->sii.coe_details.enable_pdo_configuration ?
                    "yes" : "no");
            buf += sprintf(buf, "    Enable Upload at startup: %s\n",
                    slave->sii.coe_details.enable_upload_at_startup ?
                    "yes" : "no");
            buf += sprintf(buf, "    Enable Sdo complete access: %s\n",
                    slave->sii.coe_details.enable_sdo_complete_access
                    ? "yes" : "no");
        }

        buf += sprintf(buf, "  Flags:\n");
        buf += sprintf(buf, "    Enable SafeOp: %s\n",
                slave->sii.general_flags.enable_safeop ? "yes" : "no");
        buf += sprintf(buf, "    Enable notLRW: %s\n",
                slave->sii.general_flags.enable_not_lrw ? "yes" : "no");
        buf += sprintf(buf, "  Current consumption: %i mA\n\n",
                slave->sii.current_on_ebus);
    }

    if (slave->sii.sync_count) {
        buf += sprintf(buf, "Sync managers / assigned Pdos:\n");

        for (i = 0; i < slave->sii.sync_count; i++) {
            sync = &slave->sii.syncs[i];
            buf += sprintf(buf,
                    "  SM%u: addr 0x%04X, size %u, control 0x%02X, %s\n",
                    sync->index, sync->physical_start_address,
                    sync->length, sync->control_register,
                    sync->enable ? "enable" : "disable");

            if (list_empty(&sync->pdos.list)) {
                buf += sprintf(buf, "    No Pdos assigned.\n");
            } else if (sync->assign_source != EC_ASSIGN_NONE) {
                buf += sprintf(buf, "    Pdo assignment from ");
                switch (sync->assign_source) {
                    case EC_ASSIGN_SII:
                        buf += sprintf(buf, "SII");
                        break;
                    case EC_ASSIGN_COE:
                        buf += sprintf(buf, "CoE");
                        break;
                    case EC_ASSIGN_CUSTOM:
                        buf += sprintf(buf, "application");
                        break;
                    default:
                        buf += sprintf(buf, "?");
                        break;
                }
                buf += sprintf(buf, ".\n");
            }

            list_for_each_entry(pdo, &sync->pdos.list, list) {
                buf += sprintf(buf, "    %s 0x%04X \"%s\"\n",
                        pdo->dir == EC_DIR_OUTPUT ? "RxPdo" : "TxPdo",
                        pdo->index, pdo->name ? pdo->name : "???");

                list_for_each_entry(pdo_entry, &pdo->entries, list) {
                    buf += sprintf(buf,
                            "      0x%04X:%X \"%s\", %u bit\n",
                            pdo_entry->index, pdo_entry->subindex,
                            pdo_entry->name ? pdo_entry->name : "???",
                            pdo_entry->bit_length);
                }
            }
        }
        buf += sprintf(buf, "\n");
    }

    // type-cast to avoid warnings on some compilers
    if (!list_empty((struct list_head *) &slave->sii.pdos)) {
        buf += sprintf(buf, "Available Pdos from SII:\n");

        list_for_each_entry(pdo, &slave->sii.pdos, list) {
            buf += sprintf(buf, "  %s 0x%04X \"%s\"",
                    pdo->dir == EC_DIR_OUTPUT ? "RxPdo" : "TxPdo",
                    pdo->index, pdo->name ? pdo->name : "???");
            if (pdo->sync_index >= 0)
                buf += sprintf(buf, ", default assignment: SM%u.\n",
                        pdo->sync_index);
            else
                buf += sprintf(buf, ", no default assignment.\n");

            list_for_each_entry(pdo_entry, &pdo->entries, list) {
                buf += sprintf(buf, "    0x%04X:%X \"%s\", %u bit\n",
                        pdo_entry->index, pdo_entry->subindex,
                        pdo_entry->name ? pdo_entry->name : "???",
                        pdo_entry->bit_length);
            }
        }
        buf += sprintf(buf, "\n");
    }

    size = buf - large_buffer;
    if (size >= PAGE_SIZE) {
        const char trunc[] = "\n---TRUNCATED---\n";
        unsigned int len = strlen(trunc);
        memcpy(large_buffer + PAGE_SIZE - len, trunc, len);
    }

    size = min(size, (unsigned int) PAGE_SIZE);
    memcpy(buffer, large_buffer, size);
    kfree(large_buffer);
    return size;
}

/*****************************************************************************/

/**
 * Schedules an SII write request.
 * \return 0 case of success, otherwise error code.
 */

int ec_slave_schedule_sii_writing(
        ec_sii_write_request_t *request /**< SII write request */
        )
{
    ec_master_t *master = request->slave->master;

    request->state = EC_REQUEST_QUEUED;

    // schedule SII write request.
    down(&master->sii_sem);
    list_add_tail(&request->list, &master->sii_requests);
    up(&master->sii_sem);

    // wait for processing through FSM
    if (wait_event_interruptible(master->sii_queue,
                request->state != EC_REQUEST_QUEUED)) {
        // interrupted by signal
        down(&master->sii_sem);
        if (request->state == EC_REQUEST_QUEUED) {
            list_del(&request->list);
            up(&master->sii_sem);
            return -EINTR;
        }
        // request already processing: interrupt not possible.
        up(&master->sii_sem);
    }

    // wait until master FSM has finished processing
    wait_event(master->sii_queue,
            request->state != EC_REQUEST_BUSY);

    return request->state == EC_REQUEST_SUCCESS ? 0 : -EIO;
}

/*****************************************************************************/

/**
 * Calculates the SII checksum field.
 *
 * The checksum is generated with the polynom x^8+x^2+x+1 (0x07) and an
 * initial value of 0xff (see IEC 61158-6-12 ch. 5.4).
 *
 * The below code was originally generated with PYCRC
 * http://www.tty1.net/pycrc
 *
 * ./pycrc.py --width=8 --poly=0x07 --reflect-in=0 --xor-in=0xff
 *   --reflect-out=0 --xor-out=0 --generate c --algorithm=bit-by-bit
 *
 * \return CRC8
 */

uint8_t ec_slave_sii_crc(
        const uint8_t *data, /**< pointer to data */
        size_t length /**< number of bytes in \a data */
        )
{
    unsigned int i;
    uint8_t bit, byte, crc = 0x48;

    while (length--) {
        byte = *data++;
        for (i = 0; i < 8; i++) {
            bit = crc & 0x80;
            crc = (crc << 1) | ((byte >> (7 - i)) & 0x01);
            if (bit) crc ^= 0x07;
        }
    }

    for (i = 0; i < 8; i++) {
        bit = crc & 0x80;
        crc <<= 1;
        if (bit) crc ^= 0x07;
    }

    return crc;
}

/*****************************************************************************/

/**
 * Writes complete SII contents to a slave.
 * \return data size written in case of success, otherwise error code.
 */

ssize_t ec_slave_write_sii(ec_slave_t *slave, /**< EtherCAT slave */
        const uint8_t *data, /**< new SII data */
        size_t size /**< size of data in bytes */
        )
{
    ec_sii_write_request_t request;
    const uint16_t *cat_header;
    uint16_t cat_type, cat_size;
    int ret;
    uint8_t crc;

    if (slave->master->mode != EC_MASTER_MODE_IDLE) { // FIXME
        EC_ERR("Writing SIIs only allowed in idle mode!\n");
        return -EBUSY;
    }

    if (size % 2) {
        EC_ERR("SII data size is odd (%u bytes)! SII data must be"
                " word-aligned. Dropping.\n", size);
        return -EINVAL;
    }

    // init SII write request
    INIT_LIST_HEAD(&request.list);
    request.slave = slave;
    request.data = data;
    request.word_offset = 0;
    request.word_size = size / 2;

    if (request.word_size < 0x0041) {
        EC_ERR("SII data too short (%u words)! Mimimum is"
                " 40 fixed words + 1 delimiter. Dropping.\n",
                request.word_size);
        return -EINVAL;
    }

    // calculate checksum
    crc = ec_slave_sii_crc(data, 14); // CRC over words 0 to 6
    if (crc != data[14]) {
        EC_WARN("SII CRC incorrect. Must be 0x%02x.\n", crc);
    }

    cat_header = (const uint16_t *) request.data
		+ EC_FIRST_SII_CATEGORY_OFFSET;
    cat_type = EC_READ_U16(cat_header);
    while (cat_type != 0xFFFF) { // cycle through categories
        if (cat_header + 1 >
				(const uint16_t *) request.data + request.word_size) {
            EC_ERR("SII data corrupted! Dropping.\n");
            return -EINVAL;
        }
        cat_size = EC_READ_U16(cat_header + 1);
        if (cat_header + cat_size + 2 >
				(const uint16_t *) request.data + request.word_size) {
            EC_ERR("SII data corrupted! Dropping.\n");
            return -EINVAL;
        }
        cat_header += cat_size + 2;
        cat_type = EC_READ_U16(cat_header);
    }

    // SII data ok. schedule writing.
    if ((ret = ec_slave_schedule_sii_writing(&request)))
        return ret; // error code

    return size; // success
}

/*****************************************************************************/

/**
 * Writes the Secondary slave address (alias) to the slave's SII.
 * \return data size written in case of success, otherwise error code.
 */

ssize_t ec_slave_write_alias(ec_slave_t *slave, /**< EtherCAT slave */
        const uint8_t *data, /**< alias string */
        size_t size /**< size of data in bytes */
        )
{
    ec_sii_write_request_t request;
    char *remainder;
    uint16_t alias;
    int ret;
    uint8_t sii_data[16], crc;

    if (slave->master->mode != EC_MASTER_MODE_IDLE) { // FIXME
        EC_ERR("Writing to SII is only allowed in idle mode!\n");
        return -EBUSY;
    }

    alias = simple_strtoul(data, &remainder, 0);
    if (remainder == (char *) data || (*remainder && *remainder != '\n')) {
        EC_ERR("Invalid alias value! Dropping.\n");
        return -EINVAL;
    }

    if (!slave->sii_data || slave->sii_size < 16) {
        EC_ERR("Failed to read SII contents from slave %u.\n",
                slave->ring_position);
        return -EINVAL;
    }

    // copy first 7 words of recent SII contents
    memcpy(sii_data, slave->sii_data, 14);
    
    // write new alias address in word 4
    EC_WRITE_U16(sii_data + 8, alias);

    // calculate new checksum over words 0 to 6
    crc = ec_slave_sii_crc(sii_data, 14);
    EC_WRITE_U16(sii_data + 14, crc);

    // init SII write request
    INIT_LIST_HEAD(&request.list);
    request.slave = slave;
    request.data = sii_data;
    request.word_offset = 0x0000;
    request.word_size = 8;

    if ((ret = ec_slave_schedule_sii_writing(&request)))
        return ret; // error code

    slave->sii.alias = alias; // FIXME: do this in state machine

    return size; // success
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
            case EC_SLAVE_STATE_SAFEOP:
                return sprintf(buffer, "SAFEOP\n");
            case EC_SLAVE_STATE_OP:
                return sprintf(buffer, "OP\n");
            default:
                return sprintf(buffer, "UNKNOWN\n");
        }
    }
    else if (attr == &attr_sii) {
        if (slave->sii_data) {
            if (slave->sii_size > PAGE_SIZE) {
                EC_ERR("SII contents of slave %u exceed 1 page (%u/%u).\n",
                       slave->ring_position, slave->sii_size,
                       (int) PAGE_SIZE);
            }
            else {
                memcpy(buffer, slave->sii_data, slave->sii_size);
                return slave->sii_size;
            }
        }
    }
    else if (attr == &attr_alias) {
        return sprintf(buffer, "%u\n", slave->sii.alias);
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
            ec_slave_request_state(slave, EC_SLAVE_STATE_INIT);
        else if (!strcmp(buffer, "PREOP\n"))
            ec_slave_request_state(slave, EC_SLAVE_STATE_PREOP);
        else if (!strcmp(buffer, "SAFEOP\n"))
            ec_slave_request_state(slave, EC_SLAVE_STATE_SAFEOP);
        else if (!strcmp(buffer, "OP\n"))
            ec_slave_request_state(slave, EC_SLAVE_STATE_OP);
        else {
            EC_ERR("Invalid slave state \"%s\"!\n", buffer);
            return -EINVAL;
        }

        ec_state_string(slave->requested_state, state);
        EC_INFO("Accepted new state %s for slave %u.\n",
                state, slave->ring_position);
        return size;
    }
    else if (attr == &attr_sii) {
        return ec_slave_write_sii(slave, buffer, size);
    }
    else if (attr == &attr_alias) {
        return ec_slave_write_alias(slave, buffer, size);
    }

    return -EIO;
}

/*****************************************************************************/

/**
 * Get the sync manager for either Rx- or Tx-Pdos.
 * \return pointer to sync manager, or NULL.
 */

ec_sync_t *ec_slave_get_pdo_sync(
        ec_slave_t *slave, /**< EtherCAT slave */
        ec_direction_t dir /**< input or output */
        )
{
    unsigned int sync_index;

    if (dir != EC_DIR_INPUT && dir != EC_DIR_OUTPUT) {
        EC_ERR("Invalid direction!\n");
        return NULL;
    }

    sync_index = (unsigned int) dir;
    if (slave->sii.mailbox_protocols) sync_index += 2;

    if (sync_index >= slave->sii.sync_count)
        return NULL;

    return &slave->sii.syncs[sync_index];
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
    if (vendor_id != slave->sii.vendor_id ||
        product_code != slave->sii.product_code) {
        EC_ERR("Invalid slave type at position %u:\n", slave->ring_position);
        EC_ERR("  Requested: 0x%08X 0x%08X\n", vendor_id, product_code);
        EC_ERR("      Found: 0x%08X 0x%08X\n",
                slave->sii.vendor_id, slave->sii.product_code);
        return -1;
    }
    return 0;
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
        ec_slave_t *slave /**< EtherCAT slave */,
        uint16_t index /**< Sdo index */
        )
{
    ec_sdo_t *sdo;

    list_for_each_entry(sdo, &slave->sdo_dictionary, list) {
        if (sdo->index != index) continue;
        return sdo;
    }

    return NULL;
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
