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

#include "globals.h"
#include "domain.h"
#include "master.h"

/*****************************************************************************/

void ec_domain_clear_field_regs(ec_domain_t *);
ssize_t ec_show_domain_attribute(struct kobject *, struct attribute *, char *);

/*****************************************************************************/

/** \cond */

EC_SYSFS_READ_ATTR(data_size);

static struct attribute *def_attrs[] = {
    &attr_data_size,
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

    INIT_LIST_HEAD(&domain->field_regs);
    INIT_LIST_HEAD(&domain->commands);

    // init kobject and add it to the hierarchy
    memset(&domain->kobj, 0x00, sizeof(struct kobject));
    kobject_init(&domain->kobj);
    domain->kobj.ktype = &ktype_ec_domain;
    domain->kobj.parent = &master->kobj;
    if (kobject_set_name(&domain->kobj, "domain%i", index)) {
        EC_ERR("Failed to set kobj name.\n");
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Domain destructor.
*/

void ec_domain_clear(struct kobject *kobj /**< kobject of the domain */)
{
    ec_command_t *command, *next;
    ec_domain_t *domain;

    domain = container_of(kobj, ec_domain_t, kobj);

    EC_INFO("Clearing domain %i.\n", domain->index);

    list_for_each_entry_safe(command, next, &domain->commands, list) {
        ec_command_clear(command);
        kfree(command);
    }

    ec_domain_clear_field_regs(domain);

    kfree(domain);
}

/*****************************************************************************/

/**
   Registeres a data field in a domain.
   \return 0 in case of success, else < 0
*/

int ec_domain_reg_field(ec_domain_t *domain, /**< EtherCAT domain */
                        ec_slave_t *slave, /**< slave */
                        const ec_sync_t *sync, /**< sync manager */
                        uint32_t field_offset, /**< data field offset */
                        void **data_ptr /**< pointer to the process data
                                           pointer */
                        )
{
    ec_field_reg_t *field_reg;

    if (!(field_reg =
          (ec_field_reg_t *) kmalloc(sizeof(ec_field_reg_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate field registration.\n");
        return -1;
    }

    if (ec_slave_prepare_fmmu(slave, domain, sync)) {
        EC_ERR("FMMU configuration failed.\n");
        kfree(field_reg);
        return -1;
    }

    field_reg->slave = slave;
    field_reg->sync = sync;
    field_reg->field_offset = field_offset;
    field_reg->data_ptr = data_ptr;

    list_add_tail(&field_reg->list, &domain->field_regs);
    return 0;
}

/*****************************************************************************/

/**
   Clears the list of the registered data fields.
*/

void ec_domain_clear_field_regs(ec_domain_t *domain /**< EtherCAT domain */)
{
    ec_field_reg_t *field_reg, *next;

    list_for_each_entry_safe(field_reg, next, &domain->field_regs, list) {
        list_del(&field_reg->list);
        kfree(field_reg);
    }
}

/*****************************************************************************/

/**
   Allocates a process data command and appends it to the list.
   \return 0 in case of success, else < 0
*/

int ec_domain_add_command(ec_domain_t *domain, /**< EtherCAT domain */
                          uint32_t offset, /**< logical offset */
                          size_t data_size /**< size of the command data */
                          )
{
    ec_command_t *command;

    if (!(command = kmalloc(sizeof(ec_command_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate domain command!\n");
        return -1;
    }

    ec_command_init(command);

    if (ec_command_lrw(command, offset, data_size)) {
        kfree(command);
        return -1;
    }

    list_add_tail(&command->list, &domain->commands);
    return 0;
}

/*****************************************************************************/

/**
   Creates a domain.
   Reserves domain memory, calculates the logical addresses of the
   corresponding FMMUs and sets the process data pointer of the registered
   data fields.
   \return 0 in case of success, else < 0
*/

int ec_domain_alloc(ec_domain_t *domain, /**< EtherCAT domain */
                    uint32_t base_address /**< Logische Basisadresse */
                    )
{
    ec_field_reg_t *field_reg;
    ec_slave_t *slave;
    ec_fmmu_t *fmmu;
    unsigned int i, j, cmd_count;
    uint32_t field_off, field_off_cmd;
    uint32_t cmd_offset;
    size_t cmd_data_size;
    ec_command_t *command;

    domain->base_address = base_address;

    // Größe der Prozessdaten berechnen und Kommandos allozieren
    domain->data_size = 0;
    cmd_offset = base_address;
    cmd_data_size = 0;
    cmd_count = 0;
    list_for_each_entry(slave, &domain->master->slaves, list) {
        for (j = 0; j < slave->fmmu_count; j++) {
            fmmu = &slave->fmmus[j];
            if (fmmu->domain == domain) {
                fmmu->logical_start_address = base_address + domain->data_size;
                domain->data_size += fmmu->sync->size;
                if (cmd_data_size + fmmu->sync->size > EC_MAX_DATA_SIZE) {
                    if (ec_domain_add_command(domain, cmd_offset,
                                              cmd_data_size)) return -1;
                    cmd_offset += cmd_data_size;
                    cmd_data_size = 0;
                    cmd_count++;
                }
                cmd_data_size += fmmu->sync->size;
            }
        }
    }

    // Letztes Kommando allozieren
    if (cmd_data_size) {
        if (ec_domain_add_command(domain, cmd_offset, cmd_data_size))
            return -1;
        cmd_count++;
    }

    if (!cmd_count) {
        EC_WARN("Domain %i contains no data!\n", domain->index);
        ec_domain_clear_field_regs(domain);
        return 0;
    }

    // Alle Prozessdatenzeiger setzen
    list_for_each_entry(field_reg, &domain->field_regs, list) {
        for (i = 0; i < field_reg->slave->fmmu_count; i++) {
            fmmu = &field_reg->slave->fmmus[i];
            if (fmmu->domain == domain && fmmu->sync == field_reg->sync) {
                field_off = fmmu->logical_start_address +
                    field_reg->field_offset;
                // Kommando suchen
                list_for_each_entry(command, &domain->commands, list) {
                    field_off_cmd = field_off - command->address.logical;
                    if (field_off >= command->address.logical &&
                        field_off_cmd < command->mem_size) {
                        *field_reg->data_ptr = command->data + field_off_cmd;
                    }
                }
                if (!field_reg->data_ptr) {
                    EC_ERR("Failed to assign data pointer!\n");
                    return -1;
                }
                break;
            }
        }
    }

    EC_INFO("Domain %i - Allocated %i bytes in %i command%s\n",
            domain->index, domain->data_size, cmd_count,
            cmd_count == 1 ? "" : "s");

    ec_domain_clear_field_regs(domain);

    return 0;
}

/*****************************************************************************/

/**
   Sets the number of responding slaves and outputs it on demand.
   This number isn't really the number of responding slaves, but the sum of
   the working counters of all domain commands. Some slaves increase the
   working counter by 2, some by 1.
*/

void ec_domain_response_count(ec_domain_t *domain, /**< EtherCAT domain */
                              unsigned int count /**< new WC sum */
                              )
{
    if (count != domain->response_count) {
        domain->response_count = count;
        EC_INFO("Domain %i working counter change: %i\n", domain->index,
                count);
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

    if (attr == &attr_data_size) {
        return sprintf(buffer, "%i\n", domain->data_size);
    }

    return 0;
}

/******************************************************************************
 *  Realtime interface
 *****************************************************************************/

/**
   Registers a data field in a domain.
   - If \a data_ptr is NULL, the slave is only checked against its type.
   - If \a field_count is 0, it is assumed that one data field is to be
   registered.
   - If \a field_count is greater then 1, it is assumed that \a data_ptr
   is an array of the respective size.
   \return pointer to the slave on success, else NULL
   \ingroup RealtimeInterface
*/

ec_slave_t *ecrt_domain_register_field(ec_domain_t *domain,
                                       /**< EtherCAT domain */
                                       const char *address,
                                       /**< ASCII address of the slave,
                                          see ecrt_master_get_slave() */
                                       const char *vendor_name,
                                       /**< vendor name */
                                       const char *product_name,
                                       /**< product name */
                                       void **data_ptr,
                                       /**< address of the process data
                                          pointer */
                                       const char *field_name,
                                       /**< data field name */
                                       unsigned int field_index,
                                       /**< offset of data fields with
                                          \a field_type  */
                                       unsigned int field_count
                                       /**< number of data fields (with
                                          the same type) to register */
                                       )
{
    ec_slave_t *slave;
    const ec_slave_type_t *type;
    ec_master_t *master;
    const ec_sync_t *sync;
    const ec_field_t *field;
    unsigned int field_counter, i, j, orig_field_index, orig_field_count;
    uint32_t field_offset;

    master = domain->master;

    // translate address
    if (!(slave = ecrt_master_get_slave(master, address))) return NULL;

    if (!(type = slave->type)) {
        EC_ERR("Slave \"%s\" (position %i) has unknown type!\n", address,
               slave->ring_position);
        return NULL;
    }

    if (strcmp(vendor_name, type->vendor_name) ||
        strcmp(product_name, type->product_name)) {
        EC_ERR("Invalid slave type at position %i - Requested: \"%s %s\","
               " found: \"%s %s\".\n", slave->ring_position, vendor_name,
               product_name, type->vendor_name, type->product_name);
        return NULL;
    }

    if (!data_ptr) {
        // data_ptr is NULL => mark slave as "registered" (do not warn)
        slave->registered = 1;
    }

    if (!field_count) field_count = 1;
    orig_field_index = field_index;
    orig_field_count = field_count;

    field_counter = 0;
    for (i = 0; type->sync_managers[i]; i++) {
        sync = type->sync_managers[i];
        field_offset = 0;
        for (j = 0; sync->fields[j]; j++) {
            field = sync->fields[j];
            if (!strcmp(field->name, field_name)) {
                if (field_counter++ == field_index) {
                    if (data_ptr)
                        ec_domain_reg_field(domain, slave, sync, field_offset,
                                            data_ptr++);
                    if (!(--field_count)) return slave;
                    field_index++;
                }
            }
            field_offset += field->size;
        }
    }

    EC_ERR("Slave %i (\"%s %s\") registration mismatch: Field \"%s\","
           " index %i, count %i.\n", slave->ring_position, vendor_name,
           product_name, field_name, orig_field_index, orig_field_count);
    return NULL;
}

/*****************************************************************************/

/**
   Registeres a bunch of data fields.
   Caution! The list has to be terminated with a NULL structure ({})!
   \return 0 in case of success, else < 0
   \ingroup RealtimeInterface
*/

int ecrt_domain_register_field_list(ec_domain_t *domain,
                                    /**< EtherCAT domain */
                                    const ec_field_init_t *fields
                                    /**< array of data field registrations */
                                    )
{
    const ec_field_init_t *field;

    for (field = fields; field->slave_address; field++)
        if (!ecrt_domain_register_field(domain, field->slave_address,
                                        field->vendor_name,
                                        field->product_name, field->data_ptr,
                                        field->field_name, field->field_index,
                                        field->field_count))
            return -1;

    return 0;
}

/*****************************************************************************/

/**
   Places all process data commands in the masters command queue.
   \ingroup RealtimeInterface
*/

void ecrt_domain_queue(ec_domain_t *domain /**< EtherCAT domain */)
{
    ec_command_t *command;

    list_for_each_entry(command, &domain->commands, list) {
        ec_master_queue_command(domain->master, command);
    }
}

/*****************************************************************************/

/**
   Processes received process data.
   \ingroup RealtimeInterface
*/

void ecrt_domain_process(ec_domain_t *domain /**< EtherCAT domain */)
{
    unsigned int working_counter_sum;
    ec_command_t *command;

    working_counter_sum = 0;

    list_for_each_entry(command, &domain->commands, list) {
        if (command->state == EC_CMD_RECEIVED) {
            working_counter_sum += command->working_counter;
        }
    }

    ec_domain_response_count(domain, working_counter_sum);
}

/*****************************************************************************/

/**
   Returns the state of a domain.
   \return 0 if all commands were received, else -1.
   \ingroup RealtimeInterface
*/

int ecrt_domain_state(ec_domain_t *domain /**< EtherCAT domain */)
{
    ec_command_t *command;

    list_for_each_entry(command, &domain->commands, list) {
        if (command->state != EC_CMD_RECEIVED) return -1;
    }

    return 0;
}

/*****************************************************************************/

/** \cond */

EXPORT_SYMBOL(ecrt_domain_register_field);
EXPORT_SYMBOL(ecrt_domain_register_field_list);
EXPORT_SYMBOL(ecrt_domain_queue);
EXPORT_SYMBOL(ecrt_domain_process);
EXPORT_SYMBOL(ecrt_domain_state);

/** \endcond */

/*****************************************************************************/
