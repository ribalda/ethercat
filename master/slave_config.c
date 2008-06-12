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
    ec_direction_t dir;

    sc->master = master;
    sc->alias = alias;
    sc->position = position;
    sc->vendor_id = vendor_id;
    sc->product_code = product_code;
    sc->slave = NULL;

    for (dir = EC_DIR_OUTPUT; dir <= EC_DIR_INPUT; dir++)
        ec_pdo_list_init(&sc->pdos[dir]);

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
    ec_direction_t dir;
    ec_sdo_request_t *req, *next_req;

    ec_slave_config_detach(sc);

    // Free Pdo mappings
    for (dir = EC_DIR_OUTPUT; dir <= EC_DIR_INPUT; dir++)
        ec_pdo_list_clear(&sc->pdos[dir]);

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
        EC_WARN("Failed to find slave for configuration %u:%u.\n",
                sc->alias, sc->position);
        return -1;
    }

	if (slave->config) {
		EC_ERR("Failed to attach slave configuration %u:%u. Slave %u"
				" already has a configuration!\n", sc->alias,
				sc->position, slave->ring_position);
		return -2;
	}
	if (slave->sii.vendor_id != sc->vendor_id
			|| slave->sii.product_code != sc->product_code) {
		EC_ERR("Slave %u has an invalid type (0x%08X/0x%08X) for"
				" configuration %u:%u (0x%08X/0x%08X).\n",
				slave->ring_position, slave->sii.vendor_id,
				slave->sii.product_code, sc->alias, sc->position,
				sc->vendor_id, sc->product_code);
		return -3;
	}

	// attach slave
	slave->config = sc;
	sc->slave = slave;

    if (sc->master->debug_level)
        EC_DBG("Attached slave %u to config %u:%u.\n",
                slave->ring_position, sc->alias, sc->position);

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

/** Loads the default Pdo assignment from the slave object.
 */
void ec_slave_config_load_default_assignment(ec_slave_config_t *sc)
{
    ec_direction_t dir;
    ec_pdo_list_t *pdos;
    ec_sync_t *sync;

    if (!sc->slave)
        return;
    
    for (dir = EC_DIR_OUTPUT; dir <= EC_DIR_INPUT; dir++) {
        pdos = &sc->pdos[dir];
        if ((sync = ec_slave_get_pdo_sync(sc->slave, dir)))
            ec_pdo_list_copy(pdos, &sync->pdos);
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
    const ec_sync_t *sync;
    const ec_pdo_t *default_pdo;

    if (sc->master->debug_level)
        EC_DBG("Loading default configuration for Pdo 0x%04X in"
                " config %u:%u.\n", pdo->index, sc->alias, sc->position);

    pdo->default_config = 1;

    if (!sc->slave) {
        EC_WARN("Failed to load default Pdo configuration for %u:%u:"
                " Slave not found.\n", sc->alias, sc->position);
        return;
    }

    if (!(sync = ec_slave_get_pdo_sync(sc->slave, pdo->dir))) {
        EC_WARN("Slave %u does not provide a default Pdo"
                " configuration!\n", sc->slave->ring_position);
        return;
    }

    list_for_each_entry(default_pdo, &sync->pdos.list, list) {
        if (default_pdo->index != pdo->index)
            continue;

        if (sc->master->debug_level)
            EC_DBG("  Found Pdo name \"%s\".\n",
                    default_pdo->name);

        // try to take Pdo name from mapped one
        ec_pdo_set_name(pdo, default_pdo->name);

        // copy entries (= default Pdo configuration)
        if (ec_pdo_copy_entries(pdo, default_pdo))
            return;

        if (sc->master->debug_level) {
            const ec_pdo_entry_t *entry;
            list_for_each_entry(entry, &pdo->entries, list) {
                EC_DBG("    Entry 0x%04X:%02X.\n",
                        entry->index, entry->subindex);
            }
        }
    }
}

/*****************************************************************************/

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

int ecrt_slave_config_pdo_assign_add(ec_slave_config_t *sc,
        ec_direction_t dir, uint16_t index)
{
    ec_pdo_list_t *pl = &sc->pdos[dir];
    ec_pdo_t *pdo;
    
    if (sc->master->debug_level)
        EC_DBG("Adding Pdo 0x%04X to assignment for dir %u, config %u:%u.\n",
                index, dir, sc->alias, sc->position);

    if (!(pdo = ec_pdo_list_add_pdo(pl, dir, index)))
        return -1;

    ec_slave_config_load_default_mapping(sc, pdo);
    return 0;
}

/*****************************************************************************/

void ecrt_slave_config_pdo_assign_clear(ec_slave_config_t *sc,
        ec_direction_t dir)
{
    if (sc->master->debug_level)
        EC_DBG("Clearing Pdo assignment for dir %u, config %u:%u.\n",
                dir, sc->alias, sc->position);

    ec_pdo_list_clear_pdos(&sc->pdos[dir]);
}

/*****************************************************************************/

int ecrt_slave_config_pdo_mapping_add(ec_slave_config_t *sc,
        uint16_t pdo_index, uint16_t entry_index, uint8_t entry_subindex,
        uint8_t entry_bit_length)
{
    ec_direction_t dir;
    ec_pdo_t *pdo;
    
    if (sc->master->debug_level)
        EC_DBG("Adding Pdo entry 0x%04X:%02X (%u bit) to mapping of Pdo"
                " 0x%04X, config %u:%u.\n", entry_index, entry_subindex,
                entry_bit_length, pdo_index, sc->alias, sc->position);

    for (dir = EC_DIR_OUTPUT; dir <= EC_DIR_INPUT; dir++)
        if ((pdo = ec_pdo_list_find_pdo(&sc->pdos[dir], pdo_index)))
            break;

    if (!pdo) {
        EC_ERR("Pdo 0x%04X is not assigned in config %u:%u.\n",
                pdo_index, sc->alias, sc->position);
        return -1;
    }

    return ec_pdo_add_entry(pdo, entry_index, entry_subindex,
            entry_bit_length) ? 0 : -1;
}

/*****************************************************************************/

void ecrt_slave_config_pdo_mapping_clear(ec_slave_config_t *sc,
        uint16_t pdo_index)
{
    ec_direction_t dir;
    ec_pdo_t *pdo;
    
    if (sc->master->debug_level)
        EC_DBG("Clearing mapping of Pdo 0x%04X, config %u:%u.\n",
                pdo_index, sc->alias, sc->position);

    for (dir = EC_DIR_OUTPUT; dir <= EC_DIR_INPUT; dir++)
        if ((pdo = ec_pdo_list_find_pdo(&sc->pdos[dir], pdo_index)))
            break;

    if (!pdo) {
        EC_WARN("Pdo 0x%04X is not assigned in config %u:%u.\n",
                pdo_index, sc->alias, sc->position);
        return;
    }

    ec_pdo_clear_entries(pdo);
}

/*****************************************************************************/

int ecrt_slave_config_pdos(ec_slave_config_t *sc, unsigned int n_infos,
        const ec_pdo_info_t pdo_infos[])
{
    unsigned int i, j;
    const ec_pdo_info_t *pi;
    ec_pdo_list_t *pl;
    unsigned int cleared[] = {0, 0};
    const ec_pdo_entry_info_t *ei;

    for (i = 0; i < n_infos; i++) {
        pi = &pdo_infos[i];

        if (pi->dir == EC_END)
            break;

        pl = &sc->pdos[pi->dir];

        if (!cleared[pi->dir]) {
            ecrt_slave_config_pdo_assign_clear(sc, pi->dir);
            cleared[pi->dir] = 1;
        }

        if (ecrt_slave_config_pdo_assign_add(sc, pi->dir, pi->index))
            return -1;

        if (pi->n_entries && pi->entries) { // mapping provided
            if (sc->master->debug_level)
                EC_DBG("  Pdo mapping information provided.\n");

            ecrt_slave_config_pdo_mapping_clear(sc, pi->index);

            for (j = 0; j < pi->n_entries; j++) {
                ei = &pi->entries[j];

                if (ecrt_slave_config_pdo_mapping_add(sc, pi->index,
                        ei->index, ei->subindex, ei->bit_length))
                    return -1;
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
    ec_direction_t dir;
    ec_pdo_list_t *pdos;
    unsigned int bit_offset, bit_pos;
    ec_pdo_t *pdo;
    ec_pdo_entry_t *entry;
    int sync_offset;

    if (sc->master->debug_level)
        EC_DBG("ecrt_slave_config_reg_pdo_entry(sc = 0x%x, index = 0x%04X, "
                "subindex = 0x%02X, domain = 0x%x, bit_position = 0x%x)\n",
                (unsigned int) sc, index, subindex, (unsigned int) domain,
                (unsigned int) bit_position);

    for (dir = EC_DIR_OUTPUT; dir <= EC_DIR_INPUT; dir++) {
        pdos = &sc->pdos[dir];
        bit_offset = 0;
        list_for_each_entry(pdo, &pdos->list, list) {
            list_for_each_entry(entry, &pdo->entries, list) {
                if (entry->index != index || entry->subindex != subindex) {
                    bit_offset += entry->bit_length;
                } else {
                    goto found;
                }
            }
        }
    }

    EC_ERR("Pdo entry 0x%04X:%02X is not mapped in slave config %u:%u.\n",
           index, subindex, sc->alias, sc->position);
    return -1;

found:
    sync_offset = ec_slave_config_prepare_fmmu(sc, domain, dir);
    if (sync_offset < 0)
        return -2;

    bit_pos = bit_offset % 8;
    if (bit_position) {
        *bit_position = bit_pos;
    } else if (bit_pos) {
        EC_ERR("Pdo entry 0x%04X:%02X does not byte-align in config %u:%u.\n",
                index, subindex, sc->alias, sc->position);
        return -3;
    }

    return sync_offset + bit_offset / 8;
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
        
    list_add_tail(&req->list, &sc->sdo_configs);
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
    
    list_add_tail(&req->list, &sc->sdo_requests);
    return req; 
}

/*****************************************************************************/

/** \cond */

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

/** \endcond */

/*****************************************************************************/
