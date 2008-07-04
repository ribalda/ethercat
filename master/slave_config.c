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

#include <linux/slab.h>

#include "globals.h"
#include "master.h"

#include "slave_config.h"

/*****************************************************************************/

/** Slave configuration constructor.
 *
 * See ecrt_master_slave_config() for the usage of the \a alias and \a
 * position parameters.
 */
void ec_slave_config_init(
        ec_slave_config_t *sc, /**< Slave configuration. */
        ec_master_t *master, /**< EtherCAT master. */
        uint16_t alias, /**< Slave alias. */
        uint16_t position, /**< Slave position. */
        uint32_t vendor_id, /**< Expected vendor ID. */
        uint32_t product_code /**< Expected product code. */
        )
{
    unsigned int i;

    sc->master = master;
    sc->alias = alias;
    sc->position = position;
    sc->vendor_id = vendor_id;
    sc->product_code = product_code;
    sc->slave = NULL;

    for (i = 0; i < EC_MAX_SYNC_MANAGERS; i++)
        ec_sync_config_init(&sc->sync_configs[i]);

    INIT_LIST_HEAD(&sc->sdo_configs);
    INIT_LIST_HEAD(&sc->sdo_requests);

    sc->used_fmmus = 0;
}

/*****************************************************************************/

/** Slave configuration destructor.
 *
 * Clears and frees a slave configuration object.
 */
void ec_slave_config_clear(
        ec_slave_config_t *sc /**< Slave configuration. */
        )
{
    unsigned int i;
    ec_sdo_request_t *req, *next_req;

    ec_slave_config_detach(sc);

    // Free sync managers
    for (i = 0; i < EC_MAX_SYNC_MANAGERS; i++)
        ec_sync_config_clear(&sc->sync_configs[i]);

    // free all Sdo configurations
    list_for_each_entry_safe(req, next_req, &sc->sdo_configs, list) {
        list_del(&req->list);
        ec_sdo_request_clear(req);
        kfree(req);
    }

    // free all Sdo requests
    list_for_each_entry_safe(req, next_req, &sc->sdo_requests, list) {
        list_del(&req->list);
        ec_sdo_request_clear(req);
        kfree(req);
    }
}

/*****************************************************************************/

/** Prepares an FMMU configuration.
 *
 * Configuration data for the FMMU is saved in the slave config structure and
 * is written to the slave during the configuration. The FMMU configuration
 * is done in a way, that the complete data range of the corresponding sync
 * manager is covered. Seperate FMMUs are configured for each domain. If the
 * FMMU configuration is already prepared, the function does nothing and
 * returns with success.
 *
 * \retval >=0 Success, logical offset byte address.
 * \retval -1  Error, FMMU limit reached.
 */
int ec_slave_config_prepare_fmmu(
        ec_slave_config_t *sc, /**< Slave configuration. */
        ec_domain_t *domain, /**< Domain. */
        uint8_t sync_index, /**< Sync manager index. */
        ec_direction_t dir /**< Pdo direction. */
        )
{
    unsigned int i;
    ec_fmmu_config_t *fmmu;

    // FMMU configuration already prepared?
    for (i = 0; i < sc->used_fmmus; i++) {
        fmmu = &sc->fmmu_configs[i];
        if (fmmu->domain == domain && fmmu->sync_index == sync_index)
            return fmmu->logical_start_address;
    }

    if (sc->used_fmmus == EC_MAX_FMMUS) {
        EC_ERR("FMMU limit reached for slave configuration %u:%u!\n",
                sc->alias, sc->position);
        return -1;
    }

    fmmu = &sc->fmmu_configs[sc->used_fmmus++];

    down(&sc->master->master_sem);
    ec_fmmu_config_init(fmmu, sc, domain, sync_index, dir);
    up(&sc->master->master_sem);

    return fmmu->logical_start_address;
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

	if (sc->slave)
		return 0; // already attached

    if (!(slave = ec_master_find_slave(
                    sc->master, sc->alias, sc->position))) {
        if (sc->master->debug_level)
            EC_DBG("Failed to find slave for configuration %u:%u.\n",
                    sc->alias, sc->position);
        return -1;
    }

	if (slave->config) {
        if (sc->master->debug_level)
            EC_DBG("Failed to attach slave configuration %u:%u. Slave %u"
                    " already has a configuration!\n", sc->alias,
                    sc->position, slave->ring_position);
        return -2;
    }
    if (slave->sii.vendor_id != sc->vendor_id
            || slave->sii.product_code != sc->product_code) {
        if (sc->master->debug_level)
            EC_DBG("Slave %u has an invalid type (0x%08X/0x%08X) for"
                    " configuration %u:%u (0x%08X/0x%08X).\n",
                    slave->ring_position, slave->sii.vendor_id,
                    slave->sii.product_code, sc->alias, sc->position,
                    sc->vendor_id, sc->product_code);
        return -3;
	}

	// attach slave
	slave->config = sc;
	sc->slave = slave;
    ec_slave_request_state(slave, EC_SLAVE_STATE_OP);

    if (sc->master->debug_level)
        EC_DBG("Attached slave %u to config %u:%u.\n",
                slave->ring_position, sc->alias, sc->position);

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

/** Loads the default Pdo assignment from the slave object.
 */
void ec_slave_config_load_default_sync_config(ec_slave_config_t *sc)
{
    uint8_t sync_index;
    ec_sync_config_t *sync_config;
    const ec_sync_t *sync;

    if (!sc->slave)
        return;
    
    for (sync_index = 0; sync_index < EC_MAX_SYNC_MANAGERS; sync_index++) {
        sync_config = &sc->sync_configs[sync_index];
        if ((sync = ec_slave_get_sync(sc->slave, sync_index))) {
            sync_config->dir = ec_sync_default_direction(sync);
            if (sync_config->dir == EC_DIR_INVALID)
                EC_WARN("SM%u of slave %u has an invalid direction field!\n",
                        sync_index, sc->slave->ring_position);
            ec_pdo_list_copy(&sync_config->pdos, &sync->pdos);
        }
    }
}

/*****************************************************************************/

/** Loads the default mapping for a Pdo from the slave object.
 */
void ec_slave_config_load_default_mapping(
        const ec_slave_config_t *sc,
        ec_pdo_t *pdo
        )
{
    unsigned int i;
    const ec_sync_t *sync;
    const ec_pdo_t *default_pdo;

    if (!sc->slave)
        return;

    if (sc->master->debug_level)
        EC_DBG("Loading default mapping for Pdo 0x%04X in config %u:%u.\n",
                pdo->index, sc->alias, sc->position);

    // find Pdo in any sync manager (it could be reassigned later)
    for (i = 0; i < sc->slave->sii.sync_count; i++) {
        sync = &sc->slave->sii.syncs[i];

        list_for_each_entry(default_pdo, &sync->pdos.list, list) {
            if (default_pdo->index != pdo->index)
                continue;

            if (default_pdo->name) {
                if (sc->master->debug_level)
                    EC_DBG("Found Pdo name \"%s\".\n", default_pdo->name);

                // take Pdo name from assigned one
                ec_pdo_set_name(pdo, default_pdo->name);
            }

            // copy entries (= default Pdo mapping)
            if (ec_pdo_copy_entries(pdo, default_pdo))
                return;

            if (sc->master->debug_level) {
                const ec_pdo_entry_t *entry;
                list_for_each_entry(entry, &pdo->entries, list) {
                    EC_DBG("Entry 0x%04X:%02X.\n",
                            entry->index, entry->subindex);
                }
            }

            return;
        }
    }

    if (sc->master->debug_level)
        EC_DBG("No default mapping found.\n");
}

/*****************************************************************************/

/** Get the number of Sdo configurations.
 *
 * \return Number of Sdo configurations.
 */
unsigned int ec_slave_config_sdo_count(
        const ec_slave_config_t *sc /**< Slave configuration. */
        )
{
	const ec_sdo_request_t *req;
	unsigned int count = 0;

	list_for_each_entry(req, &sc->sdo_configs, list) {
		count++;
	}

	return count;
}

/*****************************************************************************/

/** Finds an Sdo configuration via its position in the list.
 *
 * Const version.
 */
const ec_sdo_request_t *ec_slave_config_get_sdo_by_pos_const(
        const ec_slave_config_t *sc, /**< Slave configuration. */
        unsigned int pos /**< Position in the list. */
        )
{
    const ec_sdo_request_t *req;

    list_for_each_entry(req, &sc->sdo_configs, list) {
        if (pos--)
            continue;
        return req;
    }

    return NULL;
}

/******************************************************************************
 *  Realtime interface
 *****************************************************************************/

int ecrt_slave_config_sync_manager(ec_slave_config_t *sc, uint8_t sync_index,
        ec_direction_t dir)
{
    ec_sync_config_t *sync_config;
    
    if (sc->master->debug_level)
        EC_DBG("ecrt_slave_config_sync_manager(sc = 0x%x, sync_index = %u, "
                "dir = %u)\n", (u32) sc, sync_index, dir);

    if (sync_index >= EC_MAX_SYNC_MANAGERS) {
        EC_ERR("Invalid sync manager index %u!\n", sync_index);
        return -1;
    }

    if (dir != EC_DIR_OUTPUT && dir != EC_DIR_INPUT) {
        EC_ERR("Invalid direction %u!\n", (u32) dir);
        return -1;
    }

    sync_config = &sc->sync_configs[sync_index];
    sync_config->dir = dir;
    return 0;
}

/*****************************************************************************/

int ecrt_slave_config_pdo_assign_add(ec_slave_config_t *sc,
        uint8_t sync_index, uint16_t pdo_index)
{
    ec_pdo_t *pdo;

    if (sc->master->debug_level)
        EC_DBG("ecrt_slave_config_pdo_assign_add(sc = 0x%x, sync_index = %u, "
                "pdo_index = 0x%04X)\n", (u32) sc, sync_index, pdo_index);

    if (sync_index >= EC_MAX_SYNC_MANAGERS) {
        EC_ERR("Invalid sync manager index %u!\n", sync_index);
        return -1;
    }

    down(&sc->master->master_sem);

    if (!(pdo = ec_pdo_list_add_pdo(&sc->sync_configs[sync_index].pdos,
                    pdo_index))) {
        up(&sc->master->master_sem);
        return -1;
    }
    pdo->sync_index = sync_index;

    ec_slave_config_load_default_mapping(sc, pdo);

    up(&sc->master->master_sem);
    return 0;
}

/*****************************************************************************/

void ecrt_slave_config_pdo_assign_clear(ec_slave_config_t *sc,
        uint8_t sync_index)
{
    if (sc->master->debug_level)
        EC_DBG("ecrt_slave_config_pdo_assign_clear(sc = 0x%x, "
                "sync_index = %u)\n", (u32) sc, sync_index);

    if (sync_index >= EC_MAX_SYNC_MANAGERS) {
        EC_ERR("Invalid sync manager index %u!\n", sync_index);
        return;
    }

    down(&sc->master->master_sem);
    ec_pdo_list_clear_pdos(&sc->sync_configs[sync_index].pdos);
    up(&sc->master->master_sem);
}

/*****************************************************************************/

int ecrt_slave_config_pdo_mapping_add(ec_slave_config_t *sc,
        uint16_t pdo_index, uint16_t entry_index, uint8_t entry_subindex,
        uint8_t entry_bit_length)
{
    uint8_t sync_index;
    ec_pdo_t *pdo = NULL;
    int retval = -1;
    
    if (sc->master->debug_level)
        EC_DBG("ecrt_slave_config_pdo_mapping_add(sc = 0x%x, "
                "pdo_index = 0x%04X, entry_index = 0x%04X, "
                "entry_subindex = 0x%02X, entry_bit_length = %u)\n",
                (u32) sc, pdo_index, entry_index, entry_subindex,
                entry_bit_length);

    for (sync_index = 0; sync_index < EC_MAX_SYNC_MANAGERS; sync_index++)
        if ((pdo = ec_pdo_list_find_pdo(
                        &sc->sync_configs[sync_index].pdos, pdo_index)))
            break;

    if (pdo) {
        down(&sc->master->master_sem);
        retval = ec_pdo_add_entry(pdo, entry_index, entry_subindex,
                entry_bit_length) ? 0 : -1;
        up(&sc->master->master_sem);
    } else {
        EC_ERR("Pdo 0x%04X is not assigned in config %u:%u.\n",
                pdo_index, sc->alias, sc->position);
    }

    return retval;
}

/*****************************************************************************/

void ecrt_slave_config_pdo_mapping_clear(ec_slave_config_t *sc,
        uint16_t pdo_index)
{
    uint8_t sync_index;
    ec_pdo_t *pdo = NULL;
    
    if (sc->master->debug_level)
        EC_DBG("ecrt_slave_config_pdo_mapping_clear(sc = 0x%x, "
                "pdo_index = 0x%04X)\n", (u32) sc, pdo_index);

    for (sync_index = 0; sync_index < EC_MAX_SYNC_MANAGERS; sync_index++)
        if ((pdo = ec_pdo_list_find_pdo(
                        &sc->sync_configs[sync_index].pdos, pdo_index)))
            break;

    if (pdo) {
        down(&sc->master->master_sem);
        ec_pdo_clear_entries(pdo);
        up(&sc->master->master_sem);
    } else {
        EC_WARN("Pdo 0x%04X is not assigned in config %u:%u.\n",
                pdo_index, sc->alias, sc->position);
    }
}

/*****************************************************************************/

int ecrt_slave_config_pdos(ec_slave_config_t *sc,
        unsigned int n_syncs, const ec_sync_info_t syncs[])
{
    unsigned int i, j, k;
    const ec_sync_info_t *sync_info;
    const ec_pdo_info_t *pdo_info;
    const ec_pdo_entry_info_t *entry_info;

    if (sc->master->debug_level)
        EC_DBG("ecrt_slave_config_pdos(sc = 0x%x, n_syncs = %u, "
                "syncs = 0x%x)\n", (u32) sc, n_syncs, (u32) syncs);

    if (!syncs)
        return 0;

    for (i = 0; i < n_syncs; i++) {
        sync_info = &syncs[i];

        if (sync_info->index == (uint8_t) EC_END)
            break;

        if (sync_info->index >= EC_MAX_SYNC_MANAGERS) {
            EC_ERR("Invalid sync manager index %u!\n", sync_info->index);
            return -1;
        }

        if (ecrt_slave_config_sync_manager(
                    sc, sync_info->index, sync_info->dir))
            return -1;

        if (sync_info->n_pdos && sync_info->pdos) {
            ecrt_slave_config_pdo_assign_clear(sc, sync_info->index);

            for (j = 0; j < sync_info->n_pdos; j++) {
                pdo_info = &sync_info->pdos[j];

                if (ecrt_slave_config_pdo_assign_add(
                            sc, sync_info->index, pdo_info->index))
                    return -1;

                if (pdo_info->n_entries && pdo_info->entries) {
                    ecrt_slave_config_pdo_mapping_clear(sc, pdo_info->index);

                    for (k = 0; k < pdo_info->n_entries; k++) {
                        entry_info = &pdo_info->entries[k];

                        if (ecrt_slave_config_pdo_mapping_add(sc,
                                    pdo_info->index, entry_info->index,
                                    entry_info->subindex,
                                    entry_info->bit_length))
                            return -1;
                    }
                }
            }
        }
    }

    return 0;
}

/*****************************************************************************/

int ecrt_slave_config_reg_pdo_entry(
        ec_slave_config_t *sc,
        uint16_t index,
        uint8_t subindex,
        ec_domain_t *domain,
        unsigned int *bit_position
        )
{
    uint8_t sync_index;
    const ec_sync_config_t *sync_config;
    unsigned int bit_offset, bit_pos;
    ec_pdo_t *pdo;
    ec_pdo_entry_t *entry;
    int sync_offset;

    if (sc->master->debug_level)
        EC_DBG("ecrt_slave_config_reg_pdo_entry(sc = 0x%x, index = 0x%04X, "
                "subindex = 0x%02X, domain = 0x%x, bit_position = 0x%x)\n",
                (u32) sc, index, subindex, (u32) domain, (u32) bit_position);

    for (sync_index = 0; sync_index < EC_MAX_SYNC_MANAGERS; sync_index++) {
        sync_config = &sc->sync_configs[sync_index];
        bit_offset = 0;

        list_for_each_entry(pdo, &sync_config->pdos.list, list) {
            list_for_each_entry(entry, &pdo->entries, list) {
                if (entry->index != index || entry->subindex != subindex) {
                    bit_offset += entry->bit_length;
                } else {
                    bit_pos = bit_offset % 8;
                    if (bit_position) {
                        *bit_position = bit_pos;
                    } else if (bit_pos) {
                        EC_ERR("Pdo entry 0x%04X:%02X does not byte-align "
                                "in config %u:%u.\n", index, subindex,
                                sc->alias, sc->position);
                        return -3;
                    }

                    sync_offset = ec_slave_config_prepare_fmmu(
                            sc, domain, sync_index, sync_config->dir);
                    if (sync_offset < 0)
                        return -2;

                    return sync_offset + bit_offset / 8;
                }
            }
        }
    }

    EC_ERR("Pdo entry 0x%04X:%02X is not mapped in slave config %u:%u.\n",
           index, subindex, sc->alias, sc->position);
    return -1;
}


/*****************************************************************************/

int ecrt_slave_config_sdo(ec_slave_config_t *sc, uint16_t index,
        uint8_t subindex, const uint8_t *data, size_t size)
{
    ec_slave_t *slave = sc->slave;
    ec_sdo_request_t *req;

    if (slave && !(slave->sii.mailbox_protocols & EC_MBOX_COE)) {
        EC_ERR("Slave %u does not support CoE!\n", slave->ring_position);
        return -1;
    }

    if (!(req = (ec_sdo_request_t *)
          kmalloc(sizeof(ec_sdo_request_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate memory for Sdo configuration!\n");
        return -1;
    }

    ec_sdo_request_init(req);
    ec_sdo_request_address(req, index, subindex);

    if (ec_sdo_request_copy_data(req, data, size)) {
        ec_sdo_request_clear(req);
        kfree(req);
        return -1;
    }
        
    down(&sc->master->master_sem);
    list_add_tail(&req->list, &sc->sdo_configs);
    up(&sc->master->master_sem);

    return 0;
}

/*****************************************************************************/

int ecrt_slave_config_sdo8(ec_slave_config_t *sc, uint16_t index,
        uint8_t subindex, uint8_t value)
{
    uint8_t data[1];
    EC_WRITE_U8(data, value);
    return ecrt_slave_config_sdo(sc, index, subindex, data, 1);
}

/*****************************************************************************/

int ecrt_slave_config_sdo16(ec_slave_config_t *sc, uint16_t index,
        uint8_t subindex, uint16_t value)
{
    uint8_t data[2];
    EC_WRITE_U16(data, value);
    return ecrt_slave_config_sdo(sc, index, subindex, data, 2);
}

/*****************************************************************************/

int ecrt_slave_config_sdo32(ec_slave_config_t *sc, uint16_t index,
        uint8_t subindex, uint32_t value)
{
    uint8_t data[4];
    EC_WRITE_U32(data, value);
    return ecrt_slave_config_sdo(sc, index, subindex, data, 4);
}

/*****************************************************************************/

ec_sdo_request_t *ecrt_slave_config_create_sdo_request(ec_slave_config_t *sc,
        uint16_t index, uint8_t subindex, size_t size)
{
    ec_sdo_request_t *req;

    if (!(req = (ec_sdo_request_t *)
                kmalloc(sizeof(ec_sdo_request_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate Sdo request memory!\n");
        return NULL;
    }

    ec_sdo_request_init(req);
    ec_sdo_request_address(req, index, subindex);

    if (ec_sdo_request_alloc(req, size)) {
        ec_sdo_request_clear(req);
        kfree(req);
        return NULL;
    }

    // prepare data for optional writing
    memset(req->data, 0x00, size);
    req->data_size = size;
    
    down(&sc->master->master_sem);
    list_add_tail(&req->list, &sc->sdo_requests);
    up(&sc->master->master_sem);

    return req; 
}

/*****************************************************************************/

void ecrt_slave_config_state(const ec_slave_config_t *sc,
        ec_slave_config_state_t *state)
{
    state->online = sc->slave ? 1 : 0;
    if (state->online) {
        state->operational =
            sc->slave->current_state == EC_SLAVE_STATE_OP;
        state->al_state = sc->slave->current_state;
    } else {
        state->operational = 0;
        state->al_state = EC_SLAVE_STATE_UNKNOWN;
    }
}

/*****************************************************************************/

/** \cond */

EXPORT_SYMBOL(ecrt_slave_config_sync_manager);
EXPORT_SYMBOL(ecrt_slave_config_pdo_assign_add);
EXPORT_SYMBOL(ecrt_slave_config_pdo_assign_clear);
EXPORT_SYMBOL(ecrt_slave_config_pdo_mapping_add);
EXPORT_SYMBOL(ecrt_slave_config_pdo_mapping_clear);
EXPORT_SYMBOL(ecrt_slave_config_pdos);
EXPORT_SYMBOL(ecrt_slave_config_reg_pdo_entry);
EXPORT_SYMBOL(ecrt_slave_config_sdo);
EXPORT_SYMBOL(ecrt_slave_config_sdo8);
EXPORT_SYMBOL(ecrt_slave_config_sdo16);
EXPORT_SYMBOL(ecrt_slave_config_sdo32);
EXPORT_SYMBOL(ecrt_slave_config_create_sdo_request);
EXPORT_SYMBOL(ecrt_slave_config_state);

/** \endcond */

/*****************************************************************************/
