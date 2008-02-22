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
   EtherCAT slave configuration methods.
*/

/*****************************************************************************/

#include <linux/module.h>

#include "globals.h"
#include "master.h"

#include "slave_config.h"

/*****************************************************************************/

void ec_slave_config_clear(struct kobject *);
ssize_t ec_show_slave_config_attribute(struct kobject *, struct attribute *,
        char *);

/*****************************************************************************/

/** \cond */

EC_SYSFS_READ_ATTR(info);

static struct attribute *def_attrs[] = {
    &attr_info,
    NULL,
};

static struct sysfs_ops sysfs_ops = {
    .show = ec_show_slave_config_attribute
};

static struct kobj_type ktype_ec_slave_config = {
    .release = ec_slave_config_clear,
    .sysfs_ops = &sysfs_ops,
    .default_attrs = def_attrs
};

/** \endcond */

/*****************************************************************************/

/** Slave configuration constructor.
 *
 * See ecrt_master_slave_config() for the usage of the \a alias and \a
 * position parameters.
 *
 * \retval 0 Success.
 * \retval <0 Failure.
 */
int ec_slave_config_init(ec_slave_config_t *sc, /**< Slave configuration. */
        ec_master_t *master, /**< EtherCAT master. */
        uint16_t alias, /**< Slave alias. */
        uint16_t position, /**< Slave position. */
        uint32_t vendor_id, /**< Expected vendor ID. */
        uint32_t product_code /**< Expected product code. */
        )
{
    ec_direction_t dir;

    sc->master = master;
    sc->alias = alias;
    sc->position = position;
    sc->vendor_id = vendor_id;
    sc->product_code = product_code;
    sc->slave = NULL;

    for (dir = EC_DIR_OUTPUT; dir <= EC_DIR_INPUT; dir++)
        ec_pdo_mapping_init(&sc->mapping[dir]);

    INIT_LIST_HEAD(&sc->sdo_configs);

    sc->used_fmmus = 0;

    // init kobject and add it to the hierarchy
    memset(&sc->kobj, 0x00, sizeof(struct kobject));
    kobject_init(&sc->kobj);
    sc->kobj.ktype = &ktype_ec_slave_config;
    sc->kobj.parent = &master->kobj;
    if (kobject_set_name(&sc->kobj, "config-%u-%u", sc->alias, sc->position)) {
        EC_ERR("Failed to set kobject name for slave config %u:%u.\n",
                sc->alias, sc->position);
        goto out_put;
    }
    if (kobject_add(&sc->kobj)) {
        EC_ERR("Failed to add kobject for slave config %u:%u.\n",
                sc->alias, sc->position);
        goto out_put;
    }

    return 0;

 out_put:
    kobject_put(&sc->kobj);
    return -1;
}

/*****************************************************************************/

/** Slave configuration destructor.
 *
 * Clears and frees a slave configuration object.
 */
void ec_slave_config_destroy(
        ec_slave_config_t *sc /**< Slave configuration. */
        )
{
    ec_slave_config_detach(sc);

    // destroy self
    kobject_del(&sc->kobj);
    kobject_put(&sc->kobj);
}

/*****************************************************************************/

/** Clear and free the slave configuration.
 * 
 * This method is called by the kobject, once there are no more references to
 * it.
 */
void ec_slave_config_clear(struct kobject *kobj /**< kobject of the config. */)
{
    ec_slave_config_t *sc;
    ec_direction_t dir;
    ec_sdo_data_t *sdodata, *next_sdodata;

    sc = container_of(kobj, ec_slave_config_t, kobj);

    // Free Pdo mappings
    for (dir = EC_DIR_OUTPUT; dir <= EC_DIR_INPUT; dir++)
        ec_pdo_mapping_clear(&sc->mapping[dir]);

    // free all Sdo configurations
    list_for_each_entry_safe(sdodata, next_sdodata, &sc->sdo_configs, list) {
        list_del(&sdodata->list);
        kfree(sdodata->data);
        kfree(sdodata);
    }

    /** \todo */

    kfree(sc);
}

/*****************************************************************************/

/** Prepares an FMMU configuration.
 *
 * Configuration data for the FMMU is saved in the slave config structure and
 * is written to the slave during the configuration. The FMMU configuration
 * is done in a way, that the complete data range of the corresponding sync
 * manager is covered. Seperate FMMUs are configured for each domain. If the
 * FMMU configuration is already prepared, the function returns with success.
 *
 * \retval >=0 Logical offset address.
 * \retval -1  FMMU limit reached.
 */
int ec_slave_config_prepare_fmmu(
        ec_slave_config_t *sc, /**< Slave configuration. */
        ec_domain_t *domain, /**< Domain. */
        ec_direction_t dir /**< Pdo direction. */
        )
{
    unsigned int i;
    ec_fmmu_config_t *fmmu;

    // FMMU configuration already prepared?
    for (i = 0; i < sc->used_fmmus; i++) {
        fmmu = &sc->fmmu_configs[i];
        if (fmmu->domain == domain && fmmu->dir == dir)
            return fmmu->logical_start_address;
    }

    if (sc->used_fmmus == EC_MAX_FMMUS) {
        EC_ERR("FMMU limit reached for slave configuration %u:%u!\n",
                sc->alias, sc->position);
        return -1;
    }

    fmmu = &sc->fmmu_configs[sc->used_fmmus++];
    ec_fmmu_config_init(fmmu, sc, domain, dir);
    return fmmu->logical_start_address;
}

/*****************************************************************************/

/** Outputs all information about a certain slave configuration.
*/
ssize_t ec_slave_config_info(
        const ec_slave_config_t *sc, /**< Slave configuration. */
        char *buffer /**< Output buffer */
        )
{
    char *buf = buffer;
    ec_direction_t dir;
    const ec_pdo_mapping_t *map;
    const ec_pdo_t *pdo;
    const ec_pdo_entry_t *entry;
    char str[20];
    ec_sdo_data_t *sdodata;

    buf += sprintf(buf, "Alias: 0x%04X (%u)\n", sc->alias, sc->alias);
    buf += sprintf(buf, "Position: %u\n", sc->position);

    for (dir = EC_DIR_OUTPUT; dir <= EC_DIR_INPUT; dir++) {
        map = &sc->mapping[dir];
        
        if (!list_empty(&map->pdos)) {
            buf += sprintf(buf, "%s mapping:\n",
                    dir == EC_DIR_OUTPUT ? "Output" : "Input");

            list_for_each_entry(pdo, &map->pdos, list) {
                buf += sprintf(buf, "  %s 0x%04X \"%s\"\n",
                        pdo->dir == EC_DIR_OUTPUT ? "RxPdo" : "TxPdo",
                        pdo->index, pdo->name ? pdo->name : "???");

                list_for_each_entry(entry, &pdo->entries, list) {
                    buf += sprintf(buf, "    0x%04X:%X \"%s\", %u bit\n",
                            entry->index, entry->subindex,
                            entry->name ? entry->name : "???",
                            entry->bit_length);
                }
            }
        }
    }
    
    // type-cast to avoid warnings on some compilers
    if (!list_empty((struct list_head *) &sc->sdo_configs)) {
        buf += sprintf(buf, "\nSdo configurations:\n");

        list_for_each_entry(sdodata, &sc->sdo_configs, list) {
            switch (sdodata->size) {
                case 1: sprintf(str, "%u", EC_READ_U8(sdodata->data)); break;
                case 2: sprintf(str, "%u", EC_READ_U16(sdodata->data)); break;
                case 4: sprintf(str, "%u", EC_READ_U32(sdodata->data)); break;
                default: sprintf(str, "(invalid size)"); break;
            }
            buf += sprintf(buf, "  0x%04X:%-3i -> %s\n",
                    sdodata->index, sdodata->subindex, str);
        }
        buf += sprintf(buf, "\n");
    }

    return buf - buffer;
}

/*****************************************************************************/

/** Formats attribute data for SysFS read access.
 *
 * \return number of bytes to read
 */
ssize_t ec_show_slave_config_attribute(
        struct kobject *kobj, /**< Slave configuration's kobject */
        struct attribute *attr, /**< Requested attribute. */
        char *buffer /**< Memory to store data. */
        )
{
    ec_slave_config_t *sc = container_of(kobj, ec_slave_config_t, kobj);

    if (attr == &attr_info) {
        return ec_slave_config_info(sc, buffer);
    }

    return 0;
}

/*****************************************************************************/

/** Adds an Sdo configuration.
 */
int ec_slave_config_sdo(ec_slave_config_t *sc, uint16_t index,
        uint8_t subindex, const uint8_t *data, size_t size)
{
    ec_slave_t *slave = sc->slave;
    ec_sdo_data_t *sdodata;

    if (slave && !(slave->sii_mailbox_protocols & EC_MBOX_COE)) {
        EC_ERR("Slave %u does not support CoE!\n", slave->ring_position);
        return -1;
    }

    if (!(sdodata = (ec_sdo_data_t *)
          kmalloc(sizeof(ec_sdo_data_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate memory for Sdo configuration object!\n");
        return -1;
    }

    if (!(sdodata->data = (uint8_t *) kmalloc(size, GFP_KERNEL))) {
        EC_ERR("Failed to allocate memory for Sdo configuration data!\n");
        kfree(sdodata);
        return -1;
    }

    sdodata->index = index;
    sdodata->subindex = subindex;
    memcpy(sdodata->data, data, size);
    sdodata->size = size;

    list_add_tail(&sdodata->list, &sc->sdo_configs);
    return 0;
}

/*****************************************************************************/

/** Attaches the configuration to the addressed slave object.
 *
 * \retval 0 Success.
 * \retval -1 Slave not found.
 * \retval -2 Slave already configured.
 * \retval -3 Invalid slave type found at the given position.
 */
int ec_slave_config_attach(
        ec_slave_config_t *sc /**< Slave configuration. */
        )
{
	ec_slave_t *slave;
	unsigned int alias_found = 0, relative_position = 0;

	if (sc->slave)
		return 0; // already attached

	list_for_each_entry(slave, &sc->master->slaves, list) {
		if (!alias_found) {
			if (sc->alias && slave->sii_alias != sc->alias)
				continue;
			alias_found = 1;
			relative_position = 0;
		}
		if (relative_position == sc->position)
			goto found;
		relative_position++;
	}

	EC_WARN("Failed to find slave for configuration %u:%u.\n",
			sc->alias, sc->position);
	return -1;

found:
	if (slave->config) {
		EC_ERR("Failed to attach slave configuration %u:%u. Slave %u"
				" already has a configuration!\n", sc->alias,
				sc->position, slave->ring_position);
		return -2;
	}
	if (slave->sii_vendor_id != sc->vendor_id
			|| slave->sii_product_code != sc->product_code) {
		EC_ERR("Slave %u has an invalid type (0x%08X/0x%08X) for"
				" configuration %u:%u (0x%08X/0x%08X).\n",
				slave->ring_position, slave->sii_vendor_id,
				slave->sii_product_code, sc->alias, sc->position,
				sc->vendor_id, sc->product_code);
		return -3;
	}

	// attach slave
	slave->config = sc;
	sc->slave = slave;

    ec_slave_request_state(slave, EC_SLAVE_STATE_OP);

	return 0;
}

/*****************************************************************************/

/** Detaches the configuration from a slave object.
 */
void ec_slave_config_detach(
        ec_slave_config_t *sc /**< Slave configuration. */
        )
{
    if (sc->slave) {
        sc->slave->config = NULL;
        sc->slave = NULL;
    }
}

/*****************************************************************************/

/** Loads the default mapping from the slave object.
 */
void ec_slave_config_load_default_mapping(ec_slave_config_t *sc)
{
    ec_direction_t dir;
    ec_pdo_mapping_t *map;
    ec_sync_t *sync;

    if (!sc->slave)
        return;
    
    for (dir = EC_DIR_OUTPUT; dir <= EC_DIR_INPUT; dir++) {
        map = &sc->mapping[dir];
        if (!(sync = ec_slave_get_pdo_sync(sc->slave, dir)))
            continue;
        ec_pdo_mapping_copy(map, &sync->mapping);
    }
}

/******************************************************************************
 *  Realtime interface
 *****************************************************************************/

int ecrt_slave_config_mapping(ec_slave_config_t *sc, unsigned int n_entries,
        const ec_pdo_info_t pdo_infos[])
{
    unsigned int i;

    for (i = 0; i < n_entries; i++)
        if (ec_pdo_mapping_add_pdo_info(&sc->mapping[pdo_infos[i].dir],
                    &pdo_infos[i], sc))
            return -1;

    return 0;
}

/*****************************************************************************/

int ecrt_slave_config_reg_pdo_entry(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint16_t index, /**< Index of Pdo entry to register. */
        uint8_t subindex, /**< Subindex of Pdo entry to register. */
        ec_domain_t *domain /**< Domain. */
        )
{
    ec_direction_t dir;
    ec_pdo_mapping_t *map;
    unsigned int bit_offset, byte_offset;
    ec_pdo_t *pdo;
    ec_pdo_entry_t *entry;
    int ret;

    for (dir = EC_DIR_OUTPUT; dir <= EC_DIR_INPUT; dir++) {
        map = &sc->mapping[dir];
        bit_offset = 0;
        list_for_each_entry(pdo, &map->pdos, list) {
            list_for_each_entry(entry, &pdo->entries, list) {
                if (entry->index != index || entry->subindex != subindex) {
                    bit_offset += entry->bit_length;
                } else {
                    goto found;
                }
            }
        }
    }

    EC_ERR("Pdo entry 0x%04X:%u is not mapped in slave config %u:%u.\n",
           index, subindex, sc->alias, sc->position);
    return -1;

found:
    byte_offset = bit_offset / 8;
    if ((ret = ec_slave_config_prepare_fmmu(sc, domain, dir)) < 0)
        return -2;
    return ret + byte_offset;
}

/*****************************************************************************/

int ecrt_slave_config_sdo8(ec_slave_config_t *slave, uint16_t index,
        uint8_t subindex, uint8_t value)
{
    uint8_t data[1];
    EC_WRITE_U8(data, value);
    return ec_slave_config_sdo(slave, index, subindex, data, 1);
}

/*****************************************************************************/

int ecrt_slave_config_sdo16(ec_slave_config_t *slave, uint16_t index,
        uint8_t subindex, uint16_t value)
{
    uint8_t data[2];
    EC_WRITE_U16(data, value);
    return ec_slave_config_sdo(slave, index, subindex, data, 2);
}

/*****************************************************************************/

int ecrt_slave_config_sdo32(ec_slave_config_t *slave, uint16_t index,
        uint8_t subindex, uint32_t value)
{
    uint8_t data[4];
    EC_WRITE_U32(data, value);
    return ec_slave_config_sdo(slave, index, subindex, data, 4);
}

/*****************************************************************************/

/** \cond */

EXPORT_SYMBOL(ecrt_slave_config_mapping);
EXPORT_SYMBOL(ecrt_slave_config_reg_pdo_entry);
EXPORT_SYMBOL(ecrt_slave_config_sdo8);
EXPORT_SYMBOL(ecrt_slave_config_sdo16);
EXPORT_SYMBOL(ecrt_slave_config_sdo32);

/** \endcond */

/*****************************************************************************/
