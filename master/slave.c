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
void ec_slave_sdos_clear(struct kobject *);
ssize_t ec_show_slave_attribute(struct kobject *, struct attribute *, char *);
ssize_t ec_store_slave_attribute(struct kobject *, struct attribute *,
                                 const char *, size_t);
char *ec_slave_sii_string(ec_slave_t *, unsigned int);

/*****************************************************************************/

/** \cond */

EC_SYSFS_READ_ATTR(info);
EC_SYSFS_READ_WRITE_ATTR(state);
EC_SYSFS_READ_WRITE_ATTR(eeprom);
EC_SYSFS_READ_WRITE_ATTR(alias);

static struct attribute *def_attrs[] = {
    &attr_info,
    &attr_state,
    &attr_eeprom,
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

    slave->requested_state = EC_SLAVE_STATE_PREOP;
    slave->current_state = EC_SLAVE_STATE_UNKNOWN;
    slave->self_configured = 0;
    slave->error_flag = 0;
    slave->online_state = EC_SLAVE_ONLINE;
    slave->fmmu_count = 0;
    slave->pdos_registered = 0;

    slave->base_type = 0;
    slave->base_revision = 0;
    slave->base_build = 0;
    slave->base_fmmu_count = 0;

    slave->eeprom_data = NULL;
    slave->eeprom_size = 0;

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
    slave->sii_current_on_ebus = 0;

    slave->sii_strings = NULL;
    slave->sii_string_count = 0;
    slave->sii_syncs = NULL;
    slave->sii_sync_count = 0;
    INIT_LIST_HEAD(&slave->sii_pdos);
    INIT_LIST_HEAD(&slave->sdo_dictionary);
    INIT_LIST_HEAD(&slave->sdo_confs);

    slave->sdo_dictionary_fetched = 0;
    slave->jiffies_preop = 0;

    for (i = 0; i < 4; i++) {
        slave->dl_link[i] = 0;
        slave->dl_loop[i] = 0;
        slave->dl_signal[i] = 0;
        slave->sii_physical_layer[i] = 0xFF;
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

    // init SDO kobject and add it to the hierarchy
    memset(&slave->sdo_kobj, 0x00, sizeof(struct kobject));
    kobject_init(&slave->sdo_kobj);
    slave->sdo_kobj.ktype = &ktype_ec_slave_sdos;
    slave->sdo_kobj.parent = &slave->kobj;
    if (kobject_set_name(&slave->sdo_kobj, "sdos")) {
        EC_ERR("Failed to set kobject name.\n");
        goto out_sdo_put;
    }
    if (kobject_add(&slave->sdo_kobj)) {
        EC_ERR("Failed to add SDOs kobject.\n");
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

    // free all SDOs
    list_for_each_entry_safe(sdo, next_sdo, &slave->sdo_dictionary, list) {
        list_del(&sdo->list);
        ec_sdo_destroy(sdo);
    }

    // free SDO kobject
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
    ec_sdo_data_t *sdodata, *next_sdodata;
    unsigned int i;

    slave = container_of(kobj, ec_slave_t, kobj);

    // free all strings
    if (slave->sii_strings) {
        for (i = 0; i < slave->sii_string_count; i++)
            kfree(slave->sii_strings[i]);
        kfree(slave->sii_strings);
    }

    // free all sync managers
    if (slave->sii_syncs) {
        for (i = 0; i < slave->sii_sync_count; i++) {
            ec_sync_clear(&slave->sii_syncs[i]);
        }
        kfree(slave->sii_syncs);
    }

    // free all SII PDOs
    list_for_each_entry_safe(pdo, next_pdo, &slave->sii_pdos, list) {
        list_del(&pdo->list);
        ec_pdo_clear(pdo);
        kfree(pdo);
    }

    // free all SDO configurations
    list_for_each_entry_safe(sdodata, next_sdodata, &slave->sdo_confs, list) {
        list_del(&sdodata->list);
        kfree(sdodata->data);
        kfree(sdodata);
    }

    if (slave->eeprom_data) kfree(slave->eeprom_data);

    kfree(slave);
}

/*****************************************************************************/

/**
*/

void ec_slave_sdos_clear(struct kobject *kobj /**< kobject for SDOs */)
{
}

/*****************************************************************************/

/**
   Reset slave from operation mode.
*/

void ec_slave_reset(ec_slave_t *slave /**< EtherCAT slave */)
{
    ec_sdo_data_t *sdodata, *next_sdodata;
    unsigned int i;

    slave->fmmu_count = 0;
    slave->pdos_registered = 0;

    // free all SDO configurations
    list_for_each_entry_safe(sdodata, next_sdodata, &slave->sdo_confs, list) {
        list_del(&sdodata->list);
        kfree(sdodata->data);
        kfree(sdodata);
    }

    // remove estimated sync manager sizes
    for (i = 0; i < slave->sii_sync_count; i++) {
        slave->sii_syncs[i].est_length = 0;
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
            EC_DBG("Slave %i: %s -> %s.\n",
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
        if (slave->pdos_registered)
            slave->master->pdo_slaves_offline++;
        if (slave->master->debug_level)
            EC_DBG("Slave %i: offline.\n", slave->ring_position);
    }
    else if (new_state == EC_SLAVE_ONLINE &&
            slave->online_state == EC_SLAVE_OFFLINE) {
        slave->error_flag = 0; // clear error flag
        if (slave->pdos_registered)
            slave->master->pdo_slaves_offline--;
        if (slave->master->debug_level) {
            char cur_state[EC_STATE_STRING_SIZE];
            ec_state_string(slave->current_state, cur_state);
            EC_DBG("Slave %i: online (%s).\n",
                   slave->ring_position, cur_state);
        }
    }

    slave->online_state = new_state;
}

/*****************************************************************************/

/**
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
   \return 0 in case of success, else < 0
*/

int ec_slave_fetch_sii_strings(
        ec_slave_t *slave, /**< EtherCAT slave */
        const uint8_t *data /**< category data */
        )
{
    int i;
    size_t size;
    off_t offset;

    slave->sii_string_count = data[0];

    if (!slave->sii_string_count)
        return 0;

    if (!(slave->sii_strings =
                kmalloc(sizeof(char *) * slave->sii_string_count,
                    GFP_KERNEL))) {
        EC_ERR("Failed to allocate string array memory.\n");
        goto out_zero;
    }

    offset = 1;
    for (i = 0; i < slave->sii_string_count; i++) {
        size = data[offset];
        // allocate memory for string structure and data at a single blow
        if (!(slave->sii_strings[i] =
                    kmalloc(sizeof(char) * size + 1, GFP_KERNEL))) {
            EC_ERR("Failed to allocate string memory.\n");
            goto out_free;
        }
        memcpy(slave->sii_strings[i], data + offset + 1, size);
        slave->sii_strings[i][size] = 0x00; // append binary zero
        offset += 1 + size;
    }

    return 0;

out_free:
    for (i--; i >= 0; i--) kfree(slave->sii_strings[i]);
    kfree(slave->sii_strings);
    slave->sii_strings = NULL;
out_zero:
    slave->sii_string_count = 0;
    return -1;
}

/*****************************************************************************/

/**
   Fetches data from a GENERAL category.
   \return 0 in case of success, else < 0
*/

void ec_slave_fetch_sii_general(
        ec_slave_t *slave, /**< EtherCAT slave */
        const uint8_t *data /**< category data */
        )
{
    unsigned int i;

    slave->sii_group = ec_slave_sii_string(slave, data[0]);
    slave->sii_image = ec_slave_sii_string(slave, data[1]);
    slave->sii_order = ec_slave_sii_string(slave, data[2]);
    slave->sii_name = ec_slave_sii_string(slave, data[3]);

    for (i = 0; i < 4; i++)
        slave->sii_physical_layer[i] =
            (data[4] & (0x03 << (i * 2))) >> (i * 2);

    slave->sii_current_on_ebus = EC_READ_S16(data + 0x0C);
}

/*****************************************************************************/

/**
   Fetches data from a SYNC MANAGER category.
   \return 0 in case of success, else < 0
*/

int ec_slave_fetch_sii_syncs(
        ec_slave_t *slave, /**< EtherCAT slave */
        const uint8_t *data, /**< category data */
        size_t word_count /**< number of words */
        )
{
    unsigned int i;
    ec_sync_t *sync;

    // sync manager struct is 4 words long
    slave->sii_sync_count = word_count / 4;

    if (!(slave->sii_syncs =
                kmalloc(sizeof(ec_sync_t) * slave->sii_sync_count,
                    GFP_KERNEL))) {
        EC_ERR("Failed to allocate memory for sync managers.\n");
        slave->sii_sync_count = 0;
        return -1;
    }
    
    for (i = 0; i < slave->sii_sync_count; i++, data += 8) {
        sync = &slave->sii_syncs[i];

        ec_sync_init(sync, slave, i);
        sync->physical_start_address = EC_READ_U16(data);
        sync->length = EC_READ_U16(data + 2);
        sync->control_register = EC_READ_U8 (data + 4);
        sync->enable = EC_READ_U8 (data + 6);
    }

    return 0;
}

/*****************************************************************************/

/**
   Fetches data from a [RT]XPDO category.
   \return 0 in case of success, else < 0
*/

int ec_slave_fetch_sii_pdos(
        ec_slave_t *slave, /**< EtherCAT slave */
        const uint8_t *data, /**< category data */
        size_t word_count, /**< number of words */
        ec_pdo_type_t pdo_type /**< PDO type */
        )
{
    ec_pdo_t *pdo;
    ec_pdo_entry_t *entry;
    unsigned int entry_count, i;

    while (word_count >= 4) {
        if (!(pdo = kmalloc(sizeof(ec_pdo_t), GFP_KERNEL))) {
            EC_ERR("Failed to allocate PDO memory.\n");
            return -1;
        }

        ec_pdo_init(pdo);
        pdo->type = pdo_type;
        pdo->index = EC_READ_U16(data);
        entry_count = EC_READ_U8(data + 2);
        pdo->sync_index = EC_READ_U8(data + 3);
        pdo->name = ec_slave_sii_string(slave, EC_READ_U8(data + 5));
        list_add_tail(&pdo->list, &slave->sii_pdos);

        word_count -= 4;
        data += 8;

        for (i = 0; i < entry_count; i++) {
            if (!(entry = kmalloc(sizeof(ec_pdo_entry_t), GFP_KERNEL))) {
                EC_ERR("Failed to allocate PDO entry memory.\n");
                return -1;
            }

            entry->index = EC_READ_U16(data);
            entry->subindex = EC_READ_U8(data + 2);
            entry->name = ec_slave_sii_string(slave, EC_READ_U8(data + 3));
            entry->bit_length = EC_READ_U8(data + 5);
            list_add_tail(&entry->list, &pdo->entries);

            word_count -= 4;
            data += 8;
        }

        // if sync manager index is positive, the PDO is mapped by default
        if (pdo->sync_index >= 0) {
            ec_pdo_t *mapped_pdo;

            if (pdo->sync_index >= slave->sii_sync_count) {
                EC_ERR("Invalid SM index %i for PDO 0x%04X in slave %u.",
                        pdo->sync_index, pdo->index, slave->ring_position);
                return -1;
            }

            if (!(mapped_pdo = kmalloc(sizeof(ec_pdo_t), GFP_KERNEL))) {
                EC_ERR("Failed to allocate PDO memory.\n");
                return -1;
            }

            if (ec_pdo_copy(mapped_pdo, pdo)) {
                EC_ERR("Failed to copy PDO.\n");
                kfree(mapped_pdo);
                return -1;
            }

            list_add_tail(&mapped_pdo->list,
                    &slave->sii_syncs[pdo->sync_index].pdos);
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

char *ec_slave_sii_string(
        ec_slave_t *slave, /**< EtherCAT slave */
        unsigned int index /**< string index */
        )
{
    if (!index--) 
        return NULL;

    if (index >= slave->sii_string_count) {
        if (slave->master->debug_level)
            EC_WARN("String %i not found in slave %i.\n",
                    index, slave->ring_position);
        return NULL;
    }

    return slave->sii_strings[index];
}

/*****************************************************************************/

/**
 * Prepares an FMMU configuration.
 * Configuration data for the FMMU is saved in the slave structure and is
 * written to the slave in ecrt_master_activate().
 * The FMMU configuration is done in a way, that the complete data range
 * of the corresponding sync manager is covered. Seperate FMMUs are configured
 * for each domain.
 * If the FMMU configuration is already prepared, the function returns with
 * success.
 * \return 0 in case of success, else < 0
 */

int ec_slave_prepare_fmmu(
        ec_slave_t *slave, /**< EtherCAT slave */
        const ec_domain_t *domain, /**< domain */
        const ec_sync_t *sync  /**< sync manager */
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

    ec_fmmu_init(fmmu, slave, slave->fmmu_count++);
    fmmu->domain = domain;
    fmmu->sync = sync;
    fmmu->logical_start_address = 0;

    slave->pdos_registered = 1;
    
    ec_slave_request_state(slave, EC_SLAVE_STATE_OP);

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
    ec_sync_t *sync;
    ec_pdo_t *pdo;
    ec_pdo_entry_t *pdo_entry;
    int first, i;
    ec_sdo_data_t *sdodata;
    char str[20];

    off += sprintf(buffer + off, "\nName: ");

    if (slave->sii_name)
        off += sprintf(buffer + off, "%s", slave->sii_name);

    off += sprintf(buffer + off, "\nVendor ID: 0x%08X\n",
                   slave->sii_vendor_id);
    off += sprintf(buffer + off, "Product code: 0x%08X\n\n",
                   slave->sii_product_code);

    off += sprintf(buffer + off, "State: ");
    off += ec_state_string(slave->current_state, buffer + off);
    off += sprintf(buffer + off, " (");
    off += ec_state_string(slave->requested_state, buffer + off);
    off += sprintf(buffer + off, ")\nFlags: %s, %s\n",
            slave->online_state == EC_SLAVE_ONLINE ? "online" : "OFFLINE",
            slave->error_flag ? "ERROR" : "ok");
    off += sprintf(buffer + off, "Ring position: %i\n",
                   slave->ring_position);
    off += sprintf(buffer + off, "Current consumption: %i mA\n\n",
                   slave->sii_current_on_ebus);

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

    if (slave->sii_sync_count)
        off += sprintf(buffer + off, "\nSync managers / PDO mapping:\n");

    for (i = 0; i < slave->sii_sync_count; i++) {
        sync = &slave->sii_syncs[i];
        off += sprintf(buffer + off,
                "  SM%u: addr 0x%04X, size %i, control 0x%02X, %s\n",
                sync->index, sync->physical_start_address,
                ec_sync_size(sync), sync->control_register,
                sync->enable ? "enable" : "disable");

        if (list_empty(&sync->pdos))
            off += sprintf(buffer + off, "    No PDOs mapped.\n");

        list_for_each_entry(pdo, &sync->pdos, list) {
            off += sprintf(buffer + off, "    %s 0x%04X \"%s\"\n",
                    pdo->type == EC_RX_PDO ? "RXPDO" : "TXPDO",
                    pdo->index, pdo->name ? pdo->name : "???");

            list_for_each_entry(pdo_entry, &pdo->entries, list) {
                off += sprintf(buffer + off,
                        "      0x%04X:%X \"%s\", %i bit\n",
                        pdo_entry->index, pdo_entry->subindex,
                        pdo_entry->name ? pdo_entry->name : "???",
                        pdo_entry->bit_length);
            }
        }
    }

    // type-cast to avoid warnings on some compilers
    if (!list_empty((struct list_head *) &slave->sii_pdos))
        off += sprintf(buffer + off, "\nAvailable PDOs:\n");

    list_for_each_entry(pdo, &slave->sii_pdos, list) {
        off += sprintf(buffer + off, "  %s 0x%04X \"%s\"",
                       pdo->type == EC_RX_PDO ? "RXPDO" : "TXPDO",
                       pdo->index, pdo->name ? pdo->name : "???");
        if (pdo->sync_index >= 0)
            off += sprintf(buffer + off, ", default mapping: SM%u.\n",
                    pdo->sync_index);
        else
            off += sprintf(buffer + off, ", no default mapping.\n");

        list_for_each_entry(pdo_entry, &pdo->entries, list) {
            off += sprintf(buffer + off, "    0x%04X:%X \"%s\", %i bit\n",
                           pdo_entry->index, pdo_entry->subindex,
                           pdo_entry->name ? pdo_entry->name : "???",
                           pdo_entry->bit_length);
        }
    }

    // type-cast to avoid warnings on some compilers
    if (!list_empty((struct list_head *) &slave->sdo_confs))
        off += sprintf(buffer + off, "\nSDO configurations:\n");

    list_for_each_entry(sdodata, &slave->sdo_confs, list) {
        switch (sdodata->size) {
            case 1: sprintf(str, "%i", EC_READ_U8(sdodata->data)); break;
            case 2: sprintf(str, "%i", EC_READ_U16(sdodata->data)); break;
            case 4: sprintf(str, "%i", EC_READ_U32(sdodata->data)); break;
            default: sprintf(str, "(invalid size)"); break;
        }
        off += sprintf(buffer + off, "  0x%04X:%-3i -> %s\n",
                       sdodata->index, sdodata->subindex, str);
    }

    off += sprintf(buffer + off, "\n");

    return off;
}

/*****************************************************************************/

/**
 * Schedules an EEPROM write request.
 * \return 0 case of success, otherwise error code.
 */

int ec_slave_schedule_eeprom_writing(ec_eeprom_write_request_t *request)
{
    ec_master_t *master = request->slave->master;

    request->state = EC_REQUEST_QUEUED;

    // schedule EEPROM write request.
    down(&master->eeprom_sem);
    list_add_tail(&request->list, &master->eeprom_requests);
    up(&master->eeprom_sem);

    // wait for processing through FSM
    if (wait_event_interruptible(master->eeprom_queue,
                request->state != EC_REQUEST_QUEUED)) {
        // interrupted by signal
        down(&master->eeprom_sem);
        if (request->state == EC_REQUEST_QUEUED) {
            list_del(&request->list);
            up(&master->eeprom_sem);
            return -EINTR;
        }
        // request already processing: interrupt not possible.
        up(&master->eeprom_sem);
    }

    // wait until master FSM has finished processing
    wait_event(master->eeprom_queue,
            request->state != EC_REQUEST_IN_PROGRESS);

    return request->state == EC_REQUEST_COMPLETE ? 0 : -EIO;
}

/*****************************************************************************/

/**
 * Writes complete EEPROM contents to a slave.
 * \return data size written in case of success, otherwise error code.
 */

ssize_t ec_slave_write_eeprom(ec_slave_t *slave, /**< EtherCAT slave */
        const uint8_t *data, /**< new EEPROM data */
        size_t size /**< size of data in bytes */
        )
{
    ec_eeprom_write_request_t request;
    const uint16_t *cat_header;
    uint16_t cat_type, cat_size;
    int ret;

    if (slave->master->mode != EC_MASTER_MODE_IDLE) { // FIXME
        EC_ERR("Writing EEPROMs only allowed in idle mode!\n");
        return -EBUSY;
    }

    if (size % 2) {
        EC_ERR("EEPROM data size is odd! Dropping.\n");
        return -EINVAL;
    }

    // init EEPROM write request
    INIT_LIST_HEAD(&request.list);
    request.slave = slave;
    request.words = (const uint16_t *) data;
    request.offset = 0;
    request.size = size / 2;

    if (request.size < 0x0041) {
        EC_ERR("EEPROM data too short! Dropping.\n");
        return -EINVAL;
    }

    cat_header = request.words + EC_FIRST_EEPROM_CATEGORY_OFFSET;
    cat_type = EC_READ_U16(cat_header);
    while (cat_type != 0xFFFF) { // cycle through categories
        if (cat_header + 1 > request.words + request.size) {
            EC_ERR("EEPROM data corrupted! Dropping.\n");
            return -EINVAL;
        }
        cat_size = EC_READ_U16(cat_header + 1);
        if (cat_header + cat_size + 2 > request.words + request.size) {
            EC_ERR("EEPROM data corrupted! Dropping.\n");
            return -EINVAL;
        }
        cat_header += cat_size + 2;
        cat_type = EC_READ_U16(cat_header);
    }

    // EEPROM data ok. schedule writing.
    if ((ret = ec_slave_schedule_eeprom_writing(&request)))
        return ret; // error code

    return size; // success
}

/*****************************************************************************/

/**
 * Writes the Secondary slave address (alias) to the slave's EEPROM.
 * \return data size written in case of success, otherwise error code.
 */

ssize_t ec_slave_write_alias(ec_slave_t *slave, /**< EtherCAT slave */
        const uint8_t *data, /**< alias string */
        size_t size /**< size of data in bytes */
        )
{
    ec_eeprom_write_request_t request;
    char *remainder;
    uint16_t alias, word;
    int ret;

    if (slave->master->mode != EC_MASTER_MODE_IDLE) { // FIXME
        EC_ERR("Writing EEPROMs only allowed in idle mode!\n");
        return -EBUSY;
    }

    alias = simple_strtoul(data, &remainder, 0);
    if (remainder == (char *) data || (*remainder && *remainder != '\n')) {
        EC_ERR("Invalid alias value! Dropping.\n");
        return -EINVAL;
    }
    
    // correct endianess
    EC_WRITE_U16(&word, alias);

    // init EEPROM write request
    INIT_LIST_HEAD(&request.list);
    request.slave = slave;
    request.words = &word;
    request.offset = 0x0004;
    request.size = 1;

    if ((ret = ec_slave_schedule_eeprom_writing(&request)))
        return ret; // error code

    slave->sii_alias = alias; // FIXME: do this in state machine

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
    else if (attr == &attr_alias) {
        return sprintf(buffer, "%u\n", slave->sii_alias);
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
        else if (!strcmp(buffer, "SAVEOP\n"))
            ec_slave_request_state(slave, EC_SLAVE_STATE_SAVEOP);
        else if (!strcmp(buffer, "OP\n"))
            ec_slave_request_state(slave, EC_SLAVE_STATE_OP);
        else {
            EC_ERR("Invalid slave state \"%s\"!\n", buffer);
            return -EINVAL;
        }

        ec_state_string(slave->requested_state, state);
        EC_INFO("Accepted new state %s for slave %i.\n",
                state, slave->ring_position);
        return size;
    }
    else if (attr == &attr_eeprom) {
        return ec_slave_write_eeprom(slave, buffer, size);
    }
    else if (attr == &attr_alias) {
        return ec_slave_write_alias(slave, buffer, size);
    }

    return -EIO;
}

/*****************************************************************************/

/**
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
    if (slave->sii_mailbox_protocols) sync_index += 2;

    if (sync_index >= slave->sii_sync_count)
        return NULL;

    return &slave->sii_syncs[sync_index];
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
        EC_ERR("Invalid slave type at position %i:\n", slave->ring_position);
        EC_ERR("  Requested: 0x%08X 0x%08X\n", vendor_id, product_code);
        EC_ERR("      Found: 0x%08X 0x%08X\n",
                slave->sii_vendor_id, slave->sii_product_code);
        return -1;
    }
    return 0;
}

/*****************************************************************************/

/**
   Counts the total number of SDOs and entries in the dictionary.
*/

void ec_slave_sdo_dict_info(const ec_slave_t *slave, /**< EtherCAT slave */
                            unsigned int *sdo_count, /**< number of SDOs */
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

void ecrt_slave_pdo_mapping_clear(
        ec_slave_t *slave, /**< EtherCAT slave */
        ec_direction_t dir /**< output/input */
        )
{
    ec_sync_t *sync;

    if (!(slave->sii_mailbox_protocols & EC_MBOX_COE)) {
        EC_ERR("Slave %i does not support CoE!\n", slave->ring_position);
        return;
    }

    if (!(sync = ec_slave_get_pdo_sync(slave, dir)))
        return;

    ec_sync_clear_pdos(sync);
}

/*****************************************************************************/

int ecrt_slave_pdo_mapping_add(
        ec_slave_t *slave, /**< EtherCAT slave */
        ec_direction_t dir, /**< input/output */
        uint16_t pdo_index /**< Index of PDO mapping list */)
{
    ec_pdo_t *pdo;
    ec_sync_t *sync;
    unsigned int not_found = 1;

    if (!(slave->sii_mailbox_protocols & EC_MBOX_COE)) {
        EC_ERR("Slave %u does not support CoE!\n", slave->ring_position);
        return -1;
    }

    // does the slave provide the PDO list?
    list_for_each_entry(pdo, &slave->sii_pdos, list) {
        if (pdo->index == pdo_index) {
            not_found = 0;
            break;
        }
    }

    if (not_found) {
        EC_ERR("Slave %u does not provide PDO 0x%04X!\n",
                slave->ring_position, pdo_index);
        return -1;
    }

    // check direction
    if ((pdo->type == EC_TX_PDO && dir == EC_DIR_OUTPUT) ||
            (pdo->type == EC_RX_PDO && dir == EC_DIR_INPUT)) {
        EC_ERR("Invalid direction for PDO 0x%04X.\n", pdo_index);
        return -1;
    }


    if (!(sync = ec_slave_get_pdo_sync(slave, dir))) {
        EC_ERR("Failed to obtain sync manager for PDO mapping of slave %u!\n",
                slave->ring_position);
        return -1;
    }

    return ec_sync_add_pdo(sync, pdo);
}

/*****************************************************************************/

int ecrt_slave_pdo_mapping(ec_slave_t *slave, /**< EtherCAT slave */
        ec_direction_t dir, /**< input/output */
        unsigned int num_args, /**< Number of following arguments */
        ... /**< PDO indices to map */
        )
{
    va_list ap;

    ecrt_slave_pdo_mapping_clear(slave, dir);

    va_start(ap, num_args);

    for (; num_args; num_args--) {
        if (ecrt_slave_pdo_mapping_add(
                    slave, dir, (uint16_t) va_arg(ap, int))) {
            return -1;
        }
    }

    va_end(ap);
    return 0;
}


/*****************************************************************************/

/** \cond */

EXPORT_SYMBOL(ecrt_slave_conf_sdo8);
EXPORT_SYMBOL(ecrt_slave_conf_sdo16);
EXPORT_SYMBOL(ecrt_slave_conf_sdo32);
EXPORT_SYMBOL(ecrt_slave_pdo_mapping_clear);
EXPORT_SYMBOL(ecrt_slave_pdo_mapping_add);
EXPORT_SYMBOL(ecrt_slave_pdo_mapping);

/** \endcond */

/*****************************************************************************/
