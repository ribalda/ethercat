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
#include "master.h"
#include "slave_config.h"

#include "domain.h"

/*****************************************************************************/

void ec_domain_clear(struct kobject *);
void ec_domain_clear_data(ec_domain_t *);
ssize_t ec_show_domain_attribute(struct kobject *, struct attribute *, char *);

/*****************************************************************************/

/** Working counter increment values for logical read/write operations.
 *
 * \attention This is indexed by ec_direction_t.
 */
static const unsigned int working_counter_increment[] = {2, 1};

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

/** Domain constructor.
 *
 * \return 0 in case of success, else < 0
 */
int ec_domain_init(
        ec_domain_t *domain, /**< EtherCAT domain. */
        ec_master_t *master, /**< Parent master. */
        unsigned int index /**< Index. */
        )
{
    domain->master = master;
    domain->index = index;
    domain->data_size = 0;
    domain->expected_working_counter = 0x0000;
    domain->data = NULL;
    domain->data_origin = EC_ORIG_INTERNAL;
    domain->logical_base_address = 0L;
    domain->working_counter = 0xFFFF;
    domain->working_counter_changes = 0;
    domain->notify_jiffies = 0;

    INIT_LIST_HEAD(&domain->datagrams);

    // init kobject and add it to the hierarchy
    memset(&domain->kobj, 0x00, sizeof(struct kobject));
    kobject_init(&domain->kobj);
    domain->kobj.ktype = &ktype_ec_domain;
    domain->kobj.parent = &master->kobj;
    if (kobject_set_name(&domain->kobj, "domain%u", index)) {
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

/** Domain destructor.
 *
 * Clears and frees a domain object.
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

/** Clear and free domain.
 *
 * This method is called by the kobject, once there are no more references
 * to it.
 */
void ec_domain_clear(struct kobject *kobj /**< kobject of the domain */)
{
    ec_domain_t *domain;
    ec_datagram_t *datagram, *next;

    domain = container_of(kobj, ec_domain_t, kobj);

    list_for_each_entry_safe(datagram, next, &domain->datagrams, list) {
        ec_datagram_clear(datagram);
        kfree(datagram);
    }

    ec_domain_clear_data(domain);

    kfree(domain);
}

/*****************************************************************************/

/** Frees internally allocated memory.
 */
void ec_domain_clear_data(
        ec_domain_t *domain /**< EtherCAT domain. */
        )
{
    if (domain->data_origin == EC_ORIG_INTERNAL && domain->data)
        kfree(domain->data);
    domain->data = NULL;
    domain->data_origin = EC_ORIG_INTERNAL;
}

/*****************************************************************************/

/** Adds an FMMU configuration to the domain.
 */
void ec_domain_add_fmmu_config(
        ec_domain_t *domain, /**< EtherCAT domain. */
        ec_fmmu_config_t *fmmu /**< FMMU configuration. */
        )
{
    unsigned int wc_increment;
    fmmu->domain = domain;

    domain->data_size += fmmu->data_size;
    wc_increment = working_counter_increment[fmmu->dir];
    domain->expected_working_counter += wc_increment;

    if (domain->master->debug_level)
        EC_DBG("Domain %u: Added %u bytes (now %u) with dir %u -> WC %u"
                " (now %u).\n", domain->index, fmmu->data_size,
                domain->data_size, fmmu->dir, wc_increment,
                domain->expected_working_counter);
}

/*****************************************************************************/

/** Allocates a domain datagram and appends it to the list.
 *
 * \return 0 in case of success, else < 0
 */
int ec_domain_add_datagram(
        ec_domain_t *domain, /**< EtherCAT domain. */
        uint32_t logical_offset, /**< Logical offset. */
        size_t data_size, /**< Size of the data. */
        uint8_t *data, /**< Process data. */
        const unsigned int used[] /**< Used by inputs/outputs. */
        )
{
    ec_datagram_t *datagram;

    if (!(datagram = kmalloc(sizeof(ec_datagram_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate domain datagram!\n");
        return -1;
    }

    ec_datagram_init(datagram);
    snprintf(datagram->name, EC_DATAGRAM_NAME_SIZE,
            "domain%u-%u", domain->index, logical_offset);

    if (used[EC_DIR_OUTPUT] && used[EC_DIR_INPUT]) { // inputs and outputs
        if (ec_datagram_lrw(datagram, logical_offset, data_size, data)) {
            kfree(datagram);
            return -1;
        }
    } else if (used[EC_DIR_OUTPUT]) { // outputs only
        if (ec_datagram_lwr(datagram, logical_offset, data_size, data)) {
            kfree(datagram);
            return -1;
        }
    } else { // inputs only (or nothing)
        if (ec_datagram_lrd(datagram, logical_offset, data_size, data)) {
            kfree(datagram);
            return -1;
        }
    }

    list_add_tail(&datagram->list, &domain->datagrams);
    return 0;
}

/*****************************************************************************/

/** Finishes a domain.
 *
 * This allocates the necessary datagrams and writes the correct logical
 * addresses to every configured FMMU.
 *
 * \todo Check for FMMUs that do not fit into any datagram.
 *
 * \retval 0 in case of success
 * \retval <0 on failure.
 */
int ec_domain_finish(
        ec_domain_t *domain, /**< EtherCAT domain. */
        uint32_t base_address /**< Logical base address. */
        )
{
    uint32_t datagram_offset;
    size_t datagram_size;
    unsigned int datagram_count, i;
    unsigned int datagram_used[2];
    ec_slave_config_t *sc;
    ec_fmmu_config_t *fmmu;
    const ec_datagram_t *datagram;

    domain->logical_base_address = base_address;

    if (domain->data_size && domain->data_origin == EC_ORIG_INTERNAL) {
        if (!(domain->data =
                    (uint8_t *) kmalloc(domain->data_size, GFP_KERNEL))) {
            EC_ERR("Failed to allocate %u bytes internal memory for"
                    " domain %u!\n", domain->data_size, domain->index);
            return -1;
        }
    }

    // Cycle through all domain FMMUS, correct the logical base addresses and
    // set up the datagrams to carry the process data.
    datagram_offset = 0;
    datagram_size = 0;
    datagram_count = 0;
    datagram_used[EC_DIR_OUTPUT] = 0;
    datagram_used[EC_DIR_INPUT] = 0;

    list_for_each_entry(sc, &domain->master->configs, list) {
        for (i = 0; i < sc->used_fmmus; i++) {
            fmmu = &sc->fmmu_configs[i];
            if (fmmu->domain != domain)
                continue;

            // Correct logical FMMU address
            fmmu->logical_start_address += base_address;

            // Increment Input/Output counter
            datagram_used[fmmu->dir]++;

            // If the current FMMU's data do not fit in the current datagram,
            // allocate a new one.
            if (datagram_size + fmmu->data_size > EC_MAX_DATA_SIZE) {
                if (ec_domain_add_datagram(domain,
                            domain->logical_base_address + datagram_offset,
                            datagram_size, domain->data + datagram_offset,
                            datagram_used))
                    return -1;
                datagram_offset += datagram_size;
                datagram_size = 0;
                datagram_count++;
                datagram_used[EC_DIR_OUTPUT] = 0;
                datagram_used[EC_DIR_INPUT] = 0;
            }

            datagram_size += fmmu->data_size;
        }
    }

    // allocate last datagram, if data are left
    if (datagram_size) {
        if (ec_domain_add_datagram(domain,
                    domain->logical_base_address + datagram_offset,
                    datagram_size, domain->data + datagram_offset,
                    datagram_used))
            return -1;
        datagram_count++;
    }

    EC_INFO("Domain %u with logical offset %u contains %u bytes.\n",
            domain->index, domain->logical_base_address, domain->data_size);
    list_for_each_entry(datagram, &domain->datagrams, list) {
        EC_INFO("  Datagram %s, logical offset %u, size %u, type %s.\n",
                datagram->name, EC_READ_U32(datagram->address),
                datagram->data_size, ec_datagram_type_string(datagram));
    }
    
    return 0;
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
        return sprintf(buffer, "%u\n", domain->data_size);
    }

    return 0;
}

/******************************************************************************
 *  Realtime interface
 *****************************************************************************/

int ecrt_domain_reg_pdo_entry_list(ec_domain_t *domain,
        const ec_pdo_entry_reg_t *regs)
{
    const ec_pdo_entry_reg_t *reg;
    ec_slave_config_t *sc;
    int ret;
    
    for (reg = regs; reg->index; reg++) {
        if (!(sc = ecrt_master_slave_config(domain->master, reg->alias,
                        reg->position, reg->vendor_id, reg->product_code)))
            return -1;

        if ((ret = ecrt_slave_config_reg_pdo_entry(sc, reg->index,
                        reg->subindex, domain, reg->bit_position)) < 0)
            return -1;

        *reg->offset = ret;
    }

    return 0;
}

/*****************************************************************************/

size_t ecrt_domain_size(ec_domain_t *domain)
{
    return domain->data_size;
}

/*****************************************************************************/

void ecrt_domain_external_memory(ec_domain_t *domain, uint8_t *mem)
{
    ec_domain_clear_data(domain);

    domain->data = mem;
    domain->data_origin = EC_ORIG_EXTERNAL;
}

/*****************************************************************************/

uint8_t *ecrt_domain_data(ec_domain_t *domain)
{
    return domain->data;
}

/*****************************************************************************/

void ecrt_domain_process(ec_domain_t *domain)
{
    unsigned int working_counter_sum;
    ec_datagram_t *datagram;

    working_counter_sum = 0;
    list_for_each_entry(datagram, &domain->datagrams, list) {
        ec_datagram_output_stats(datagram);
        if (datagram->state == EC_DATAGRAM_RECEIVED) {
            working_counter_sum += datagram->working_counter;
        }
    }

    if (working_counter_sum != domain->working_counter) {
        domain->working_counter_changes++;
        domain->working_counter = working_counter_sum;
    }

    if (domain->working_counter_changes &&
        jiffies - domain->notify_jiffies > HZ) {
        domain->notify_jiffies = jiffies;
        if (domain->working_counter_changes == 1) {
            EC_INFO("Domain %u: Working counter changed to %u/%u.\n",
                    domain->index, domain->working_counter,
                    domain->expected_working_counter);
        }
        else {
            EC_INFO("Domain %u: %u working counter changes. Currently %u/%u.\n",
                    domain->index, domain->working_counter_changes,
                    domain->working_counter, domain->expected_working_counter);
        }
        domain->working_counter_changes = 0;
    }
}

/*****************************************************************************/

void ecrt_domain_queue(ec_domain_t *domain)
{
    ec_datagram_t *datagram;

    list_for_each_entry(datagram, &domain->datagrams, list) {
        ec_master_queue_datagram(domain->master, datagram);
    }
}

/*****************************************************************************/

void ecrt_domain_state(const ec_domain_t *domain, ec_domain_state_t *state)
{
    state->working_counter = domain->working_counter;

    if (domain->working_counter) {
        if (domain->working_counter == domain->expected_working_counter) {
            state->wc_state = EC_WC_COMPLETE;
        } else {
            state->wc_state = EC_WC_INCOMPLETE;
        }
    } else {
        state->wc_state = EC_WC_ZERO;
    }
}

/*****************************************************************************/

/** \cond */

EXPORT_SYMBOL(ecrt_domain_reg_pdo_entry_list);
EXPORT_SYMBOL(ecrt_domain_size);
EXPORT_SYMBOL(ecrt_domain_external_memory);
EXPORT_SYMBOL(ecrt_domain_data);
EXPORT_SYMBOL(ecrt_domain_process);
EXPORT_SYMBOL(ecrt_domain_queue);
EXPORT_SYMBOL(ecrt_domain_state);

/** \endcond */

/*****************************************************************************/
