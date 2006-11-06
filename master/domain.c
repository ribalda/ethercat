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
   EtherCAT domain methods.
*/

/*****************************************************************************/

#include <linux/module.h>

#include "globals.h"
#include "domain.h"
#include "master.h"

/*****************************************************************************/

/**
   Data registration type.
*/

typedef struct
{
    struct list_head list; /**< list item */
    ec_slave_t *slave; /**< slave */
    const ec_sii_sync_t *sync; /**< sync manager */
    off_t sync_offset; /**< pdo offset */
    void **data_ptr; /**< pointer to process data pointer(s) */
}
ec_data_reg_t;

/*****************************************************************************/

void ec_domain_clear(struct kobject *);
void ec_domain_clear_data_regs(ec_domain_t *);
ssize_t ec_show_domain_attribute(struct kobject *, struct attribute *, char *);

/*****************************************************************************/

/** \cond */

EC_SYSFS_READ_ATTR(image_size);

static struct attribute *def_attrs[] = {
    &attr_image_size,
    NULL,
};

static struct sysfs_ops sysfs_ops = {
    .show = &ec_show_domain_attribute,
    .store = NULL
};

static struct kobj_type ktype_ec_domain = {
    .release = ec_domain_clear,
    .sysfs_ops = &sysfs_ops,
    .default_attrs = def_attrs
};

/** \endcond */

/*****************************************************************************/

/**
   Domain constructor.
   \return 0 in case of success, else < 0
*/

int ec_domain_init(ec_domain_t *domain, /**< EtherCAT domain */
                   ec_master_t *master, /**< owning master */
                   unsigned int index /**< domain index */
                   )
{
    domain->master = master;
    domain->index = index;
    domain->data_size = 0;
    domain->base_address = 0;
    domain->response_count = 0xFFFFFFFF;
    domain->notify_jiffies = 0;
    domain->working_counter_changes = 0;

    INIT_LIST_HEAD(&domain->data_regs);
    INIT_LIST_HEAD(&domain->datagrams);

    // init kobject and add it to the hierarchy
    memset(&domain->kobj, 0x00, sizeof(struct kobject));
    kobject_init(&domain->kobj);
    domain->kobj.ktype = &ktype_ec_domain;
    domain->kobj.parent = &master->kobj;
    if (kobject_set_name(&domain->kobj, "domain%i", index)) {
        EC_ERR("Failed to set kobj name.\n");
        kobject_put(&domain->kobj);
        return -1;
    }
    if (kobject_add(&domain->kobj)) {
        EC_ERR("Failed to add domain kobject.\n");
        kobject_put(&domain->kobj);
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Domain destructor.
   Clears and frees a domain object.
*/

void ec_domain_destroy(ec_domain_t *domain /**< EtherCAT domain */)
{
    ec_datagram_t *datagram;

    // dequeue datagrams
    list_for_each_entry(datagram, &domain->datagrams, list) {
        if (!list_empty(&datagram->queue)) // datagram queued?
            list_del_init(&datagram->queue);
    }

    // destroy self
    kobject_del(&domain->kobj);
    kobject_put(&domain->kobj);
}

/*****************************************************************************/

/**
   Clear and free domain.
   This method is called by the kobject,
   once there are no more references to it.
*/

void ec_domain_clear(struct kobject *kobj /**< kobject of the domain */)
{
    ec_datagram_t *datagram, *next;
    ec_domain_t *domain;

    domain = container_of(kobj, ec_domain_t, kobj);

    list_for_each_entry_safe(datagram, next, &domain->datagrams, list) {
        ec_datagram_clear(datagram);
        kfree(datagram);
    }

    ec_domain_clear_data_regs(domain);

    kfree(domain);
}

/*****************************************************************************/

/**
   Registeres a PDO entry.
   \return 0 in case of success, else < 0
*/

int ec_domain_reg_pdo_entry(ec_domain_t *domain, /**< EtherCAT domain */
                            ec_slave_t *slave, /**< slave */
                            const ec_sii_pdo_t *pdo, /**< PDO */
                            const ec_sii_pdo_entry_t *entry,
                            /**< PDO registration entry */
                            void **data_ptr /**< pointer to the process data
                                               pointer */
                            )
{
    ec_data_reg_t *data_reg;
    const ec_sii_sync_t *sync;
    const ec_sii_pdo_t *other_pdo;
    const ec_sii_pdo_entry_t *other_entry;
    unsigned int bit_offset, byte_offset, sync_found;

    // Find sync manager for PDO
    sync_found = 0;
    list_for_each_entry(sync, &slave->sii_syncs, list) {
        if (sync->index == pdo->sync_index) {
            sync_found = 1;
            break;
        }
    }

    if (!sync_found) {
        EC_ERR("No sync manager for PDO 0x%04X:%i.",
               pdo->index, entry->subindex);
        return -1;
    }

    // Calculate offset (in sync manager) for process data pointer
    bit_offset = 0;
    byte_offset = 0;
    list_for_each_entry(other_pdo, &slave->sii_pdos, list) {
        if (other_pdo->sync_index != sync->index) continue;

        list_for_each_entry(other_entry, &other_pdo->entries, list) {
            if (other_entry == entry) {
                byte_offset = bit_offset / 8;
                break;
            }
            bit_offset += other_entry->bit_length;
        }
    }

    // Allocate memory for data registration object
    if (!(data_reg =
          (ec_data_reg_t *) kmalloc(sizeof(ec_data_reg_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate data registration.\n");
        return -1;
    }

    if (ec_slave_prepare_fmmu(slave, domain, sync)) {
        EC_ERR("FMMU configuration failed.\n");
        kfree(data_reg);
        return -1;
    }

    data_reg->slave = slave;
    data_reg->sync = sync;
    data_reg->sync_offset = byte_offset;
    data_reg->data_ptr = data_ptr;

    list_add_tail(&data_reg->list, &domain->data_regs);

    ec_slave_request_state(slave, EC_SLAVE_STATE_OP);

    return 0;
}

/*****************************************************************************/

/**
   Registeres a PDO range.
   \return 0 in case of success, else < 0
*/

int ec_domain_reg_pdo_range(ec_domain_t *domain, /**< EtherCAT domain */
                            ec_slave_t *slave, /**< slave */
                            ec_direction_t dir, /**< data direction */
                            uint16_t offset, /**< offset */
                            uint16_t length, /**< length */
                            void **data_ptr /**< pointer to the process data
                                               pointer */
                            )
{
    ec_data_reg_t *data_reg;
    ec_sii_sync_t *sync;
    unsigned int sync_found, sync_index;
    uint16_t sync_length;

    switch (dir) {
        case EC_DIR_OUTPUT: sync_index = 2; break;
        case EC_DIR_INPUT:  sync_index = 3; break;
        default:
            EC_ERR("Invalid direction!\n");
            return -1;
    }

    // Find sync manager
    sync_found = 0;
    list_for_each_entry(sync, &slave->sii_syncs, list) {
        if (sync->index == sync_index) {
            sync_found = 1;
            break;
        }
    }

    if (!sync_found) {
        EC_ERR("No sync manager found for PDO range.\n");
        return -1;
    }

     // Allocate memory for data registration object
    if (!(data_reg =
          (ec_data_reg_t *) kmalloc(sizeof(ec_data_reg_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate data registration.\n");
        return -1;
    }

    if (ec_slave_prepare_fmmu(slave, domain, sync)) {
        EC_ERR("FMMU configuration failed.\n");
        kfree(data_reg);
        return -1;
    }

    data_reg->slave = slave;
    data_reg->sync = sync;
    data_reg->sync_offset = offset;
    data_reg->data_ptr = data_ptr;

    // estimate sync manager length
    sync_length = offset + length;
    if (sync->est_length < sync_length) {
        sync->est_length = sync_length;
        if (domain->master->debug_level) {
            EC_DBG("Estimating length of sync manager %i of slave %i to %i.\n",
                   sync->index, slave->ring_position, sync_length);
        }
    }

    list_add_tail(&data_reg->list, &domain->data_regs);

    ec_slave_request_state(slave, EC_SLAVE_STATE_OP);

    return 0;
}

/*****************************************************************************/

/**
   Clears the list of the data registrations.
*/

void ec_domain_clear_data_regs(ec_domain_t *domain /**< EtherCAT domain */)
{
    ec_data_reg_t *data_reg, *next;

    list_for_each_entry_safe(data_reg, next, &domain->data_regs, list) {
        list_del(&data_reg->list);
        kfree(data_reg);
    }
}

/*****************************************************************************/

/**
   Allocates a process data datagram and appends it to the list.
   \return 0 in case of success, else < 0
*/

int ec_domain_add_datagram(ec_domain_t *domain, /**< EtherCAT domain */
                           uint32_t offset, /**< logical offset */
                           size_t data_size /**< size of the datagram data */
                           )
{
    ec_datagram_t *datagram;

    if (!(datagram = kmalloc(sizeof(ec_datagram_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate domain datagram!\n");
        return -1;
    }

    ec_datagram_init(datagram);

    if (ec_datagram_lrw(datagram, offset, data_size)) {
        kfree(datagram);
        return -1;
    }

    list_add_tail(&datagram->list, &domain->datagrams);
    return 0;
}

/*****************************************************************************/

/**
   Creates a domain.
   Reserves domain memory, calculates the logical addresses of the
   corresponding FMMUs and sets the process data pointer of the registered
   process data.
   \return 0 in case of success, else < 0
*/

int ec_domain_alloc(ec_domain_t *domain, /**< EtherCAT domain */
                    uint32_t base_address /**< logical base address */
                    )
{
    ec_data_reg_t *data_reg;
    ec_slave_t *slave;
    ec_fmmu_t *fmmu;
    unsigned int i, j, datagram_count;
    uint32_t pdo_off, pdo_off_datagram;
    uint32_t datagram_offset;
    size_t datagram_data_size, sync_size;
    ec_datagram_t *datagram;

    domain->base_address = base_address;

    // calculate size of process data and allocate memory
    domain->data_size = 0;
    datagram_offset = base_address;
    datagram_data_size = 0;
    datagram_count = 0;
    list_for_each_entry(slave, &domain->master->slaves, list) {
        for (j = 0; j < slave->fmmu_count; j++) {
            fmmu = &slave->fmmus[j];
            if (fmmu->domain == domain) {
                fmmu->logical_start_address = base_address + domain->data_size;
                sync_size = ec_slave_calc_sync_size(slave, fmmu->sync);
                domain->data_size += sync_size;
                if (datagram_data_size + sync_size > EC_MAX_DATA_SIZE) {
                    if (ec_domain_add_datagram(domain, datagram_offset,
                                               datagram_data_size)) return -1;
                    datagram_offset += datagram_data_size;
                    datagram_data_size = 0;
                    datagram_count++;
                }
                datagram_data_size += sync_size;
            }
        }
    }

    // allocate last datagram
    if (datagram_data_size) {
        if (ec_domain_add_datagram(domain, datagram_offset,
                                   datagram_data_size))
            return -1;
        datagram_count++;
    }

    if (!datagram_count) {
        EC_WARN("Domain %i contains no data!\n", domain->index);
        ec_domain_clear_data_regs(domain);
        return 0;
    }

    // set all process data pointers
    list_for_each_entry(data_reg, &domain->data_regs, list) {
        for (i = 0; i < data_reg->slave->fmmu_count; i++) {
            fmmu = &data_reg->slave->fmmus[i];
            if (fmmu->domain == domain && fmmu->sync == data_reg->sync) {
                pdo_off = fmmu->logical_start_address + data_reg->sync_offset;
                // search datagram
                list_for_each_entry(datagram, &domain->datagrams, list) {
                    pdo_off_datagram = pdo_off - datagram->address.logical;
                    if (pdo_off >= datagram->address.logical &&
                        pdo_off_datagram < datagram->mem_size) {
                        *data_reg->data_ptr = datagram->data +
                            pdo_off_datagram;
                    }
                }
                if (!data_reg->data_ptr) {
                    EC_ERR("Failed to assign data pointer!\n");
                    return -1;
                }
                break;
            }
        }
    }

    EC_INFO("Domain %i - Allocated %i bytes in %i datagram%s.\n",
            domain->index, domain->data_size, datagram_count,
            datagram_count == 1 ? "" : "s");

    ec_domain_clear_data_regs(domain);

    return 0;
}

/*****************************************************************************/

/**
   Places all process data datagrams in the masters datagram queue.
*/

void ec_domain_queue_datagrams(ec_domain_t *domain /**< EtherCAT domain */)
{
    ec_datagram_t *datagram;

    list_for_each_entry(datagram, &domain->datagrams, list) {
        ec_master_queue_datagram(domain->master, datagram);
    }
}

/*****************************************************************************/

/**
   Formats attribute data for SysFS reading.
   \return number of bytes to read
*/

ssize_t ec_show_domain_attribute(struct kobject *kobj, /**< kobject */
                                 struct attribute *attr, /**< attribute */
                                 char *buffer /**< memory to store data in */
                                 )
{
    ec_domain_t *domain = container_of(kobj, ec_domain_t, kobj);

    if (attr == &attr_image_size) {
        return sprintf(buffer, "%i\n", domain->data_size);
    }

    return 0;
}

/******************************************************************************
 *  Realtime interface
 *****************************************************************************/

/**
   Registers a PDO in a domain.
   - If \a data_ptr is NULL, the slave is only validated.
   \return pointer to the slave on success, else NULL
   \ingroup RealtimeInterface
*/

ec_slave_t *ecrt_domain_register_pdo(ec_domain_t *domain,
                                     /**< EtherCAT domain */
                                     const char *address,
                                     /**< ASCII address of the slave,
                                        see ecrt_master_get_slave() */
                                     uint32_t vendor_id,
                                     /**< vendor ID */
                                     uint32_t product_code,
                                     /**< product code */
                                     uint16_t pdo_index,
                                     /**< PDO index */
                                     uint8_t pdo_subindex,
                                     /**< PDO subindex */
                                     void **data_ptr
                                     /**< address of the process data
                                        pointer */
                                     )
{
    ec_slave_t *slave;
    ec_master_t *master;
    const ec_sii_pdo_t *pdo;
    const ec_sii_pdo_entry_t *entry;

    master = domain->master;

    // translate address and validate slave
    if (!(slave = ecrt_master_get_slave(master, address))) return NULL;
    if (ec_slave_validate(slave, vendor_id, product_code)) return NULL;

    if (!data_ptr) return slave;

    list_for_each_entry(pdo, &slave->sii_pdos, list) {
        list_for_each_entry(entry, &pdo->entries, list) {
            if (entry->index != pdo_index
                || entry->subindex != pdo_subindex) continue;

            if (ec_domain_reg_pdo_entry(domain, slave, pdo, entry, data_ptr)) {
                return NULL;
            }

            return slave;
        }
    }

    EC_ERR("Slave %i does not provide PDO 0x%04X:%i.\n",
           slave->ring_position, pdo_index, pdo_subindex);
    return NULL;
}

/*****************************************************************************/

/**
   Registeres a bunch of data fields.
   Caution! The list has to be terminated with a NULL structure ({})!
   \return 0 in case of success, else < 0
   \ingroup RealtimeInterface
*/

int ecrt_domain_register_pdo_list(ec_domain_t *domain,
                                  /**< EtherCAT domain */
                                  const ec_pdo_reg_t *pdos
                                  /**< array of PDO registrations */
                                  )
{
    const ec_pdo_reg_t *pdo;

    for (pdo = pdos; pdo->slave_address; pdo++)
        if (!ecrt_domain_register_pdo(domain, pdo->slave_address,
                                      pdo->vendor_id,
                                      pdo->product_code,
                                      pdo->pdo_index,
                                      pdo->pdo_subindex,
                                      pdo->data_ptr))
            return -1;

    return 0;
}

/*****************************************************************************/

/**
   Registers a PDO range in a domain.
   - If \a data_ptr is NULL, the slave is only validated.
   \return pointer to the slave on success, else NULL
   \ingroup RealtimeInterface
*/

ec_slave_t *ecrt_domain_register_pdo_range(ec_domain_t *domain,
                                           /**< EtherCAT domain */
                                           const char *address,
                                           /**< ASCII address of the slave,
                                              see ecrt_master_get_slave() */
                                           uint32_t vendor_id,
                                           /**< vendor ID */
                                           uint32_t product_code,
                                           /**< product code */
                                           ec_direction_t direction,
                                           /**< data direction */
                                           uint16_t offset,
                                           /**< offset in slave's PDO range */
                                           uint16_t length,
                                           /**< length of this range */
                                           void **data_ptr
                                           /**< address of the process data
                                              pointer */
                                           )
{
    ec_slave_t *slave;
    ec_master_t *master;

    master = domain->master;

    // translate address and validate slave
    if (!(slave = ecrt_master_get_slave(master, address))) return NULL;
    if (ec_slave_validate(slave, vendor_id, product_code)) return NULL;

    if (!data_ptr) return slave;

    if (ec_domain_reg_pdo_range(domain, slave,
                                direction, offset, length, data_ptr)) {
        return NULL;
    }

    return slave;
}

/*****************************************************************************/

/**
   Processes received process data and requeues the domain datagram(s).
   \ingroup RealtimeInterface
*/

void ecrt_domain_process(ec_domain_t *domain /**< EtherCAT domain */)
{
    unsigned int working_counter_sum;
    ec_datagram_t *datagram;

    working_counter_sum = 0;
    domain->state = 0;
    list_for_each_entry(datagram, &domain->datagrams, list) {
        if (datagram->state == EC_DATAGRAM_RECEIVED) {
            working_counter_sum += datagram->working_counter;
        }
        else {
            domain->state = -1;
        }
    }

    if (working_counter_sum != domain->response_count) {
        domain->working_counter_changes++;
        domain->response_count = working_counter_sum;
    }

    if (domain->working_counter_changes &&
        jiffies - domain->notify_jiffies > HZ) {
        domain->notify_jiffies = jiffies;
        if (domain->working_counter_changes == 1) {
            EC_INFO("Domain %i working counter change: %i\n", domain->index,
                    domain->response_count);
        }
        else {
            EC_INFO("Domain %i: %u WC changes. Current response count: %i\n",
                    domain->index, domain->working_counter_changes,
                    domain->response_count);
        }
        domain->working_counter_changes = 0;
    }

    ec_domain_queue_datagrams(domain);
}

/*****************************************************************************/

/**
   Returns the state of a domain.
   \return 0 if all datagrams were received, else -1.
   \ingroup RealtimeInterface
*/

int ecrt_domain_state(const ec_domain_t *domain /**< EtherCAT domain */)
{
    return domain->state;
}

/*****************************************************************************/

/** \cond */

EXPORT_SYMBOL(ecrt_domain_register_pdo);
EXPORT_SYMBOL(ecrt_domain_register_pdo_list);
EXPORT_SYMBOL(ecrt_domain_register_pdo_range);
EXPORT_SYMBOL(ecrt_domain_process);
EXPORT_SYMBOL(ecrt_domain_state);

/** \endcond */

/*****************************************************************************/
