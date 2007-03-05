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
void ec_master_destroy_domains(ec_master_t *);
void ec_master_sync_io(ec_master_t *);
static int ec_master_thread(void *);
void ec_master_eoe_run(unsigned long);
void ec_master_check_sdo(unsigned long);
int ec_master_measure_bus_time(ec_master_t *);
ssize_t ec_show_master_attribute(struct kobject *, struct attribute *, char *);
ssize_t ec_store_master_attribute(struct kobject *, struct attribute *,
                                  const char *, size_t);

/*****************************************************************************/

/** \cond */

EC_SYSFS_READ_ATTR(info);
EC_SYSFS_READ_WRITE_ATTR(debug_level);

static struct attribute *ec_def_attrs[] = {
    &attr_info,
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
        struct kobject *module_kobj, /**< kobject of the master module */
        unsigned int index, /**< master index */
        const ec_device_id_t *main_id, /**< ID of main device */
        const ec_device_id_t *backup_id, /**< ID of main device */
        unsigned int eoeif_count /**< number of EoE interfaces */
        )
{
    ec_eoe_t *eoe, *next_eoe;
    unsigned int i;

    atomic_set(&master->available, 1);
    master->index = index;

    master->main_device_id = main_id;
    master->backup_device_id = backup_id;
    init_MUTEX(&master->device_sem);

    master->mode = EC_MASTER_MODE_ORPHANED;

    INIT_LIST_HEAD(&master->slaves);
    master->slave_count = 0;

    INIT_LIST_HEAD(&master->datagram_queue);
    master->datagram_index = 0;

    INIT_LIST_HEAD(&master->domains);
    master->debug_level = 0;

    master->stats.timeouts = 0;
    master->stats.corrupted = 0;
    master->stats.skipped = 0;
    master->stats.unmatched = 0;
    master->stats.output_jiffies = 0;

    for (i = 0; i < HZ; i++) {
        master->idle_cycle_times[i] = 0;
        master->eoe_cycle_times[i] = 0;
    }
    master->idle_cycle_time_pos = 0;
    master->eoe_cycle_time_pos = 0;

    init_timer(&master->eoe_timer);
    master->eoe_timer.function = ec_master_eoe_run;
    master->eoe_timer.data = (unsigned long) master;
    master->eoe_running = 0;
    master->eoe_checked = 0;
    INIT_LIST_HEAD(&master->eoe_handlers);

    master->internal_lock = SPIN_LOCK_UNLOCKED;
    master->request_cb = NULL;
    master->release_cb = NULL;
    master->cb_data = NULL;

    INIT_LIST_HEAD(&master->eeprom_requests);
    init_MUTEX(&master->eeprom_sem);
    init_waitqueue_head(&master->eeprom_queue);

    master->sdo_request = NULL;
    master->sdo_seq_user = 0;
    master->sdo_seq_master = 0;
    init_MUTEX(&master->sdo_sem);
    init_timer(&master->sdo_timer);
    master->sdo_timer.function = ec_master_check_sdo;
    master->sdo_timer.data = (unsigned long) master;
    init_completion(&master->sdo_complete);

    // init devices
    if (ec_device_init(&master->main_device, master))
        goto out_return;

    if (ec_device_init(&master->backup_device, master))
        goto out_clear_main;

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

    // init state machine datagram
    ec_datagram_init(&master->fsm_datagram);
    if (ec_datagram_prealloc(&master->fsm_datagram, EC_MAX_DATA_SIZE)) {
        EC_ERR("Failed to allocate FSM datagram.\n");
        goto out_clear_eoe;
    }

    // create state machine object
    ec_fsm_master_init(&master->fsm, master, &master->fsm_datagram);

    // init kobject and add it to the hierarchy
    memset(&master->kobj, 0x00, sizeof(struct kobject));
    kobject_init(&master->kobj);
    master->kobj.ktype = &ktype_ec_master;
    master->kobj.parent = module_kobj;
    
    if (kobject_set_name(&master->kobj, "master%i", index)) {
        EC_ERR("Failed to set master kobject name.\n");
        kobject_put(&master->kobj);
        return -1;
    }
    
    if (kobject_add(&master->kobj)) {
        EC_ERR("Failed to add master kobject.\n");
        kobject_put(&master->kobj);
        return -1;
    }

    return 0;

out_clear_eoe:
    list_for_each_entry_safe(eoe, next_eoe, &master->eoe_handlers, list) {
        list_del(&eoe->list);
        ec_eoe_clear(eoe);
        kfree(eoe);
    }
    ec_device_clear(&master->backup_device);
out_clear_main:
    ec_device_clear(&master->main_device);
out_return:
    return -1;
}

/*****************************************************************************/

/**
   Master destructor.
   Clears the kobj-hierarchy bottom up and frees the master.
*/

void ec_master_destroy(ec_master_t *master /**< EtherCAT master */)
{
    ec_master_destroy_slaves(master);
    ec_master_destroy_domains(master);

    // destroy self
    kobject_del(&master->kobj);
    kobject_put(&master->kobj); // free master
}

/*****************************************************************************/

/**
   Clear and free master.
   This method is called by the kobject,
   once there are no more references to it.
*/

void ec_master_clear(struct kobject *kobj /**< kobject of the master */)
{
    ec_master_t *master = container_of(kobj, ec_master_t, kobj);
    ec_eoe_t *eoe, *next_eoe;
    ec_datagram_t *datagram, *next_datagram;

    // list of EEPROM requests is empty,
    // otherwise master could not be cleared.

    // dequeue all datagrams
    list_for_each_entry_safe(datagram, next_datagram,
                             &master->datagram_queue, queue) {
        datagram->state = EC_DATAGRAM_ERROR;
        list_del_init(&datagram->queue);
    }

    ec_fsm_master_clear(&master->fsm);
    ec_datagram_clear(&master->fsm_datagram);

    // clear EoE objects
    list_for_each_entry_safe(eoe, next_eoe, &master->eoe_handlers, list) {
        list_del(&eoe->list);
        ec_eoe_clear(eoe);
        kfree(eoe);
    }

    ec_device_clear(&master->backup_device);
    ec_device_clear(&master->main_device);

    EC_INFO("Master %i freed.\n", master->index);

    kfree(master);
}

/*****************************************************************************/

/**
   Destroy all slaves.
*/

void ec_master_destroy_slaves(ec_master_t *master)
{
    ec_slave_t *slave, *next_slave;

    list_for_each_entry_safe(slave, next_slave, &master->slaves, list) {
        list_del(&slave->list);
        ec_slave_destroy(slave);
    }

    master->slave_count = 0;
}

/*****************************************************************************/

/**
   Destroy all domains.
*/

void ec_master_destroy_domains(ec_master_t *master)
{
    ec_domain_t *domain, *next_d;

    list_for_each_entry_safe(domain, next_d, &master->domains, list) {
        list_del(&domain->list);
        ec_domain_destroy(domain);
    }
}

/*****************************************************************************/

/**
   Flushes the SDO request queue.
*/

void ec_master_flush_sdo_requests(ec_master_t *master)
{
    del_timer_sync(&master->sdo_timer);
    complete(&master->sdo_complete);
    master->sdo_request = NULL;
    master->sdo_seq_user = 0;
    master->sdo_seq_master = 0;
}

/*****************************************************************************/

/**
   Internal locking callback.
*/

int ec_master_request_cb(void *master /**< callback data */)
{
    spin_lock(&((ec_master_t *) master)->internal_lock);
    return 0;
}

/*****************************************************************************/

/**
   Internal unlocking callback.
*/

void ec_master_release_cb(void *master /**< callback data */)
{
    spin_unlock(&((ec_master_t *) master)->internal_lock);
}

/*****************************************************************************/

/**
 * Starts the master thread.
*/

int ec_master_thread_start(ec_master_t *master /**< EtherCAT master */)
{
    init_completion(&master->thread_exit);
    
    EC_INFO("Starting master thread.\n");
    if (!(master->thread_id =
                kernel_thread(ec_master_thread, master, CLONE_KERNEL)))
        return -1;
    
    return 0;
}

/*****************************************************************************/

/**
 * Stops the master thread.
*/

void ec_master_thread_stop(ec_master_t *master /**< EtherCAT master */)
{
    if (master->thread_id) {
        kill_proc(master->thread_id, SIGTERM, 1);
        wait_for_completion(&master->thread_exit);
        EC_INFO("Master thread exited.\n");
    }    
}

/*****************************************************************************/

/**
 * Transition function from ORPHANED to IDLE mode.
*/

int ec_master_enter_idle_mode(ec_master_t *master /**< EtherCAT master */)
{
    master->request_cb = ec_master_request_cb;
    master->release_cb = ec_master_release_cb;
    master->cb_data = master;
	
    master->mode = EC_MASTER_MODE_IDLE;
    if (ec_master_thread_start(master)) {
        master->mode = EC_MASTER_MODE_ORPHANED;
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
 * Transition function from IDLE to ORPHANED mode.
*/

void ec_master_leave_idle_mode(ec_master_t *master /**< EtherCAT master */)
{
    master->mode = EC_MASTER_MODE_ORPHANED;
    
    ec_master_eoe_stop(master);
    ec_master_thread_stop(master);
    ec_master_flush_sdo_requests(master);
    ec_master_destroy_slaves(master);
}

/*****************************************************************************/

/**
 * Transition function from IDLE to OPERATION mode.
*/

int ec_master_enter_operation_mode(ec_master_t *master /**< EtherCAT master */)
{
    ec_slave_t *slave;

    ec_master_eoe_stop(master); // stop EoE timer
    master->eoe_checked = 1; // prevent from starting again by FSM
    ec_master_thread_stop(master);

    master->mode = EC_MASTER_MODE_OPERATION;

    // wait for FSM datagram
    if (master->fsm_datagram.state == EC_DATAGRAM_SENT) {
        while (get_cycles() - master->fsm_datagram.cycles_sent
               < (cycles_t) EC_IO_TIMEOUT /* us */ * (cpu_khz / 1000)) {}
        ecrt_master_receive(master);
    }

    // finish running master FSM
    if (ec_fsm_master_running(&master->fsm)) {
        while (ec_fsm_master_exec(&master->fsm)) {
            ec_master_sync_io(master);
        }
    }

    if (master->debug_level) {
        if (ec_master_measure_bus_time(master)) {
            EC_ERR("Bus time measuring failed!\n");
            goto out_idle;
        }
    }

    // set initial slave states
    list_for_each_entry(slave, &master->slaves, list) {
        if (ec_slave_is_coupler(slave)) {
            ec_slave_request_state(slave, EC_SLAVE_STATE_OP);
        }
        else {
            ec_slave_request_state(slave, EC_SLAVE_STATE_PREOP);
        }
    }

    master->eoe_checked = 0; // allow starting EoE again

    return 0;

 out_idle:
    master->mode = EC_MASTER_MODE_IDLE;
    if (ec_master_thread_start(master)) {
        EC_WARN("Failed to start master thread again!\n");
    }
    return -1;
}

/*****************************************************************************/

/**
 * Transition function from OPERATION to IDLE mode.
*/

void ec_master_leave_operation_mode(ec_master_t *master
                                    /**< EtherCAT master */)
{
    ec_slave_t *slave;
    ec_fsm_master_t *fsm = &master->fsm;
    ec_fsm_slave_t fsm_slave;

    ec_master_eoe_stop(master); // stop EoE timer
    master->eoe_checked = 1; // prevent from starting again by FSM

    // wait for FSM datagram
    if (master->fsm_datagram.state == EC_DATAGRAM_SENT) {
        // active waiting
        while (get_cycles() - master->fsm_datagram.cycles_sent
               < (cycles_t) EC_IO_TIMEOUT /* us */ * (cpu_khz / 1000));
        ecrt_master_receive(master);
    }

    // finish running master FSM
    if (ec_fsm_master_running(fsm)) {
        while (ec_fsm_master_exec(fsm)) {
            ec_master_sync_io(master);
        }
    }

    ec_fsm_slave_init(&fsm_slave, &master->fsm_datagram);
    
    // set states for all slaves
    list_for_each_entry(slave, &master->slaves, list) {
        ec_slave_reset(slave);
        ec_slave_request_state(slave, EC_SLAVE_STATE_PREOP);

        // don't try to set PREOP for slaves that don't respond,
        // because of 3 second timeout.
        if (slave->online_state == EC_SLAVE_OFFLINE) {
            if (master->debug_level)
                EC_DBG("Skipping to configure offline slave %i.\n",
                        slave->ring_position);
            continue;
        }

        ec_fsm_slave_start_conf(&fsm_slave, slave);
        while (ec_fsm_slave_exec(&fsm_slave)) {
            ec_master_sync_io(master);
        }
    }

    ec_fsm_slave_clear(&fsm_slave);

    ec_master_destroy_domains(master);

    master->request_cb = ec_master_request_cb;
    master->release_cb = ec_master_release_cb;
    master->cb_data = master;

    master->eoe_checked = 0; // allow EoE again

    master->mode = EC_MASTER_MODE_IDLE;
    if (ec_master_thread_start(master)) {
        EC_WARN("Failed to start master thread!\n");
    }
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
        frame_data = ec_device_tx_data(&master->main_device);
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
        ec_device_send(&master->main_device, cur_data - frame_data);
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
        datagram->cycles_received = master->main_device.cycles_poll;
        datagram->jiffies_received = master->main_device.jiffies_poll;
        list_del_init(&datagram->queue);
    }
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
   Master kernel thread function.
*/

static int ec_master_thread(void *data)
{
    ec_master_t *master = (ec_master_t *) data;
    cycles_t cycles_start, cycles_end;

    daemonize("EtherCAT");
    allow_signal(SIGTERM);

    while (!signal_pending(current) && master->mode == EC_MASTER_MODE_IDLE) {
        cycles_start = get_cycles();

        // receive
        spin_lock_bh(&master->internal_lock);
        ecrt_master_receive(master);
        spin_unlock_bh(&master->internal_lock);

        // execute master state machine
        ec_fsm_master_exec(&master->fsm);

        // send
        spin_lock_bh(&master->internal_lock);
        ecrt_master_send(master);
        spin_unlock_bh(&master->internal_lock);
        
        cycles_end = get_cycles();
        master->idle_cycle_times[master->idle_cycle_time_pos]
            = (u32) (cycles_end - cycles_start) * 1000 / cpu_khz;
        master->idle_cycle_time_pos++;
        master->idle_cycle_time_pos %= HZ;

        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(1);
    }
    
    master->thread_id = 0;
    complete_and_exit(&master->thread_exit, 0);
}


/*****************************************************************************/

ssize_t ec_master_device_info(const ec_device_t *device,
        const ec_device_id_t *dev_id,
        char *buffer)
{
    unsigned int frames_lost;
    off_t off = 0;
    
    off += ec_device_id_print(dev_id, buffer + off);
    
    if (device->dev) {
        off += sprintf(buffer + off, " (connected).\n");      
        off += sprintf(buffer + off, "    Frames sent:     %u\n",
                device->tx_count);
        off += sprintf(buffer + off, "    Frames received: %u\n",
                device->rx_count);
        frames_lost = device->tx_count - device->rx_count;
        if (frames_lost) frames_lost--;
        off += sprintf(buffer + off, "    Frames lost:     %u\n", frames_lost);
    }
    else if (dev_id->type != ec_device_id_empty) {
        off += sprintf(buffer + off, " (WAITING).\n");      
    }
    else {
        off += sprintf(buffer + off, ".\n");
    }
    
    return off;
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

    off += sprintf(buffer + off, "\nDevices:\n");
    
    if (down_interruptible(&master->device_sem)) {
        EC_ERR("Interrupted while waiting for device!\n");
        return -EINVAL;
    }
    
    off += sprintf(buffer + off, "  Main: ");
    off += ec_master_device_info(&master->main_device,
            master->main_device_id, buffer + off);
    off += sprintf(buffer + off, "  Backup: ");
    off += ec_master_device_info(&master->backup_device,
            master->backup_device_id, buffer + off);

    up(&master->device_sem);

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

    if (attr == &attr_debug_level) {
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
            if (eoe->opened)
                ec_slave_request_state(slave, EC_SLAVE_STATE_OP);
            else
                ec_slave_request_state(slave, EC_SLAVE_STATE_PREOP);
            break;
        }

        if (!found) {
            if (master->debug_level)
                EC_WARN("No EoE handler for slave %i!\n",
                        slave->ring_position);
            ec_slave_request_state(slave, EC_SLAVE_STATE_PREOP);
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
            ec_slave_request_state(eoe->slave, EC_SLAVE_STATE_PREOP);
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

    // receive datagrams
    if (master->request_cb(master->cb_data)) goto queue_timer;
    cycles_start = get_cycles();
    ecrt_master_receive(master);
    master->release_cb(master->cb_data);

    // actual EoE processing
    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        ec_eoe_run(eoe);
    }

    // send datagrams
    if (master->request_cb(master->cb_data)) goto queue_timer;
    ecrt_master_send(master);
    cycles_end = get_cycles();
    master->release_cb(master->cb_data);

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
*/

void ec_master_check_sdo(unsigned long data /**< master pointer */)
{
    ec_master_t *master = (ec_master_t *) data;

    if (master->sdo_seq_master != master->sdo_seq_user) {
        master->sdo_timer.expires = jiffies + 10;
        add_timer(&master->sdo_timer);
        return;
    }

    // master has processed the request
    complete(&master->sdo_complete);
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

    EC_DBG("Bus time is (min/avg/max) %u / %u.%u / %u us.\n",
           min, sum / 100, sum % 100, max);
    ec_datagram_clear(&datagram);
    return 0;

  error:
    ec_datagram_clear(&datagram);
    return -1;
}

/*****************************************************************************/

/**
   Prepares synchronous IO.
   Queues all domain datagrams and sends them. Then waits a certain time, so
   that ecrt_master_receive() can be called securely.
*/

void ec_master_prepare(ec_master_t *master /**< EtherCAT master */)
{
    ec_domain_t *domain;
    cycles_t cycles_start, cycles_end, cycles_timeout;

    // queue datagrams of all domains
    list_for_each_entry(domain, &master->domains, list)
        ecrt_domain_queue(domain);

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
        return NULL;
    }

    if (list_empty(&master->domains)) index = 0;
    else {
        last_domain = list_entry(master->domains.prev, ec_domain_t, list);
        index = last_domain->index + 1;
    }

    if (ec_domain_init(domain, master, index)) {
        EC_ERR("Failed to init domain.\n");
        return NULL;
    }

    list_add_tail(&domain->list, &master->domains);

    return domain;
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
    ec_fsm_slave_t fsm_slave;
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

    ec_fsm_slave_init(&fsm_slave, &master->fsm_datagram);

    // configure all slaves
    list_for_each_entry(slave, &master->slaves, list) {
        ec_fsm_slave_start_conf(&fsm_slave, slave);
        while (ec_fsm_slave_exec(&fsm_slave)) { 
            ec_master_sync_io(master);
        }

        if (!ec_fsm_slave_success(&fsm_slave)) {
            ec_fsm_slave_clear(&fsm_slave);
            EC_ERR("Failed to configure slave %i!\n", slave->ring_position);
            return -1;
        }
    }

    ec_fsm_slave_clear(&fsm_slave);
    ec_master_prepare(master); // prepare asynchronous IO

    return 0;
}

/*****************************************************************************/

/**
   Sends queued datagrams and waits for their reception.
*/

void ec_master_sync_io(ec_master_t *master /**< EtherCAT master */)
{
    ec_datagram_t *datagram;
    unsigned int datagrams_sent;

    // send all datagrams
    ecrt_master_send(master);

    while (1) { // active waiting
        schedule(); // schedule other processes while waiting.
        ecrt_master_receive(master); // receive and dequeue datagrams

        // count number of datagrams still waiting for response
        datagrams_sent = 0;
        list_for_each_entry(datagram, &master->datagram_queue, queue) {
            // there may be another process that queued commands
            // in the meantime.
            if (datagram->state == EC_DATAGRAM_QUEUED) continue;
            datagrams_sent++;
        }

        // abort loop if there are no more datagrams marked as sent.
        if (!datagrams_sent) break;
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

    if (unlikely(!master->main_device.link_state)) {
        // link is down, no datagram can be sent
        list_for_each_entry_safe(datagram, n, &master->datagram_queue, queue) {
            datagram->state = EC_DATAGRAM_ERROR;
            list_del_init(&datagram->queue);
        }

        // query link state
        ec_device_poll(&master->main_device);
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
    ec_device_poll(&master->main_device);

    cycles_timeout = (cycles_t) EC_IO_TIMEOUT /* us */ * (cpu_khz / 1000);

    // dequeue all datagrams that timed out
    list_for_each_entry_safe(datagram, next, &master->datagram_queue, queue) {
        if (datagram->state != EC_DATAGRAM_SENT) continue;

        if (master->main_device.cycles_poll - datagram->cycles_sent
            > cycles_timeout) {
            list_del_init(&datagram->queue);
            datagram->state = EC_DATAGRAM_TIMED_OUT;
            master->stats.timeouts++;
            ec_master_output_stats(master);
        }
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

    // execute master state machine in a loop
    ec_fsm_master_exec(&master->fsm);
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
    const char *original;
    unsigned int alias_requested, alias_found;
    ec_slave_t *alias_slave = NULL, *slave;

    original = address;

    if (!address || address[0] == 0) return NULL;

    alias_requested = 0;
    if (address[0] == '#') {
        alias_requested = 1;
        address++;
    }

    first = simple_strtoul(address, &remainder, 0);
    if (remainder == address) {
        EC_ERR("Slave address \"%s\" - First number empty!\n", original);
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
            EC_ERR("Slave address \"%s\" - Alias not found!\n", original);
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
                   original);
        }
    }
    else if (remainder[0] == ':') { // field position
        remainder++;
        second = simple_strtoul(remainder, &remainder2, 0);

        if (remainder2 == remainder) {
            EC_ERR("Slave address \"%s\" - Second number empty!\n", original);
            return NULL;
        }

        if (remainder2[0]) {
            EC_ERR("Slave address \"%s\" - Invalid trailer!\n", original);
            return NULL;
        }

        if (alias_requested) {
            if (!ec_slave_is_coupler(alias_slave)) {
                EC_ERR("Slave address \"%s\": Alias slave must be bus coupler"
                       " in colon mode.\n", original);
                return NULL;
            }
            list_for_each_entry(slave, &master->slaves, list) {
                if (slave->coupler_index == alias_slave->coupler_index
                    && slave->coupler_subindex == second)
                    return slave;
            }
            EC_ERR("Slave address \"%s\" - Bus coupler %i has no %lu. slave"
                   " following!\n", original, alias_slave->ring_position,
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
        EC_ERR("Slave address \"%s\" - Invalid format!\n", original);

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
EXPORT_SYMBOL(ecrt_master_send);
EXPORT_SYMBOL(ecrt_master_receive);
EXPORT_SYMBOL(ecrt_master_run);
EXPORT_SYMBOL(ecrt_master_callbacks);
EXPORT_SYMBOL(ecrt_master_get_slave);

/** \endcond */

/*****************************************************************************/
