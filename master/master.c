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
#include "device.h"
#include "datagram.h"
#include "ethernet.h"

/*****************************************************************************/

void ec_master_clear(struct kobject *);
void ec_master_sync_io(ec_master_t *);
void ec_master_idle_run(void *);
void ec_master_eoe_run(unsigned long);
ssize_t ec_show_master_attribute(struct kobject *, struct attribute *, char *);
ssize_t ec_store_master_attribute(struct kobject *, struct attribute *,
                                  const char *, size_t);

/*****************************************************************************/

/** \cond */

EC_SYSFS_READ_ATTR(info);
EC_SYSFS_READ_WRITE_ATTR(eeprom_write_enable);
EC_SYSFS_READ_WRITE_ATTR(debug_level);

static struct attribute *ec_def_attrs[] = {
    &attr_info,
    &attr_eeprom_write_enable,
    &attr_debug_level,
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
                   unsigned int eoeif_count, /**< number of EoE interfaces */
                   dev_t dev_num /**< number for XML cdev's */
                   )
{
    ec_eoe_t *eoe, *next_eoe;
    unsigned int i;

    EC_INFO("Initializing master %i.\n", index);

    master->index = index;
    master->device = NULL;
    init_MUTEX(&master->device_sem);
    atomic_set(&master->available, 1);
    INIT_LIST_HEAD(&master->slaves);
    INIT_LIST_HEAD(&master->datagram_queue);
    INIT_LIST_HEAD(&master->domains);
    INIT_LIST_HEAD(&master->eoe_handlers);
    INIT_WORK(&master->idle_work, ec_master_idle_run, (void *) master);
    init_timer(&master->eoe_timer);
    master->eoe_timer.function = ec_master_eoe_run;
    master->eoe_timer.data = (unsigned long) master;
    master->internal_lock = SPIN_LOCK_UNLOCKED;
    master->eoe_running = 0;
    master->eoe_checked = 0;
    for (i = 0; i < HZ; i++) {
        master->idle_cycle_times[i] = 0;
        master->eoe_cycle_times[i] = 0;
    }
    master->idle_cycle_time_pos = 0;
    master->eoe_cycle_time_pos = 0;

    // create workqueue
    if (!(master->workqueue = create_singlethread_workqueue("EtherCAT"))) {
        EC_ERR("Failed to create master workqueue.\n");
        goto out_return;
    }

    // init XML character device
    if (ec_xmldev_init(&master->xmldev, master, dev_num)) {
        EC_ERR("Failed to init XML character device.\n");
        goto out_clear_wq;
    }

    // create EoE handlers
    for (i = 0; i < eoeif_count; i++) {
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
    ec_xmldev_clear(&master->xmldev);
 out_clear_wq:
    destroy_workqueue(master->workqueue);
 out_return:
    return -1;
}

/*****************************************************************************/

/**
   Master destructor.
   Removes all pending datagrams, clears the slave list, clears all domains
   and frees the device.
*/

void ec_master_clear(struct kobject *kobj /**< kobject of the master */)
{
    ec_master_t *master = container_of(kobj, ec_master_t, kobj);
    ec_eoe_t *eoe, *next_eoe;

    EC_INFO("Clearing master %i...\n", master->index);

    ec_master_reset(master);
    ec_fsm_clear(&master->fsm);
    destroy_workqueue(master->workqueue);
    ec_xmldev_clear(&master->xmldev);

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
    ec_datagram_t *datagram, *next_c;
    ec_domain_t *domain, *next_d;

    ec_master_eoe_stop(master);
    ec_master_idle_stop(master);
    ec_master_clear_slaves(master);

    // empty datagram queue
    list_for_each_entry_safe(datagram, next_c,
                             &master->datagram_queue, queue) {
        datagram->state = EC_DATAGRAM_ERROR;
        list_del_init(&datagram->queue);
    }

    // clear domains
    list_for_each_entry_safe(domain, next_d, &master->domains, list) {
        list_del(&domain->list);
        kobject_del(&domain->kobj);
        kobject_put(&domain->kobj);
    }

    master->datagram_index = 0;
    master->debug_level = 0;

    master->stats.timeouts = 0;
    master->stats.corrupted = 0;
    master->stats.skipped = 0;
    master->stats.unmatched = 0;
    master->stats.output_jiffies = 0;

    master->mode = EC_MASTER_MODE_ORPHANED;

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
   Places a datagram in the datagram queue.
*/

void ec_master_queue_datagram(ec_master_t *master, /**< EtherCAT master */
                              ec_datagram_t *datagram /**< datagram */
                              )
{
    ec_datagram_t *queued_datagram;

    // check, if the datagram is already queued
    list_for_each_entry(queued_datagram, &master->datagram_queue, queue) {
        if (queued_datagram == datagram) {
            master->stats.skipped++;
            ec_master_output_stats(master);
            datagram->state = EC_DATAGRAM_QUEUED;
            return;
        }
    }

    list_add_tail(&datagram->queue, &master->datagram_queue);
    datagram->state = EC_DATAGRAM_QUEUED;
    datagram->cycles_queued = get_cycles();
}

/*****************************************************************************/

/**
   Sends the datagrams in the queue.
   \return 0 in case of success, else < 0
*/

void ec_master_send_datagrams(ec_master_t *master /**< EtherCAT master */)
{
    ec_datagram_t *datagram, *next;
    size_t datagram_size;
    uint8_t *frame_data, *cur_data;
    void *follows_word;
    cycles_t cycles_start, cycles_sent, cycles_end;
    unsigned long jiffies_sent;
    unsigned int frame_count, more_datagrams_waiting;
    struct list_head sent_datagrams;

    cycles_start = get_cycles();
    frame_count = 0;
    INIT_LIST_HEAD(&sent_datagrams);

    if (unlikely(master->debug_level > 1))
        EC_DBG("ec_master_send_datagrams\n");

    do {
        // fetch pointer to transmit socket buffer
        frame_data = ec_device_tx_data(master->device);
        cur_data = frame_data + EC_FRAME_HEADER_SIZE;
        follows_word = NULL;
        more_datagrams_waiting = 0;

        // fill current frame with datagrams
        list_for_each_entry(datagram, &master->datagram_queue, queue) {
            if (datagram->state != EC_DATAGRAM_QUEUED) continue;

            // does the current datagram fit in the frame?
            datagram_size = EC_DATAGRAM_HEADER_SIZE + datagram->data_size
                + EC_DATAGRAM_FOOTER_SIZE;
            if (cur_data - frame_data + datagram_size > ETH_DATA_LEN) {
                more_datagrams_waiting = 1;
                break;
            }

            list_add_tail(&datagram->sent, &sent_datagrams);
            datagram->index = master->datagram_index++;

            if (unlikely(master->debug_level > 1))
                EC_DBG("adding datagram 0x%02X\n", datagram->index);

            // set "datagram following" flag in previous frame
            if (follows_word)
                EC_WRITE_U16(follows_word, EC_READ_U16(follows_word) | 0x8000);

            // EtherCAT datagram header
            EC_WRITE_U8 (cur_data,     datagram->type);
            EC_WRITE_U8 (cur_data + 1, datagram->index);
            EC_WRITE_U32(cur_data + 2, datagram->address.logical);
            EC_WRITE_U16(cur_data + 6, datagram->data_size & 0x7FF);
            EC_WRITE_U16(cur_data + 8, 0x0000);
            follows_word = cur_data + 6;
            cur_data += EC_DATAGRAM_HEADER_SIZE;

            // EtherCAT datagram data
            memcpy(cur_data, datagram->data, datagram->data_size);
            cur_data += datagram->data_size;

            // EtherCAT datagram footer
            EC_WRITE_U16(cur_data, 0x0000); // reset working counter
            cur_data += EC_DATAGRAM_FOOTER_SIZE;
        }

        if (list_empty(&sent_datagrams)) {
            if (unlikely(master->debug_level > 1))
                EC_DBG("nothing to send.\n");
            break;
        }

        // EtherCAT frame header
        EC_WRITE_U16(frame_data, ((cur_data - frame_data
                                   - EC_FRAME_HEADER_SIZE) & 0x7FF) | 0x1000);

        // pad frame
        while (cur_data - frame_data < ETH_ZLEN - ETH_HLEN)
            EC_WRITE_U8(cur_data++, 0x00);

        if (unlikely(master->debug_level > 1))
            EC_DBG("frame size: %i\n", cur_data - frame_data);

        // send frame
        ec_device_send(master->device, cur_data - frame_data);
        cycles_sent = get_cycles();
        jiffies_sent = jiffies;

        // set datagram states and sending timestamps
        list_for_each_entry_safe(datagram, next, &sent_datagrams, sent) {
            datagram->state = EC_DATAGRAM_SENT;
            datagram->cycles_sent = cycles_sent;
            datagram->jiffies_sent = jiffies_sent;
            list_del_init(&datagram->sent); // empty list of sent datagrams
        }

        frame_count++;
    }
    while (more_datagrams_waiting);

    if (unlikely(master->debug_level > 1)) {
        cycles_end = get_cycles();
        EC_DBG("ec_master_send_datagrams sent %i frames in %ius.\n",
               frame_count,
               (unsigned int) (cycles_end - cycles_start) * 1000 / cpu_khz);
    }
}

/*****************************************************************************/

/**
   Processes a received frame.
   This function is called by the network driver for every received frame.
   \return 0 in case of success, else < 0
*/

void ec_master_receive_datagrams(ec_master_t *master, /**< EtherCAT master */
                                 const uint8_t *frame_data, /**< frame data */
                                 size_t size /**< size of the received data */
                                 )
{
    size_t frame_size, data_size;
    uint8_t datagram_type, datagram_index;
    unsigned int cmd_follows, matched;
    const uint8_t *cur_data;
    ec_datagram_t *datagram;

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
        // process datagram header
        datagram_type  = EC_READ_U8 (cur_data);
        datagram_index = EC_READ_U8 (cur_data + 1);
        data_size      = EC_READ_U16(cur_data + 6) & 0x07FF;
        cmd_follows    = EC_READ_U16(cur_data + 6) & 0x8000;
        cur_data += EC_DATAGRAM_HEADER_SIZE;

        if (unlikely(cur_data - frame_data
                     + data_size + EC_DATAGRAM_FOOTER_SIZE > size)) {
            master->stats.corrupted++;
            ec_master_output_stats(master);
            return;
        }

        // search for matching datagram in the queue
        matched = 0;
        list_for_each_entry(datagram, &master->datagram_queue, queue) {
            if (datagram->state == EC_DATAGRAM_SENT
                && datagram->type == datagram_type
                && datagram->index == datagram_index
                && datagram->data_size == data_size) {
                matched = 1;
                break;
            }
        }

        // no matching datagram was found
        if (!matched) {
            master->stats.unmatched++;
            ec_master_output_stats(master);
            cur_data += data_size + EC_DATAGRAM_FOOTER_SIZE;
            continue;
        }

        // copy received data into the datagram memory
        memcpy(datagram->data, cur_data, data_size);
        cur_data += data_size;

        // set the datagram's working counter
        datagram->working_counter = EC_READ_U16(cur_data);
        cur_data += EC_DATAGRAM_FOOTER_SIZE;

        // dequeue the received datagram
        datagram->state = EC_DATAGRAM_RECEIVED;
        datagram->cycles_received = master->device->cycles_isr;
        datagram->jiffies_received = master->device->jiffies_isr;
        list_del_init(&datagram->queue);
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
    ec_fsm_t *fsm = &master->fsm;

    ec_fsm_startup(fsm); // init startup state machine

    do {
        ec_fsm_execute(fsm);
        ec_master_sync_io(master);
    }
    while (ec_fsm_startup_running(fsm));

    if (!ec_fsm_startup_success(fsm)) {
        ec_master_clear_slaves(master);
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Output statistics in cyclic mode.
   This function outputs statistical data on demand, but not more often than
   necessary. The output happens at most once a second.
*/

void ec_master_output_stats(ec_master_t *master /**< EtherCAT master */)
{
    if (unlikely(jiffies - master->stats.output_jiffies >= HZ)) {
        master->stats.output_jiffies = jiffies;

        if (master->stats.timeouts) {
            EC_WARN("%i datagram%s TIMED OUT!\n", master->stats.timeouts,
                    master->stats.timeouts == 1 ? "" : "s");
            master->stats.timeouts = 0;
        }
        if (master->stats.corrupted) {
            EC_WARN("%i frame%s CORRUPTED!\n", master->stats.corrupted,
                    master->stats.corrupted == 1 ? "" : "s");
            master->stats.corrupted = 0;
        }
        if (master->stats.skipped) {
            EC_WARN("%i datagram%s SKIPPED!\n", master->stats.skipped,
                    master->stats.skipped == 1 ? "" : "s");
            master->stats.skipped = 0;
        }
        if (master->stats.unmatched) {
            EC_WARN("%i datagram%s UNMATCHED!\n", master->stats.unmatched,
                    master->stats.unmatched == 1 ? "" : "s");
            master->stats.unmatched = 0;
        }
    }
}

/*****************************************************************************/

/**
   Starts the Idle mode.
*/

void ec_master_idle_start(ec_master_t *master /**< EtherCAT master */)
{
    if (master->mode == EC_MASTER_MODE_IDLE) return;

    if (master->mode == EC_MASTER_MODE_OPERATION) {
        EC_ERR("ec_master_idle_start: Master already running!\n");
        return;
    }

    EC_INFO("Starting Idle mode.\n");

    master->mode = EC_MASTER_MODE_IDLE;
    ec_fsm_reset(&master->fsm);
    queue_delayed_work(master->workqueue, &master->idle_work, 1);
}

/*****************************************************************************/

/**
   Stops the Idle mode.
*/

void ec_master_idle_stop(ec_master_t *master /**< EtherCAT master */)
{
    if (master->mode != EC_MASTER_MODE_IDLE) return;

    ec_master_eoe_stop(master);

    EC_INFO("Stopping Idle mode.\n");
    master->mode = EC_MASTER_MODE_ORPHANED; // this is important for the
                                            // IDLE work function to not
                                            // queue itself again

    if (!cancel_delayed_work(&master->idle_work)) {
        flush_workqueue(master->workqueue);
    }

    ec_master_clear_slaves(master);
}

/*****************************************************************************/

/**
   Idle mode function.
*/

void ec_master_idle_run(void *data /**< master pointer */)
{
    ec_master_t *master = (ec_master_t *) data;
    cycles_t cycles_start, cycles_end;

    // aquire master lock
    spin_lock_bh(&master->internal_lock);

    cycles_start = get_cycles();
    ecrt_master_receive(master);

    // execute master state machine
    ec_fsm_execute(&master->fsm);

    ecrt_master_send(master);
    cycles_end = get_cycles();

    // release master lock
    spin_unlock_bh(&master->internal_lock);

    master->idle_cycle_times[master->idle_cycle_time_pos]
        = (u32) (cycles_end - cycles_start) * 1000 / cpu_khz;
    master->idle_cycle_time_pos++;
    master->idle_cycle_time_pos %= HZ;

    if (master->mode == EC_MASTER_MODE_IDLE)
        queue_delayed_work(master->workqueue, &master->idle_work, 1);
}

/*****************************************************************************/

/**
   Initializes a sync manager configuration page with EEPROM data.
   The referenced memory (\a data) must be at least EC_SYNC_SIZE bytes.
*/

void ec_sync_config(const ec_sii_sync_t *sync, /**< sync manager */
                    const ec_slave_t *slave, /**< EtherCAT slave */
                    uint8_t *data /**> configuration memory */
                    )
{
    size_t sync_size;

    sync_size = ec_slave_calc_sync_size(slave, sync);

    if (slave->master->debug_level) {
        EC_DBG("Slave %i, sync manager %i:\n", slave->ring_position,
               sync->index);
        EC_DBG("  Address: 0x%04X\n", sync->physical_start_address);
        EC_DBG("     Size: %i\n", sync_size);
        EC_DBG("  Control: 0x%02X\n", sync->control_register);
    }

    EC_WRITE_U16(data,     sync->physical_start_address);
    EC_WRITE_U16(data + 2, sync_size);
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
                    const ec_slave_t *slave, /**< EtherCAT slave */
                    uint8_t *data /**> configuration memory */
                    )
{
    size_t sync_size;

    sync_size = ec_slave_calc_sync_size(slave, fmmu->sync);

    EC_WRITE_U32(data,      fmmu->logical_start_address);
    EC_WRITE_U16(data + 4,  sync_size); // size of fmmu
    EC_WRITE_U8 (data + 6,  0x00); // logical start bit
    EC_WRITE_U8 (data + 7,  0x07); // logical end bit
    EC_WRITE_U16(data + 8,  fmmu->sync->physical_start_address);
    EC_WRITE_U8 (data + 10, 0x00); // physical start bit
    EC_WRITE_U8 (data + 11, ((fmmu->sync->control_register & 0x04)
                             ? 0x02 : 0x01));
    EC_WRITE_U16(data + 12, 0x0001); // enable
    EC_WRITE_U16(data + 14, 0x0000); // reserved
}

/*****************************************************************************/

/**
   Formats master information for SysFS read access.
   \return number of bytes written
*/

ssize_t ec_master_info(ec_master_t *master, /**< EtherCAT master */
                       char *buffer /**< memory to store data */
                       )
{
    off_t off = 0;
    ec_eoe_t *eoe;
    uint32_t cur, sum, min, max, pos, i;

    off += sprintf(buffer + off, "\nVersion: " EC_MASTER_VERSION);
    off += sprintf(buffer + off, "\nMode: ");
    switch (master->mode) {
        case EC_MASTER_MODE_ORPHANED:
            off += sprintf(buffer + off, "ORPHANED");
            break;
        case EC_MASTER_MODE_IDLE:
            off += sprintf(buffer + off, "IDLE");
            break;
        case EC_MASTER_MODE_OPERATION:
            off += sprintf(buffer + off, "OPERATION");
            break;
    }

    off += sprintf(buffer + off, "\nSlaves: %i\n",
                   master->slave_count);

    off += sprintf(buffer + off, "\nTiming (min/avg/max) [us]:\n");

    sum = 0;
    min = 0xFFFFFFFF;
    max = 0;
    pos = master->idle_cycle_time_pos;
    for (i = 0; i < HZ; i++) {
        cur = master->idle_cycle_times[(i + pos) % HZ];
        sum += cur;
        if (cur < min) min = cur;
        if (cur > max) max = cur;
    }
    off += sprintf(buffer + off, "  Idle cycle: %u / %u.%u / %u\n",
                   min, sum / HZ, (sum * 100 / HZ) % 100, max);

    sum = 0;
    min = 0xFFFFFFFF;
    max = 0;
    pos = master->eoe_cycle_time_pos;
    for (i = 0; i < HZ; i++) {
        cur = master->eoe_cycle_times[(i + pos) % HZ];
        sum += cur;
        if (cur < min) min = cur;
        if (cur > max) max = cur;
    }
    off += sprintf(buffer + off, "  EoE cycle: %u / %u.%u / %u\n",
                   min, sum / HZ, (sum * 100 / HZ) % 100, max);

    if (!list_empty(&master->eoe_handlers))
        off += sprintf(buffer + off, "\nEoE statistics (RX/TX) [bps]:\n");
    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        off += sprintf(buffer + off, "  %s: %u / %u (%u KB/s)\n",
                       eoe->dev->name, eoe->rx_rate, eoe->tx_rate,
                       ((eoe->rx_rate + eoe->tx_rate) / 8 + 512) / 1024);
    }

    off += sprintf(buffer + off, "\n");

    return off;
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

    if (attr == &attr_info) {
        return ec_master_info(master, buffer);
    }
    else if (attr == &attr_debug_level) {
        return sprintf(buffer, "%i\n", master->debug_level);
    }
    else if (attr == &attr_eeprom_write_enable) {
        return sprintf(buffer, "%i\n", master->eeprom_write_enable);
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
    else if (attr == &attr_debug_level) {
        if (!strcmp(buffer, "0\n")) {
            master->debug_level = 0;
        }
        else if (!strcmp(buffer, "1\n")) {
            master->debug_level = 1;
        }
        else if (!strcmp(buffer, "2\n")) {
            master->debug_level = 2;
        }
        else {
            EC_ERR("Invalid debug level value!\n");
            return -EINVAL;
        }

        EC_INFO("Master debug level set to %i.\n", master->debug_level);
        return size;
    }

    return -EINVAL;
}

/*****************************************************************************/

/**
   Starts Ethernet-over-EtherCAT processing on demand.
*/

void ec_master_eoe_start(ec_master_t *master /**< EtherCAT master */)
{
    ec_eoe_t *eoe;
    ec_slave_t *slave;
    unsigned int coupled, found;

    if (master->eoe_running || master->eoe_checked) return;

    master->eoe_checked = 1;

    // if the locking callbacks are not set in operation mode,
    // the EoE timer my not be started.
    if (master->mode == EC_MASTER_MODE_OPERATION
        && (!master->request_cb || !master->release_cb)) {
        EC_INFO("No EoE processing because of missing locking callbacks.\n");
        return;
    }

    // decouple all EoE handlers
    list_for_each_entry(eoe, &master->eoe_handlers, list)
        eoe->slave = NULL;

    // couple a free EoE handler to every EoE-capable slave
    coupled = 0;
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
            if (eoe->opened) slave->requested_state = EC_SLAVE_STATE_OP;
            else slave->requested_state = EC_SLAVE_STATE_INIT;
            slave->error_flag = 0;
            break;
        }

        if (!found) {
            EC_WARN("No EoE handler for slave %i!\n", slave->ring_position);
            slave->requested_state = EC_SLAVE_STATE_INIT;
            slave->error_flag = 0;
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

    master->eoe_checked = 0;

    if (!master->eoe_running) return;

    EC_INFO("Stopping EoE processing.\n");

    del_timer_sync(&master->eoe_timer);

    // decouple all EoE handlers
    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        if (eoe->slave) {
            eoe->slave->requested_state = EC_SLAVE_STATE_INIT;
            eoe->slave->error_flag = 0;
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
    cycles_t cycles_start, cycles_end;
    unsigned long restart_jiffies;

    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        if (ec_eoe_active(eoe)) active++;
    }
    if (!active) goto queue_timer;

    // aquire master lock...
    if (master->mode == EC_MASTER_MODE_OPERATION) {
        // request_cb must return 0, if the lock has been aquired!
        if (master->request_cb(master->cb_data))
            goto queue_timer;
    }
    else if (master->mode == EC_MASTER_MODE_IDLE) {
        spin_lock(&master->internal_lock);
    }
    else
        goto queue_timer;

    // actual EoE processing
    cycles_start = get_cycles();
    ecrt_master_receive(master);

    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        ec_eoe_run(eoe);
    }

    ecrt_master_send(master);
    cycles_end = get_cycles();

    // release lock...
    if (master->mode == EC_MASTER_MODE_OPERATION) {
        master->release_cb(master->cb_data);
    }
    else if (master->mode == EC_MASTER_MODE_IDLE) {
        spin_unlock(&master->internal_lock);
    }

    master->eoe_cycle_times[master->eoe_cycle_time_pos]
        = (u32) (cycles_end - cycles_start) * 1000 / cpu_khz;
    master->eoe_cycle_time_pos++;
    master->eoe_cycle_time_pos %= HZ;

 queue_timer:
    restart_jiffies = HZ / EC_EOE_FREQUENCY;
    if (!restart_jiffies) restart_jiffies = 1;
    master->eoe_timer.expires += restart_jiffies;
    add_timer(&master->eoe_timer);
}

/*****************************************************************************/

/**
   Calculates Advanced Position Adresses.
*/

void ec_master_calc_addressing(ec_master_t *master /**< EtherCAT master */)
{
    uint16_t coupler_index, coupler_subindex;
    uint16_t reverse_coupler_index, current_coupler_index;
    ec_slave_t *slave;

    coupler_index = 0;
    reverse_coupler_index = 0xFFFF;
    current_coupler_index = 0x0000;
    coupler_subindex = 0;

    list_for_each_entry(slave, &master->slaves, list) {
        if (ec_slave_is_coupler(slave)) {
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
}

/*****************************************************************************/

/**
   Measures the time, a frame is on the bus.
   \return 0 in case of success, else < 0
*/

int ec_master_measure_bus_time(ec_master_t *master)
{
    ec_datagram_t datagram;
    uint32_t cur, sum, min, max, i;

    ec_datagram_init(&datagram);

    if (ec_datagram_brd(&datagram, 0x130, 2)) {
        EC_ERR("Failed to allocate datagram for bus time measuring.\n");
        ec_datagram_clear(&datagram);
        return -1;
    }

    ecrt_master_receive(master);

    sum = 0;
    min = 0xFFFFFFFF;
    max = 0;

    for (i = 0; i < 100; i++) {
        ec_master_queue_datagram(master, &datagram);
        ecrt_master_send(master);

        while (1) {
            ecrt_master_receive(master);

            if (datagram.state == EC_DATAGRAM_RECEIVED) {
                break;
            }
            else if (datagram.state == EC_DATAGRAM_ERROR) {
                EC_WARN("Failed to measure bus time.\n");
                goto error;
            }
            else if (datagram.state == EC_DATAGRAM_TIMED_OUT) {
                EC_WARN("Timeout while measuring bus time.\n");
                goto error;
            }
        }

        cur = (unsigned int) (datagram.cycles_received
                              - datagram.cycles_sent) * 1000 / cpu_khz;
        sum += cur;
        if (cur > max) max = cur;
        if (cur < min) min = cur;
    }

    EC_INFO("Bus time is (min/avg/max) %u / %u.%u / %u us.\n",
            min, sum / 100, sum % 100, max);
    ec_datagram_clear(&datagram);
    return 0;

  error:
    ec_datagram_clear(&datagram);
    return -1;
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
    uint32_t domain_offset;
    ec_domain_t *domain;
    ec_fsm_t *fsm = &master->fsm;
    ec_slave_t *slave;

    // allocate all domains
    domain_offset = 0;
    list_for_each_entry(domain, &master->domains, list) {
        if (ec_domain_alloc(domain, domain_offset)) {
            EC_ERR("Failed to allocate domain %X!\n", (u32) domain);
            return -1;
        }
        domain_offset += domain->data_size;
    }

    // determine initial states.
    list_for_each_entry(slave, &master->slaves, list) {
        if (ec_slave_is_coupler(slave) || slave->registered) {
            slave->requested_state = EC_SLAVE_STATE_OP;
        }
        else {
            slave->requested_state = EC_SLAVE_STATE_PREOP;
        }
    }

    ec_fsm_configuration(fsm); // init configuration state machine

    do {
        ec_fsm_execute(fsm);
        ec_master_sync_io(master);
    }
    while (ec_fsm_configuration_running(fsm));

    if (!ec_fsm_configuration_success(fsm)) {
        return -1;
    }

    ec_fsm_reset(&master->fsm); // prepare for operation state machine

    return 0;
}

/*****************************************************************************/

/**
   Resets all slaves to INIT state.
   \ingroup RealtimeInterface
*/

void ecrt_master_deactivate(ec_master_t *master /**< EtherCAT master */)
{
    ec_fsm_t *fsm = &master->fsm;
    ec_slave_t *slave;

    list_for_each_entry(slave, &master->slaves, list) {
        slave->requested_state = EC_SLAVE_STATE_INIT;
    }

    ec_fsm_configuration(fsm); // init configuration state machine

    do {
        ec_fsm_execute(fsm);
        ec_master_sync_io(master);
    }
    while (ec_fsm_configuration_running(fsm));
}

/*****************************************************************************/

/**
   Sends queued datagrams and waits for their reception.
*/

void ec_master_sync_io(ec_master_t *master /**< EtherCAT master */)
{
    ec_datagram_t *datagram;
    unsigned int datagrams_waiting;

    // send datagrams
    ecrt_master_send(master);

    while (1) { // active waiting
        ecrt_master_receive(master); // receive and dequeue datagrams

        // count number of datagrams still waiting for response
        datagrams_waiting = 0;
        list_for_each_entry(datagram, &master->datagram_queue, queue) {
            datagrams_waiting++;
        }

        // if there are no more datagrams waiting, abort loop.
        if (!datagrams_waiting) break;
    }
}

/*****************************************************************************/

/**
   Asynchronous sending of datagrams.
   \ingroup RealtimeInterface
*/

void ecrt_master_send(ec_master_t *master /**< EtherCAT master */)
{
    ec_datagram_t *datagram, *n;

    if (unlikely(!master->device->link_state)) {
        // link is down, no datagram can be sent
        list_for_each_entry_safe(datagram, n, &master->datagram_queue, queue) {
            datagram->state = EC_DATAGRAM_ERROR;
            list_del_init(&datagram->queue);
        }

        // query link state
        ec_device_call_isr(master->device);
        return;
    }

    // send frames
    ec_master_send_datagrams(master);
}

/*****************************************************************************/

/**
   Asynchronous receiving of datagrams.
   \ingroup RealtimeInterface
*/

void ecrt_master_receive(ec_master_t *master /**< EtherCAT master */)
{
    ec_datagram_t *datagram, *next;
    cycles_t cycles_timeout;

    // receive datagrams
    ec_device_call_isr(master->device);

    cycles_timeout = (cycles_t) EC_IO_TIMEOUT /* us */ * (cpu_khz / 1000);

    // dequeue all datagrams that timed out
    list_for_each_entry_safe(datagram, next, &master->datagram_queue, queue) {
        switch (datagram->state) {
            case EC_DATAGRAM_QUEUED:
                if (master->device->cycles_isr
                    - datagram->cycles_queued > cycles_timeout) {
                    list_del_init(&datagram->queue);
                    datagram->state = EC_DATAGRAM_TIMED_OUT;
                    master->stats.timeouts++;
                    ec_master_output_stats(master);
                }
                break;
            case EC_DATAGRAM_SENT:
                if (master->device->cycles_isr
                    - datagram->cycles_sent > cycles_timeout) {
                    list_del_init(&datagram->queue);
                    datagram->state = EC_DATAGRAM_TIMED_OUT;
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
   Queues all domain datagrams and sends them. Then waits a certain time, so
   that ecrt_master_receive() can be called securely.
   \ingroup RealtimeInterface
*/

void ecrt_master_prepare(ec_master_t *master /**< EtherCAT master */)
{
    ec_domain_t *domain;
    cycles_t cycles_start, cycles_end, cycles_timeout;

    // queue datagrams of all domains
    list_for_each_entry(domain, &master->domains, list)
        ec_domain_queue(domain);

    ecrt_master_send(master);

    cycles_start = get_cycles();
    cycles_timeout = (cycles_t) EC_IO_TIMEOUT /* us */ * (cpu_khz / 1000);

    // active waiting
    while (1) {
        udelay(100);
        cycles_end = get_cycles();
        if (cycles_end - cycles_start >= cycles_timeout) break;
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
            if (!ec_slave_is_coupler(alias_slave)) {
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

/** \cond */

EXPORT_SYMBOL(ecrt_master_create_domain);
EXPORT_SYMBOL(ecrt_master_activate);
EXPORT_SYMBOL(ecrt_master_deactivate);
EXPORT_SYMBOL(ecrt_master_prepare);
EXPORT_SYMBOL(ecrt_master_send);
EXPORT_SYMBOL(ecrt_master_receive);
EXPORT_SYMBOL(ecrt_master_run);
EXPORT_SYMBOL(ecrt_master_callbacks);
EXPORT_SYMBOL(ecrt_master_get_slave);

/** \endcond */

/*****************************************************************************/
