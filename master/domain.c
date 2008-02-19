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
    domain->base_address = 0;
    domain->working_counter = 0xFFFFFFFF;
    domain->notify_jiffies = 0;
    domain->working_counter_changes = 0;

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
    ec_domain_t *domain;
    ec_datagram_t *datagram, *next;

    domain = container_of(kobj, ec_domain_t, kobj);

    list_for_each_entry_safe(datagram, next, &domain->datagrams, list) {
        ec_datagram_clear(datagram);
        kfree(datagram);
    }

    kfree(domain);
}

/*****************************************************************************/

/** Allocates a domain datagram and appends it to the list.
 *
 * \return 0 in case of success, else < 0
 */
int ec_domain_add_datagram(
        ec_domain_t *domain, /**< EtherCAT domain. */
        uint32_t offset, /**< Logical offset. */
        size_t data_size /**< Size of the data. */
        )
{
    ec_datagram_t *datagram;

    if (!(datagram = kmalloc(sizeof(ec_datagram_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate domain datagram!\n");
        return -1;
    }

    ec_datagram_init(datagram);
    snprintf(datagram->name, EC_DATAGRAM_NAME_SIZE,
            "domain%u-%u", domain->index, offset);

    if (ec_datagram_lrw(datagram, offset, data_size)) {
        kfree(datagram);
        return -1;
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
 * \retval 0 in case of success
 * \retval <0 on failure.
 */
int ec_domain_finish(
        ec_domain_t *domain, /**< EtherCAT domain. */
        uint32_t base_address /**< Logical base address. */
        )
{
    uint32_t datagram_offset;
    size_t datagram_data_size;
    unsigned int datagram_count, i;
    ec_slave_config_t *sc;
    ec_fmmu_config_t *fmmu;

    domain->base_address = base_address;

    // Cycle through all domain FMMUS, add the logical base address and assign
    // as many PDO entries as possible to the datagrams.
    datagram_offset = base_address;
    datagram_data_size = 0;
    datagram_count = 0;
    list_for_each_entry(sc, &domain->master->configs, list) {
        for (i = 0; i < sc->used_fmmus; i++) {
            fmmu = &sc->fmmu_configs[i];
            if (fmmu->domain != domain)
                continue;

            fmmu->logical_start_address += base_address;
            if (datagram_data_size + fmmu->data_size > EC_MAX_DATA_SIZE) {
                if (ec_domain_add_datagram(domain, datagram_offset,
                            datagram_data_size)) return -1;
                datagram_offset += datagram_data_size;
                datagram_data_size = 0;
                datagram_count++;
            }
            datagram_data_size += fmmu->data_size;
        }
    }

    // allocate last datagram
    if (datagram_data_size) {
        if (ec_domain_add_datagram(domain, datagram_offset,
                                   datagram_data_size))
            return -1;
        datagram_count++;
    }

    EC_INFO("Domain %u with logical offset %u contains %u bytes in %u"
            " datagram%s.\n", domain->index, domain->base_address,
            domain->data_size, datagram_count, datagram_count == 1 ? "" : "s");
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

int ecrt_domain_reg_pdo_entry(ec_domain_t *domain, ec_slave_config_t *sc,
        uint16_t index, uint8_t subindex)
{
    return ec_slave_config_reg_pdo_entry(sc, domain, index, subindex);
}

/*****************************************************************************/

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

        if ((ret = ecrt_domain_reg_pdo_entry(domain, sc, reg->index,
                        reg->subindex)) < 0)
            return -1;

        *reg->offset = ret;
    }

    return 0;
}

/*****************************************************************************/

void ecrt_domain_process(ec_domain_t *domain)
{
    unsigned int working_counter_sum;
    ec_datagram_t *datagram;

    working_counter_sum = 0;
    domain->state = 0;
    list_for_each_entry(datagram, &domain->datagrams, list) {
        ec_datagram_output_stats(datagram);
        if (datagram->state == EC_DATAGRAM_RECEIVED) {
            working_counter_sum += datagram->working_counter;
        }
        else {
            domain->state = -1;
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
            EC_INFO("Domain %u working counter change: %u\n", domain->index,
                    domain->working_counter);
        }
        else {
            EC_INFO("Domain %u: %u working counter changes. Currently %u\n",
                    domain->index, domain->working_counter_changes,
                    domain->working_counter);
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
    state->wc_state = EC_WC_ZERO; // FIXME
}

/*****************************************************************************/

/** \cond */

EXPORT_SYMBOL(ecrt_domain_reg_pdo_entry);
EXPORT_SYMBOL(ecrt_domain_reg_pdo_entry_list);
//EXPORT_SYMBOL(ecrt_domain_size);
//EXPORT_SYMBOL(ecrt_domain_memory);
EXPORT_SYMBOL(ecrt_domain_process);
EXPORT_SYMBOL(ecrt_domain_queue);
EXPORT_SYMBOL(ecrt_domain_state);

/** \endcond */

/*****************************************************************************/
