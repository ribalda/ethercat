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
   EtherCAT master methods.
*/

/*****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "../include/ecrt.h"
#include "globals.h"
#include "master.h"
#include "slave.h"
#include "types.h"
#include "device.h"
#include "command.h"
#include "ethernet.h"

/*****************************************************************************/

void ec_master_freerun(void *);
void ec_master_eoe_run(unsigned long);
ssize_t ec_show_master_attribute(struct kobject *, struct attribute *, char *);
ssize_t ec_store_master_attribute(struct kobject *, struct attribute *,
                                  const char *, size_t);

/*****************************************************************************/

/** \cond */

EC_SYSFS_READ_ATTR(slave_count);
EC_SYSFS_READ_ATTR(mode);
EC_SYSFS_READ_WRITE_ATTR(eeprom_write_enable);

static struct attribute *ec_def_attrs[] = {
    &attr_slave_count,
    &attr_mode,
    &attr_eeprom_write_enable,
    NULL,
};

static struct sysfs_ops ec_sysfs_ops = {
    .show = &ec_show_master_attribute,
    .store = ec_store_master_attribute
};

static struct kobj_type ktype_ec_master = {
    .release = ec_master_clear,
    .sysfs_ops = &ec_sysfs_ops,
    .default_attrs = ec_def_attrs
};

/** \endcond */

/*****************************************************************************/

/**
   Master constructor.
   \return 0 in case of success, else < 0
*/

int ec_master_init(ec_master_t *master, /**< EtherCAT master */
                   unsigned int index, /**< master index */
                   unsigned int eoe_devices /**< number of EoE devices */
                   )
{
    ec_eoe_t *eoe, *next_eoe;
    unsigned int i;

    EC_INFO("Initializing master %i.\n", index);

    master->index = index;
    master->device = NULL;
    master->reserved = 0;
    INIT_LIST_HEAD(&master->slaves);
    INIT_LIST_HEAD(&master->command_queue);
    INIT_LIST_HEAD(&master->domains);
    INIT_LIST_HEAD(&master->eoe_handlers);
    ec_command_init(&master->simple_command);
    INIT_WORK(&master->freerun_work, ec_master_freerun, (void *) master);
    init_timer(&master->eoe_timer);
    master->eoe_timer.function = ec_master_eoe_run;
    master->eoe_timer.data = (unsigned long) master;
    master->internal_lock = SPIN_LOCK_UNLOCKED;
    master->eoe_running = 0;

    // create workqueue
    if (!(master->workqueue = create_singlethread_workqueue("EtherCAT"))) {
        EC_ERR("Failed to create master workqueue.\n");
        goto out_return;
    }

    // create EoE handlers
    for (i = 0; i < eoe_devices; i++) {
        if (!(eoe = (ec_eoe_t *) kmalloc(sizeof(ec_eoe_t), GFP_KERNEL))) {
            EC_ERR("Failed to allocate EoE-Object.\n");
            goto out_clear_eoe;
        }
        if (ec_eoe_init(eoe)) {
            kfree(eoe);
            goto out_clear_eoe;
        }
        list_add_tail(&eoe->list, &master->eoe_handlers);
    }

    // create state machine object
    if (ec_fsm_init(&master->fsm, master)) goto out_clear_eoe;

    // init kobject and add it to the hierarchy
    memset(&master->kobj, 0x00, sizeof(struct kobject));
    kobject_init(&master->kobj);
    master->kobj.ktype = &ktype_ec_master;
    if (kobject_set_name(&master->kobj, "ethercat%i", index)) {
        EC_ERR("Failed to set kobj name.\n");
        kobject_put(&master->kobj);
        return -1;
    }

    ec_master_reset(master);
    return 0;

 out_clear_eoe:
    list_for_each_entry_safe(eoe, next_eoe, &master->eoe_handlers, list) {
        list_del(&eoe->list);
        ec_eoe_clear(eoe);
        kfree(eoe);
    }
    destroy_workqueue(master->workqueue);
 out_return:
    return -1;
}

/*****************************************************************************/

/**
   Master destructor.
   Removes all pending commands, clears the slave list, clears all domains
   and frees the device.
*/

void ec_master_clear(struct kobject *kobj /**< kobject of the master */)
{
    ec_master_t *master = container_of(kobj, ec_master_t, kobj);
    ec_eoe_t *eoe, *next_eoe;

    EC_INFO("Clearing master %i...\n", master->index);

    ec_master_reset(master);
    ec_fsm_clear(&master->fsm);
    ec_command_clear(&master->simple_command);
    destroy_workqueue(master->workqueue);

    // clear EoE objects
    list_for_each_entry_safe(eoe, next_eoe, &master->eoe_handlers, list) {
        list_del(&eoe->list);
        ec_eoe_clear(eoe);
        kfree(eoe);
    }

    if (master->device) {
        ec_device_clear(master->device);
        kfree(master->device);
    }

    EC_INFO("Master %i cleared.\n", master->index);
}

/*****************************************************************************/

/**
   Resets the master.
   Note: This function has to be called, everytime ec_master_release() is
   called, to free the slave list, domains etc.
*/

void ec_master_reset(ec_master_t *master /**< EtherCAT master */)
{
    ec_command_t *command, *next_c;
    ec_domain_t *domain, *next_d;

    ec_master_eoe_stop(master);
    ec_master_freerun_stop(master);
    ec_master_clear_slaves(master);

    // empty command queue
    list_for_each_entry_safe(command, next_c, &master->command_queue, queue) {
        list_del_init(&command->queue);
        command->state = EC_CMD_ERROR;
    }

    // clear domains
    list_for_each_entry_safe(domain, next_d, &master->domains, list) {
        list_del(&domain->list);
        kobject_del(&domain->kobj);
        kobject_put(&domain->kobj);
    }

    master->command_index = 0;
    master->debug_level = 0;
    master->timeout = 500; // 500us

    master->stats.timeouts = 0;
    master->stats.delayed = 0;
    master->stats.corrupted = 0;
    master->stats.unmatched = 0;
    master->stats.t_last = 0;

    master->mode = EC_MASTER_MODE_IDLE;

    master->request_cb = NULL;
    master->release_cb = NULL;
    master->cb_data = NULL;

    master->eeprom_write_enable = 0;

    ec_fsm_reset(&master->fsm);
}

/*****************************************************************************/

/**
   Clears all slaves.
*/

void ec_master_clear_slaves(ec_master_t *master)
{
    ec_slave_t *slave, *next_slave;

    list_for_each_entry_safe(slave, next_slave, &master->slaves, list) {
        list_del(&slave->list);
        kobject_del(&slave->kobj);
        kobject_put(&slave->kobj);
    }
    master->slave_count = 0;
}

/*****************************************************************************/

/**
   Places a command in the command queue.
*/

void ec_master_queue_command(ec_master_t *master, /**< EtherCAT master */
                             ec_command_t *command /**< command */
                             )
{
    ec_command_t *queued_command;

    // check, if the command is already queued
    list_for_each_entry(queued_command, &master->command_queue, queue) {
        if (queued_command == command) {
            command->state = EC_CMD_QUEUED;
            if (unlikely(master->debug_level))
                EC_WARN("command already queued.\n");
            return;
        }
    }

    list_add_tail(&command->queue, &master->command_queue);
    command->state = EC_CMD_QUEUED;
}

/*****************************************************************************/

/**
   Sends the commands in the queue.
   \return 0 in case of success, else < 0
*/

void ec_master_send_commands(ec_master_t *master /**< EtherCAT master */)
{
    ec_command_t *command;
    size_t command_size;
    uint8_t *frame_data, *cur_data;
    void *follows_word;
    cycles_t t_start, t_end;
    unsigned int frame_count, more_commands_waiting;

    frame_count = 0;
    t_start = get_cycles();

    if (unlikely(master->debug_level > 0))
        EC_DBG("ec_master_send_commands\n");

    do {
        // fetch pointer to transmit socket buffer
        frame_data = ec_device_tx_data(master->device);
        cur_data = frame_data + EC_FRAME_HEADER_SIZE;
        follows_word = NULL;
        more_commands_waiting = 0;

        // fill current frame with commands
        list_for_each_entry(command, &master->command_queue, queue) {
            if (command->state != EC_CMD_QUEUED) continue;

            // does the current command fit in the frame?
            command_size = EC_COMMAND_HEADER_SIZE + command->data_size
                + EC_COMMAND_FOOTER_SIZE;
            if (cur_data - frame_data + command_size > ETH_DATA_LEN) {
                more_commands_waiting = 1;
                break;
            }

            command->state = EC_CMD_SENT;
            command->t_sent = t_start;
            command->index = master->command_index++;

            if (unlikely(master->debug_level > 0))
                EC_DBG("adding command 0x%02X\n", command->index);

            // set "command following" flag in previous frame
            if (follows_word)
                EC_WRITE_U16(follows_word, EC_READ_U16(follows_word) | 0x8000);

            // EtherCAT command header
            EC_WRITE_U8 (cur_data,     command->type);
            EC_WRITE_U8 (cur_data + 1, command->index);
            EC_WRITE_U32(cur_data + 2, command->address.logical);
            EC_WRITE_U16(cur_data + 6, command->data_size & 0x7FF);
            EC_WRITE_U16(cur_data + 8, 0x0000);
            follows_word = cur_data + 6;
            cur_data += EC_COMMAND_HEADER_SIZE;

            // EtherCAT command data
            memcpy(cur_data, command->data, command->data_size);
            cur_data += command->data_size;

            // EtherCAT command footer
            EC_WRITE_U16(cur_data, 0x0000); // reset working counter
            cur_data += EC_COMMAND_FOOTER_SIZE;
        }

        if (cur_data - frame_data == EC_FRAME_HEADER_SIZE) {
            if (unlikely(master->debug_level > 0))
                EC_DBG("nothing to send.\n");
            break;
        }

        // EtherCAT frame header
        EC_WRITE_U16(frame_data, ((cur_data - frame_data
                                   - EC_FRAME_HEADER_SIZE) & 0x7FF) | 0x1000);

        // pad frame
        while (cur_data - frame_data < ETH_ZLEN - ETH_HLEN)
            EC_WRITE_U8(cur_data++, 0x00);

        if (unlikely(master->debug_level > 0))
            EC_DBG("frame size: %i\n", cur_data - frame_data);

        // send frame
        ec_device_send(master->device, cur_data - frame_data);
        frame_count++;
    }
    while (more_commands_waiting);

    if (unlikely(master->debug_level > 0)) {
        t_end = get_cycles();
        EC_DBG("ec_master_send_commands sent %i frames in %ius.\n",
               frame_count, (u32) (t_end - t_start) * 1000 / cpu_khz);
    }
}

/*****************************************************************************/

/**
   Processes a received frame.
   This function is called by the network driver for every received frame.
   \return 0 in case of success, else < 0
*/

void ec_master_receive(ec_master_t *master, /**< EtherCAT master */
                       const uint8_t *frame_data, /**< received data */
                       size_t size /**< size of the received data */
                       )
{
    size_t frame_size, data_size;
    uint8_t command_type, command_index;
    unsigned int cmd_follows, matched;
    const uint8_t *cur_data;
    ec_command_t *command;

    if (unlikely(size < EC_FRAME_HEADER_SIZE)) {
        master->stats.corrupted++;
        ec_master_output_stats(master);
        return;
    }

    cur_data = frame_data;

    // check length of entire frame
    frame_size = EC_READ_U16(cur_data) & 0x07FF;
    cur_data += EC_FRAME_HEADER_SIZE;

    if (unlikely(frame_size > size)) {
        master->stats.corrupted++;
        ec_master_output_stats(master);
        return;
    }

    cmd_follows = 1;
    while (cmd_follows) {
        // process command header
        command_type  = EC_READ_U8 (cur_data);
        command_index = EC_READ_U8 (cur_data + 1);
        data_size     = EC_READ_U16(cur_data + 6) & 0x07FF;
        cmd_follows   = EC_READ_U16(cur_data + 6) & 0x8000;
        cur_data += EC_COMMAND_HEADER_SIZE;

        if (unlikely(cur_data - frame_data
                     + data_size + EC_COMMAND_FOOTER_SIZE > size)) {
            master->stats.corrupted++;
            ec_master_output_stats(master);
            return;
        }

        // search for matching command in the queue
        matched = 0;
        list_for_each_entry(command, &master->command_queue, queue) {
            if (command->state == EC_CMD_SENT
                && command->type == command_type
                && command->index == command_index
                && command->data_size == data_size) {
                matched = 1;
                break;
            }
        }

        // no matching command was found
        if (!matched) {
            master->stats.unmatched++;
            ec_master_output_stats(master);
            cur_data += data_size + EC_COMMAND_FOOTER_SIZE;
            continue;
        }

        // copy received data in the command memory
        memcpy(command->data, cur_data, data_size);
        cur_data += data_size;

        // set the command's working counter
        command->working_counter = EC_READ_U16(cur_data);
        cur_data += EC_COMMAND_FOOTER_SIZE;

        // dequeue the received command
        command->state = EC_CMD_RECEIVED;
        list_del_init(&command->queue);
    }
}

/*****************************************************************************/

/**
   Sends a single command and waits for its reception.
   If the slave doesn't respond, the command is sent again.
   \return 0 in case of success, else < 0
*/

int ec_master_simple_io(ec_master_t *master, /**< EtherCAT master */
                        ec_command_t *command /**< command */
                        )
{
    unsigned int response_tries_left;

    response_tries_left = 10;

    while (1)
    {
        ec_master_queue_command(master, command);
        ecrt_master_sync_io(master);

        if (command->state == EC_CMD_RECEIVED) {
            if (likely(command->working_counter))
                return 0;
        }
        else if (command->state == EC_CMD_TIMEOUT) {
            EC_ERR("Simple-IO TIMEOUT!\n");
            return -1;
        }
        else if (command->state == EC_CMD_ERROR) {
            EC_ERR("Simple-IO command error!\n");
            return -1;
        }

        // no direct response, wait a little bit...
        udelay(100);

        if (unlikely(--response_tries_left)) {
            EC_ERR("No response in simple-IO!\n");
            return -1;
        }
    }
}

/*****************************************************************************/

/**
   Scans the EtherCAT bus for slaves.
   Creates a list of slave structures for further processing.
   \return 0 in case of success, else < 0
*/

int ec_master_bus_scan(ec_master_t *master /**< EtherCAT master */)
{
    ec_slave_t *slave;
    ec_slave_ident_t *ident;
    ec_command_t *command;
    unsigned int i;
    uint16_t coupler_index, coupler_subindex;
    uint16_t reverse_coupler_index, current_coupler_index;

    if (!list_empty(&master->slaves)) {
        EC_ERR("Slave scan already done!\n");
        return -1;
    }

    command = &master->simple_command;

    // determine number of slaves on bus
    if (ec_command_brd(command, 0x0000, 4)) return -1;
    if (unlikely(ec_master_simple_io(master, command))) return -1;
    master->slave_count = command->working_counter;
    EC_INFO("Found %i slaves on bus.\n", master->slave_count);

    if (!master->slave_count) return 0;

    // init slaves
    for (i = 0; i < master->slave_count; i++) {
        if (!(slave =
              (ec_slave_t *) kmalloc(sizeof(ec_slave_t), GFP_KERNEL))) {
            EC_ERR("Failed to allocate slave %i!\n", i);
            goto out_free;
        }

        if (ec_slave_init(slave, master, i, i + 1)) goto out_free;

        if (kobject_add(&slave->kobj)) {
            EC_ERR("Failed to add kobject.\n");
            kobject_put(&slave->kobj); // free
            goto out_free;
        }

        list_add_tail(&slave->list, &master->slaves);
    }

    coupler_index = 0;
    reverse_coupler_index = 0xFFFF;
    current_coupler_index = 0x3FFF;
    coupler_subindex = 0;

    // for every slave on the bus
    list_for_each_entry(slave, &master->slaves, list) {

        // write station address
        if (ec_command_apwr(command, slave->ring_position, 0x0010,
                            sizeof(uint16_t))) goto out_free;
        EC_WRITE_U16(command->data, slave->station_address);
        if (unlikely(ec_master_simple_io(master, command))) {
            EC_ERR("Writing station address failed on slave %i!\n",
                   slave->ring_position);
            goto out_free;
        }

        // fetch all slave information
        if (ec_slave_fetch(slave)) goto out_free;

        // search for identification in "database"
        ident = slave_idents;
        while (ident->type) {
            if (unlikely(ident->vendor_id == slave->sii_vendor_id
                         && ident->product_code == slave->sii_product_code)) {
                slave->type = ident->type;
                break;
            }
            ident++;
        }

        if (!slave->type) {
            EC_WARN("Unknown slave device (vendor 0x%08X, code 0x%08X) at"
                    " position %i.\n", slave->sii_vendor_id,
                    slave->sii_product_code, slave->ring_position);
        }
        else if (slave->type->special == EC_TYPE_BUS_COUPLER) {
            if (slave->sii_alias)
                current_coupler_index = reverse_coupler_index--;
            else
                current_coupler_index = coupler_index++;
            coupler_subindex = 0;
        }

        slave->coupler_index = current_coupler_index;
        slave->coupler_subindex = coupler_subindex;
        coupler_subindex++;
    }

    return 0;

 out_free:
    ec_master_clear_slaves(master);
    return -1;
}

/*****************************************************************************/

/**
   Output statistics in cyclic mode.
   This function outputs statistical data on demand, but not more often than
   necessary. The output happens at most once a second.
*/

void ec_master_output_stats(ec_master_t *master /**< EtherCAT master */)
{
    cycles_t t_now = get_cycles();

    if (unlikely((u32) (t_now - master->stats.t_last) / cpu_khz > 1000)) {
        if (master->stats.timeouts) {
            EC_WARN("%i commands TIMED OUT!\n", master->stats.timeouts);
            master->stats.timeouts = 0;
        }
        if (master->stats.delayed) {
            EC_WARN("%i frame(s) DELAYED!\n", master->stats.delayed);
            master->stats.delayed = 0;
        }
        if (master->stats.corrupted) {
            EC_WARN("%i frame(s) CORRUPTED!\n", master->stats.corrupted);
            master->stats.corrupted = 0;
        }
        if (master->stats.unmatched) {
            EC_WARN("%i command(s) UNMATCHED!\n", master->stats.unmatched);
            master->stats.unmatched = 0;
        }
        master->stats.t_last = t_now;
    }
}

/*****************************************************************************/

/**
   Starts the Free-Run mode.
*/

void ec_master_freerun_start(ec_master_t *master /**< EtherCAT master */)
{
    if (master->mode == EC_MASTER_MODE_FREERUN) return;

    if (master->mode == EC_MASTER_MODE_RUNNING) {
        EC_ERR("ec_master_freerun_start: Master already running!\n");
        return;
    }

    EC_INFO("Starting Free-Run mode.\n");

    master->mode = EC_MASTER_MODE_FREERUN;
    ec_fsm_reset(&master->fsm);
    queue_delayed_work(master->workqueue, &master->freerun_work, 1);
}

/*****************************************************************************/

/**
   Stops the Free-Run mode.
*/

void ec_master_freerun_stop(ec_master_t *master /**< EtherCAT master */)
{
    if (master->mode != EC_MASTER_MODE_FREERUN) return;

    ec_master_eoe_stop(master);

    EC_INFO("Stopping Free-Run mode.\n");

    if (!cancel_delayed_work(&master->freerun_work)) {
        flush_workqueue(master->workqueue);
    }

    ec_master_clear_slaves(master);
    master->mode = EC_MASTER_MODE_IDLE;
}

/*****************************************************************************/

/**
   Free-Run mode function.
*/

void ec_master_freerun(void *data /**< master pointer */)
{
    ec_master_t *master = (ec_master_t *) data;

    // aquire master lock
    spin_lock_bh(&master->internal_lock);

    ecrt_master_async_receive(master);

    // execute master state machine
    ec_fsm_execute(&master->fsm);

    ecrt_master_async_send(master);

    // release master lock
    spin_unlock_bh(&master->internal_lock);

    queue_delayed_work(master->workqueue, &master->freerun_work, 1);
}

/*****************************************************************************/

/**
   Initializes a sync manager configuration page.
   The referenced memory (\a data) must be at least EC_SYNC_SIZE bytes.
*/

void ec_sync_config(const ec_sync_t *sync, /**< sync manager */
                    uint8_t *data /**> configuration memory */
                    )
{
    EC_WRITE_U16(data,     sync->physical_start_address);
    EC_WRITE_U16(data + 2, sync->size);
    EC_WRITE_U8 (data + 4, sync->control_byte);
    EC_WRITE_U8 (data + 5, 0x00); // status byte (read only)
    EC_WRITE_U16(data + 6, 0x0001); // enable
}

/*****************************************************************************/

/**
   Initializes a sync manager configuration page with EEPROM data.
   The referenced memory (\a data) must be at least EC_SYNC_SIZE bytes.
*/

void ec_eeprom_sync_config(const ec_eeprom_sync_t *sync, /**< sync manager */
                           uint8_t *data /**> configuration memory */
                           )
{
    EC_WRITE_U16(data,     sync->physical_start_address);
    EC_WRITE_U16(data + 2, sync->length);
    EC_WRITE_U8 (data + 4, sync->control_register);
    EC_WRITE_U8 (data + 5, 0x00); // status byte (read only)
    EC_WRITE_U16(data + 6, sync->enable ? 0x0001 : 0x0000); // enable
}

/*****************************************************************************/

/**
   Initializes an FMMU configuration page.
   The referenced memory (\a data) must be at least EC_FMMU_SIZE bytes.
*/

void ec_fmmu_config(const ec_fmmu_t *fmmu, /**< FMMU */
                    uint8_t *data /**> configuration memory */
                    )
{
    EC_WRITE_U32(data,      fmmu->logical_start_address);
    EC_WRITE_U16(data + 4,  fmmu->sync->size);
    EC_WRITE_U8 (data + 6,  0x00); // logical start bit
    EC_WRITE_U8 (data + 7,  0x07); // logical end bit
    EC_WRITE_U16(data + 8,  fmmu->sync->physical_start_address);
    EC_WRITE_U8 (data + 10, 0x00); // physical start bit
    EC_WRITE_U8 (data + 11, (fmmu->sync->control_byte & 0x04) ? 0x02 : 0x01);
    EC_WRITE_U16(data + 12, 0x0001); // enable
    EC_WRITE_U16(data + 14, 0x0000); // reserved
}

/*****************************************************************************/

/**
   Formats attribute data for SysFS read access.
   \return number of bytes to read
*/

ssize_t ec_show_master_attribute(struct kobject *kobj, /**< kobject */
                                 struct attribute *attr, /**< attribute */
                                 char *buffer /**< memory to store data */
                                 )
{
    ec_master_t *master = container_of(kobj, ec_master_t, kobj);

    if (attr == &attr_slave_count) {
        return sprintf(buffer, "%i\n", master->slave_count);
    }
    else if (attr == &attr_mode) {
        switch (master->mode) {
            case EC_MASTER_MODE_IDLE:
                return sprintf(buffer, "IDLE\n");
            case EC_MASTER_MODE_FREERUN:
                return sprintf(buffer, "FREERUN\n");
            case EC_MASTER_MODE_RUNNING:
                return sprintf(buffer, "RUNNING\n");
        }
    }

    return 0;
}

/*****************************************************************************/

/**
   Formats attribute data for SysFS write access.
   \return number of bytes processed, or negative error code
*/

ssize_t ec_store_master_attribute(struct kobject *kobj, /**< slave's kobject */
                                  struct attribute *attr, /**< attribute */
                                  const char *buffer, /**< memory with data */
                                  size_t size /**< size of data to store */
                                  )
{
    ec_master_t *master = container_of(kobj, ec_master_t, kobj);

    if (attr == &attr_eeprom_write_enable) {
        if (!strcmp(buffer, "1\n")) {
            master->eeprom_write_enable = 1;
            EC_INFO("Slave EEPROM writing enabled.\n");
            return size;
        }
        else if (!strcmp(buffer, "0\n")) {
            master->eeprom_write_enable = 0;
            EC_INFO("Slave EEPROM writing disabled.\n");
            return size;
        }

        EC_ERR("Invalid value for eeprom_write_enable!\n");

        if (master->eeprom_write_enable) {
            master->eeprom_write_enable = 0;
            EC_INFO("Slave EEPROM writing disabled.\n");
        }
    }

    return -EINVAL;
}

/*****************************************************************************/

/**
   Starts Ethernet-over-EtherCAT processing for all EoE-capable slaves.
*/

void ec_master_eoe_start(ec_master_t *master /**< EtherCAT master */)
{
    ec_eoe_t *eoe;
    ec_slave_t *slave;
    unsigned int coupled, found;

    if (master->eoe_running) return;

    // decouple all EoE handlers
    list_for_each_entry(eoe, &master->eoe_handlers, list)
        eoe->slave = NULL;
    coupled = 0;

    // couple a free EoE handler to every EoE-capable slave
    list_for_each_entry(slave, &master->slaves, list) {
        if (!(slave->sii_mailbox_protocols & EC_MBOX_EOE)) continue;

        found = 0;
        list_for_each_entry(eoe, &master->eoe_handlers, list) {
            if (eoe->slave) continue;
            eoe->slave = slave;
            found = 1;
            coupled++;
            EC_INFO("Coupling device %s to slave %i.\n",
                    eoe->dev->name, slave->ring_position);
            if (eoe->opened) {
                slave->requested_state = EC_SLAVE_STATE_OP;
            }
            else {
                slave->requested_state = EC_SLAVE_STATE_INIT;
            }
            slave->state_error = 0;
            break;
        }

        if (!found) {
            EC_WARN("No EoE handler for slave %i!\n", slave->ring_position);
            slave->requested_state = EC_SLAVE_STATE_INIT;
            slave->state_error = 0;
        }
    }

    if (!coupled) {
        EC_INFO("No EoE handlers coupled.\n");
        return;
    }

    EC_INFO("Starting EoE processing.\n");
    master->eoe_running = 1;

    // start EoE processing
    master->eoe_timer.expires = jiffies + 10;
    add_timer(&master->eoe_timer);
    return;
}

/*****************************************************************************/

/**
   Stops the Ethernet-over-EtherCAT processing.
*/

void ec_master_eoe_stop(ec_master_t *master /**< EtherCAT master */)
{
    ec_eoe_t *eoe;

    if (!master->eoe_running) return;

    EC_INFO("Stopping EoE processing.\n");

    del_timer_sync(&master->eoe_timer);

    // decouple all EoE handlers
    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        if (eoe->slave) {
            eoe->slave->requested_state = EC_SLAVE_STATE_INIT;
            eoe->slave->state_error = 0;
            eoe->slave = NULL;
        }
    }

    master->eoe_running = 0;
}

/*****************************************************************************/
/**
   Does the Ethernet-over-EtherCAT processing.
*/

void ec_master_eoe_run(unsigned long data /**< master pointer */)
{
    ec_master_t *master = (ec_master_t *) data;
    ec_eoe_t *eoe;
    unsigned int active = 0;

    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        if (ec_eoe_active(eoe)) active++;
    }
    if (!active) goto queue_timer;

    // aquire master lock...
    if (master->mode == EC_MASTER_MODE_RUNNING) {
        // request_cb must return 0, if the lock has been aquired!
        if (master->request_cb(master->cb_data))
            goto queue_timer;
    }
    else if (master->mode == EC_MASTER_MODE_FREERUN) {
        spin_lock(&master->internal_lock);
    }
    else
        goto queue_timer;

    // actual EoE stuff
    ecrt_master_async_receive(master);
    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        ec_eoe_run(eoe);
    }
    ecrt_master_async_send(master);

    // release lock...
    if (master->mode == EC_MASTER_MODE_RUNNING) {
        master->release_cb(master->cb_data);
    }
    else if (master->mode == EC_MASTER_MODE_FREERUN) {
        spin_unlock(&master->internal_lock);
    }

 queue_timer:
    master->eoe_timer.expires += HZ / EC_EOE_FREQUENCY;
    add_timer(&master->eoe_timer);
}

/******************************************************************************
 *  Realtime interface
 *****************************************************************************/

/**
   Creates a domain.
   \return pointer to new domain on success, else NULL
   \ingroup RealtimeInterface
*/

ec_domain_t *ecrt_master_create_domain(ec_master_t *master /**< master */)
{
    ec_domain_t *domain, *last_domain;
    unsigned int index;

    if (!(domain = (ec_domain_t *) kmalloc(sizeof(ec_domain_t), GFP_KERNEL))) {
        EC_ERR("Error allocating domain memory!\n");
        goto out_return;
    }

    if (list_empty(&master->domains)) index = 0;
    else {
        last_domain = list_entry(master->domains.prev, ec_domain_t, list);
        index = last_domain->index + 1;
    }

    if (ec_domain_init(domain, master, index)) {
        EC_ERR("Failed to init domain.\n");
        goto out_return;
    }

    if (kobject_add(&domain->kobj)) {
        EC_ERR("Failed to add domain kobject.\n");
        goto out_put;
    }

    list_add_tail(&domain->list, &master->domains);
    return domain;

 out_put:
    kobject_put(&domain->kobj);
 out_return:
    return NULL;
}

/*****************************************************************************/

/**
   Configures all slaves and leads them to the OP state.
   Does the complete configuration and activation for all slaves. Sets sync
   managers and FMMUs, and does the appropriate transitions, until the slave
   is operational.
   \return 0 in case of success, else < 0
   \ingroup RealtimeInterface
*/

int ecrt_master_activate(ec_master_t *master /**< EtherCAT master */)
{
    unsigned int j;
    ec_slave_t *slave;
    ec_command_t *command;
    const ec_sync_t *sync;
    const ec_slave_type_t *type;
    const ec_fmmu_t *fmmu;
    uint32_t domain_offset;
    ec_domain_t *domain;
    ec_eeprom_sync_t *eeprom_sync, mbox_sync;

    command = &master->simple_command;

    // allocate all domains
    domain_offset = 0;
    list_for_each_entry(domain, &master->domains, list) {
        if (ec_domain_alloc(domain, domain_offset)) {
            EC_ERR("Failed to allocate domain %X!\n", (u32) domain);
            return -1;
        }
        domain_offset += domain->data_size;
    }

    // configure and activate slaves
    list_for_each_entry(slave, &master->slaves, list) {

        // change state to INIT
        if (unlikely(ec_slave_state_change(slave, EC_SLAVE_STATE_INIT)))
            return -1;

        // check for slave registration
        type = slave->type;
        if (!type)
            EC_WARN("Slave %i has unknown type!\n", slave->ring_position);

        // check and reset CRC fault counters
        ec_slave_check_crc(slave);

        // reset FMMUs
        if (slave->base_fmmu_count) {
            if (ec_command_npwr(command, slave->station_address, 0x0600,
                                EC_FMMU_SIZE * slave->base_fmmu_count))
                return -1;
            memset(command->data, 0x00, EC_FMMU_SIZE * slave->base_fmmu_count);
            if (unlikely(ec_master_simple_io(master, command))) {
                EC_ERR("Resetting FMMUs failed on slave %i!\n",
                       slave->ring_position);
                return -1;
            }
        }

        // reset sync managers
        if (slave->base_sync_count) {
            if (ec_command_npwr(command, slave->station_address, 0x0800,
                                EC_SYNC_SIZE * slave->base_sync_count))
                return -1;
            memset(command->data, 0x00, EC_SYNC_SIZE * slave->base_sync_count);
            if (unlikely(ec_master_simple_io(master, command))) {
                EC_ERR("Resetting sync managers failed on slave %i!\n",
                       slave->ring_position);
                return -1;
            }
        }

        // configure sync managers
        if (type) { // known slave type, take type's SM information
            for (j = 0; type->sync_managers[j] && j < EC_MAX_SYNC; j++) {
                sync = type->sync_managers[j];
                if (ec_command_npwr(command, slave->station_address,
                                    0x0800 + j * EC_SYNC_SIZE, EC_SYNC_SIZE))
                    return -1;
                ec_sync_config(sync, command->data);
                if (unlikely(ec_master_simple_io(master, command))) {
                    EC_ERR("Setting sync manager %i failed on slave %i!\n",
                           j, slave->ring_position);
                    return -1;
                }
            }
        }
        else if (slave->sii_mailbox_protocols) { // unknown type, but mailbox
            // does the device supply SM configurations in its EEPROM?
	    if (!list_empty(&slave->eeprom_syncs)) {
		list_for_each_entry(eeprom_sync, &slave->eeprom_syncs, list) {
		    EC_INFO("Sync manager %i...\n", eeprom_sync->index);
		    if (ec_command_npwr(command, slave->station_address,
					0x800 + eeprom_sync->index *
					EC_SYNC_SIZE,
					EC_SYNC_SIZE)) return -1;
		    ec_eeprom_sync_config(eeprom_sync, command->data);
		    if (unlikely(ec_master_simple_io(master, command))) {
			EC_ERR("Setting sync manager %i failed on slave %i!\n",
			       eeprom_sync->index, slave->ring_position);
			return -1;
		    }
		}
            }
	    else { // no sync manager information; guess mailbox settings
		mbox_sync.physical_start_address =
                    slave->sii_rx_mailbox_offset;
		mbox_sync.length = slave->sii_rx_mailbox_size;
		mbox_sync.control_register = 0x26;
		mbox_sync.enable = 1;
		if (ec_command_npwr(command, slave->station_address,
				    0x800,EC_SYNC_SIZE)) return -1;
		ec_eeprom_sync_config(&mbox_sync, command->data);
		if (unlikely(ec_master_simple_io(master, command))) {
		    EC_ERR("Setting sync manager 0 failed on slave %i!\n",
			   slave->ring_position);
		    return -1;
		}

		mbox_sync.physical_start_address =
                    slave->sii_tx_mailbox_offset;
		mbox_sync.length = slave->sii_tx_mailbox_size;
		mbox_sync.control_register = 0x22;
		mbox_sync.enable = 1;
		if (ec_command_npwr(command, slave->station_address,
				    0x808, EC_SYNC_SIZE)) return -1;
		ec_eeprom_sync_config(&mbox_sync, command->data);
		if (unlikely(ec_master_simple_io(master, command))) {
		    EC_ERR("Setting sync manager 1 failed on slave %i!\n",
			   slave->ring_position);
		    return -1;
		}
	    }
	    EC_INFO("Mailbox configured for unknown slave %i\n",
		    slave->ring_position);
        }

        // change state to PREOP
        if (unlikely(ec_slave_state_change(slave, EC_SLAVE_STATE_PREOP)))
            return -1;

        // stop activation here for slaves without type
        if (!type) continue;

        // slaves that are not registered are only brought into PREOP
        // state -> nice blinking and mailbox communication possible
        if (!slave->registered && !slave->type->special) {
            EC_WARN("Slave %i was not registered!\n", slave->ring_position);
            continue;
        }

        // configure FMMUs
        for (j = 0; j < slave->fmmu_count; j++) {
            fmmu = &slave->fmmus[j];
            if (ec_command_npwr(command, slave->station_address,
                                0x0600 + j * EC_FMMU_SIZE, EC_FMMU_SIZE))
                return -1;
            ec_fmmu_config(fmmu, command->data);
            if (unlikely(ec_master_simple_io(master, command))) {
                EC_ERR("Setting FMMU %i failed on slave %i!\n",
                       j, slave->ring_position);
                return -1;
            }
        }

        // change state to SAVEOP
        if (unlikely(ec_slave_state_change(slave, EC_SLAVE_STATE_SAVEOP)))
            return -1;

        // change state to OP
        if (unlikely(ec_slave_state_change(slave, EC_SLAVE_STATE_OP)))
            return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Resets all slaves to INIT state.
   \ingroup RealtimeInterface
*/

void ecrt_master_deactivate(ec_master_t *master /**< EtherCAT master */)
{
    ec_slave_t *slave;

    list_for_each_entry(slave, &master->slaves, list) {
        ec_slave_check_crc(slave);
        ec_slave_state_change(slave, EC_SLAVE_STATE_INIT);
    }
}


/*****************************************************************************/

/**
   Fetches the SDO dictionaries of all slaves.
   Slaves that do not support the CoE protocol are left out.
   \return 0 in case of success, else < 0
   \ingroup RealtimeInterface
*/

int ecrt_master_fetch_sdo_lists(ec_master_t *master /**< EtherCAT master */)
{
    ec_slave_t *slave;

    list_for_each_entry(slave, &master->slaves, list) {
        if (slave->sii_mailbox_protocols & EC_MBOX_COE) {
            if (unlikely(ec_slave_fetch_sdo_list(slave))) {
                EC_ERR("Failed to fetch SDO list on slave %i!\n",
                       slave->ring_position);
                return -1;
            }
        }
    }

    return 0;
}

/*****************************************************************************/

/**
   Sends queued commands and waits for their reception.
   \ingroup RealtimeInterface
*/

void ecrt_master_sync_io(ec_master_t *master /**< EtherCAT master */)
{
    ec_command_t *command, *n;
    unsigned int commands_sent;
    cycles_t t_start, t_end, t_timeout;

    // send commands
    ecrt_master_async_send(master);

    t_start = get_cycles(); // measure io time
    t_timeout = (cycles_t) master->timeout * (cpu_khz / 1000);

    while (1) { // active waiting
        ec_device_call_isr(master->device);

        t_end = get_cycles(); // take current time
        if (t_end - t_start >= t_timeout) break; // timeout!

        commands_sent = 0;
        list_for_each_entry_safe(command, n, &master->command_queue, queue) {
            if (command->state == EC_CMD_RECEIVED)
                list_del_init(&command->queue);
            else if (command->state == EC_CMD_SENT)
                commands_sent++;
        }

        if (!commands_sent) break;
    }

    // timeout; dequeue all commands
    list_for_each_entry_safe(command, n, &master->command_queue, queue) {
        switch (command->state) {
            case EC_CMD_SENT:
            case EC_CMD_QUEUED:
                command->state = EC_CMD_TIMEOUT;
                master->stats.timeouts++;
                ec_master_output_stats(master);
                break;
            case EC_CMD_RECEIVED:
                master->stats.delayed++;
                ec_master_output_stats(master);
                break;
            default:
                break;
        }
        list_del_init(&command->queue);
    }
}

/*****************************************************************************/

/**
   Asynchronous sending of commands.
   \ingroup RealtimeInterface
*/

void ecrt_master_async_send(ec_master_t *master /**< EtherCAT master */)
{
    ec_command_t *command, *n;

    if (unlikely(!master->device->link_state)) {
        // link is down, no command can be sent
        list_for_each_entry_safe(command, n, &master->command_queue, queue) {
            command->state = EC_CMD_ERROR;
            list_del_init(&command->queue);
        }

        // query link state
        ec_device_call_isr(master->device);
        return;
    }

    // send frames
    ec_master_send_commands(master);
}

/*****************************************************************************/

/**
   Asynchronous receiving of commands.
   \ingroup RealtimeInterface
*/

void ecrt_master_async_receive(ec_master_t *master /**< EtherCAT master */)
{
    ec_command_t *command, *next;
    cycles_t t_received, t_timeout;

    ec_device_call_isr(master->device);

    t_received = get_cycles();
    t_timeout = (cycles_t) master->timeout * (cpu_khz / 1000);

    // dequeue all received commands
    list_for_each_entry_safe(command, next, &master->command_queue, queue)
        if (command->state == EC_CMD_RECEIVED) list_del_init(&command->queue);

    // dequeue all commands that timed out
    list_for_each_entry_safe(command, next, &master->command_queue, queue) {
        switch (command->state) {
            case EC_CMD_SENT:
            case EC_CMD_QUEUED:
                if (t_received - command->t_sent > t_timeout) {
                    list_del_init(&command->queue);
                    command->state = EC_CMD_TIMEOUT;
                    master->stats.timeouts++;
                    ec_master_output_stats(master);
                }
                break;
            default:
                break;
        }
    }
}

/*****************************************************************************/

/**
   Prepares synchronous IO.
   Queues all domain commands and sends them. Then waits a certain time, so
   that ecrt_master_sasync_receive() can be called securely.
   \ingroup RealtimeInterface
*/

void ecrt_master_prepare_async_io(ec_master_t *master /**< EtherCAT master */)
{
    ec_domain_t *domain;
    cycles_t t_start, t_end, t_timeout;

    // queue commands of all domains
    list_for_each_entry(domain, &master->domains, list)
        ecrt_domain_queue(domain);

    ecrt_master_async_send(master);

    t_start = get_cycles(); // take sending time
    t_timeout = (cycles_t) master->timeout * (cpu_khz / 1000);

    // active waiting
    while (1) {
        t_end = get_cycles();
        if (t_end - t_start >= t_timeout) break;
    }
}

/*****************************************************************************/

/**
   Does all cyclic master work.
   \ingroup RealtimeInterface
*/

void ecrt_master_run(ec_master_t *master /**< EtherCAT master */)
{
    // output statistics
    ec_master_output_stats(master);

    // execute master state machine
    ec_fsm_execute(&master->fsm);
}

/*****************************************************************************/

/**
   Translates an ASCII coded bus-address to a slave pointer.
   These are the valid addressing schemes:
   - \a "X" = the X. slave on the bus,
   - \a "X:Y" = the Y. slave after the X. branch (bus coupler),
   - \a "#X" = the slave with alias X,
   - \a "#X:Y" = the Y. slave after the branch (bus coupler) with alias X.
   X and Y are zero-based indices and may be provided in hexadecimal or octal
   notation (with respective prefix).
   \return pointer to the slave on success, else NULL
   \ingroup RealtimeInterface
*/

ec_slave_t *ecrt_master_get_slave(const ec_master_t *master, /**< Master */
                                  const char *address /**< address string */
                                  )
{
    unsigned long first, second;
    char *remainder, *remainder2;
    unsigned int alias_requested, alias_found;
    ec_slave_t *alias_slave = NULL, *slave;

    if (!address || address[0] == 0) return NULL;

    alias_requested = 0;
    if (address[0] == '#') {
        alias_requested = 1;
        address++;
    }

    first = simple_strtoul(address, &remainder, 0);
    if (remainder == address) {
        EC_ERR("Slave address \"%s\" - First number empty!\n", address);
        return NULL;
    }

    if (alias_requested) {
        alias_found = 0;
        list_for_each_entry(alias_slave, &master->slaves, list) {
            if (alias_slave->sii_alias == first) {
                alias_found = 1;
                break;
            }
        }
        if (!alias_found) {
            EC_ERR("Slave address \"%s\" - Alias not found!\n", address);
            return NULL;
        }
    }

    if (!remainder[0]) { // absolute position
        if (alias_requested) {
            return alias_slave;
        }
        else {
            list_for_each_entry(slave, &master->slaves, list) {
                if (slave->ring_position == first) return slave;
            }
            EC_ERR("Slave address \"%s\" - Absolute position invalid!\n",
                   address);
        }
    }
    else if (remainder[0] == ':') { // field position
        remainder++;
        second = simple_strtoul(remainder, &remainder2, 0);

        if (remainder2 == remainder) {
            EC_ERR("Slave address \"%s\" - Second number empty!\n", address);
            return NULL;
        }

        if (remainder2[0]) {
            EC_ERR("Slave address \"%s\" - Invalid trailer!\n", address);
            return NULL;
        }

        if (alias_requested) {
            if (!alias_slave->type ||
                alias_slave->type->special != EC_TYPE_BUS_COUPLER) {
                EC_ERR("Slave address \"%s\": Alias slave must be bus coupler"
                       " in colon mode.\n", address);
                return NULL;
            }
            list_for_each_entry(slave, &master->slaves, list) {
                if (slave->coupler_index == alias_slave->coupler_index
                    && slave->coupler_subindex == second)
                    return slave;
            }
            EC_ERR("Slave address \"%s\" - Bus coupler %i has no %lu. slave"
                   " following!\n", address, alias_slave->ring_position,
                   second);
            return NULL;
        }
        else {
            list_for_each_entry(slave, &master->slaves, list) {
                if (slave->coupler_index == first
                    && slave->coupler_subindex == second) return slave;
            }
        }
    }
    else
        EC_ERR("Slave address \"%s\" - Invalid format!\n", address);

    return NULL;
}

/*****************************************************************************/

/**
   Sets the locking callbacks.
   The request_cb function must return zero, to allow another instance
   (the EoE process for example) to access the master. Non-zero means,
   that access is forbidden at this time.
   \ingroup RealtimeInterface
*/

void ecrt_master_callbacks(ec_master_t *master, /**< EtherCAT master */
                           int (*request_cb)(void *), /**< request lock CB */
                           void (*release_cb)(void *), /**< release lock CB */
                           void *cb_data /**< data parameter */
                           )
{
    master->request_cb = request_cb;
    master->release_cb = release_cb;
    master->cb_data = cb_data;
}

/*****************************************************************************/

/**
   Starts Ethernet-over-EtherCAT processing for all EoE-capable slaves.
   \ingroup RealtimeInterface
*/

int ecrt_master_start_eoe(ec_master_t *master /**< EtherCAT master */)
{
    if (!master->request_cb || !master->release_cb) {
        EC_ERR("EoE requires master callbacks to be set!\n");
        return -1;
    }

    ec_master_eoe_start(master);
    return 0;
}

/*****************************************************************************/

/**
   Sets the debug level of the master.
   The following levels are valid:
   - 1: only output positions marks and basic data
   - 2: additional frame data output
   \ingroup RealtimeInterface
*/

void ecrt_master_debug(ec_master_t *master, /**< EtherCAT master */
                       int level /**< debug level */
                       )
{
    if (level != master->debug_level) {
        master->debug_level = level;
        EC_INFO("Master debug level set to %i.\n", level);
    }
}

/*****************************************************************************/

/**
   Outputs all master information.
   Verbosity:
   - 0: Only slave types and positions
   - 1: with EEPROM contents
   - >1: with SDO dictionaries
   \ingroup RealtimeInterface
*/

void ecrt_master_print(const ec_master_t *master, /**< EtherCAT master */
                       unsigned int verbosity /**< verbosity level */
                       )
{
    ec_slave_t *slave;
    ec_eoe_t *eoe;

    EC_INFO("*** Begin master information ***\n");
    if (master->slave_count) {
        EC_INFO("Slave list:\n");
        list_for_each_entry(slave, &master->slaves, list)
            ec_slave_print(slave, verbosity);
    }
    if (!list_empty(&master->eoe_handlers)) {
        EC_INFO("Ethernet-over-EtherCAT (EoE) objects:\n");
        list_for_each_entry(eoe, &master->eoe_handlers, list) {
            ec_eoe_print(eoe);
        }
    }
    EC_INFO("*** End master information ***\n");
}

/*****************************************************************************/

/** \cond */

EXPORT_SYMBOL(ecrt_master_create_domain);
EXPORT_SYMBOL(ecrt_master_activate);
EXPORT_SYMBOL(ecrt_master_deactivate);
EXPORT_SYMBOL(ecrt_master_fetch_sdo_lists);
EXPORT_SYMBOL(ecrt_master_prepare_async_io);
EXPORT_SYMBOL(ecrt_master_sync_io);
EXPORT_SYMBOL(ecrt_master_async_send);
EXPORT_SYMBOL(ecrt_master_async_receive);
EXPORT_SYMBOL(ecrt_master_run);
EXPORT_SYMBOL(ecrt_master_callbacks);
EXPORT_SYMBOL(ecrt_master_start_eoe);
EXPORT_SYMBOL(ecrt_master_debug);
EXPORT_SYMBOL(ecrt_master_print);
EXPORT_SYMBOL(ecrt_master_get_slave);

/** \endcond */

/*****************************************************************************/
