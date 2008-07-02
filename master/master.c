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
#include <linux/device.h>
#include <linux/version.h>

#include "../include/ecrt.h"
#include "globals.h"
#include "slave.h"
#include "slave_config.h"
#include "device.h"
#include "datagram.h"
#ifdef EC_EOE
#include "ethernet.h"
#endif
#include "master.h"

/*****************************************************************************/

void ec_master_clear_slave_configs(ec_master_t *);
void ec_master_clear_domains(ec_master_t *);
static int ec_master_idle_thread(ec_master_t *);
static int ec_master_operation_thread(ec_master_t *);
#ifdef EC_EOE
void ec_master_eoe_run(unsigned long);
#endif

/*****************************************************************************/

/**
   Master constructor.
   \return 0 in case of success, else < 0
*/

int ec_master_init(ec_master_t *master, /**< EtherCAT master */
        unsigned int index, /**< master index */
        const uint8_t *main_mac, /**< MAC address of main device */
        const uint8_t *backup_mac, /**< MAC address of backup device */
        dev_t device_number, /**< Character device number. */
        struct class *class /**< Device class. */
        )
{
    master->index = index;
    master->reserved = 0;

    init_MUTEX(&master->master_sem);

    master->main_mac = main_mac;
    master->backup_mac = backup_mac;
    init_MUTEX(&master->device_sem);

    master->phase = EC_ORPHANED;
    master->injection_seq_fsm = 0;
    master->injection_seq_rt = 0;

    master->slaves = NULL;
    master->slave_count = 0;
    
    INIT_LIST_HEAD(&master->configs);

    master->scan_busy = 0;
    master->allow_scan = 1;
    init_MUTEX(&master->scan_sem);
    init_waitqueue_head(&master->scan_queue);

    master->config_busy = 0;
    master->allow_config = 1;
    init_MUTEX(&master->config_sem);
    init_waitqueue_head(&master->config_queue);
    
    INIT_LIST_HEAD(&master->datagram_queue);
    master->datagram_index = 0;

    INIT_LIST_HEAD(&master->domains);

    master->debug_level = 0;
    master->stats.timeouts = 0;
    master->stats.corrupted = 0;
    master->stats.unmatched = 0;
    master->stats.output_jiffies = 0;
    master->frames_timed_out = 0;

#ifdef EC_EOE
    init_timer(&master->eoe_timer);
    master->eoe_timer.function = ec_master_eoe_run;
    master->eoe_timer.data = (unsigned long) master;
    master->eoe_running = 0;
    INIT_LIST_HEAD(&master->eoe_handlers);
#endif

    master->internal_lock = SPIN_LOCK_UNLOCKED;
    master->request_cb = NULL;
    master->release_cb = NULL;
    master->cb_data = NULL;

    INIT_LIST_HEAD(&master->sii_requests);
    init_MUTEX(&master->sii_sem);
    init_waitqueue_head(&master->sii_queue);

    INIT_LIST_HEAD(&master->slave_sdo_requests);
    init_MUTEX(&master->sdo_sem);
    init_waitqueue_head(&master->sdo_queue);

    // init devices
    if (ec_device_init(&master->main_device, master))
        goto out_return;

    if (ec_device_init(&master->backup_device, master))
        goto out_clear_main;

    // init state machine datagram
    ec_datagram_init(&master->fsm_datagram);
    snprintf(master->fsm_datagram.name, EC_DATAGRAM_NAME_SIZE, "master-fsm");
    if (ec_datagram_prealloc(&master->fsm_datagram, EC_MAX_DATA_SIZE)) {
        EC_ERR("Failed to allocate FSM datagram.\n");
        goto out_clear_backup;
    }

    // create state machine object
    ec_fsm_master_init(&master->fsm, master, &master->fsm_datagram);

    // init character device
    if (ec_cdev_init(&master->cdev, master, device_number))
        goto out_clear_fsm;
    
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 15)
    master->class_device = class_device_create(class,
            MKDEV(MAJOR(device_number), master->index),
            NULL, "EtherCAT%u", master->index);
#else
    master->class_device = class_device_create(class, NULL,
            MKDEV(MAJOR(device_number), master->index),
            NULL, "EtherCAT%u", master->index);
#endif
    if (IS_ERR(master->class_device)) {
        EC_ERR("Failed to create class device!\n");
        goto out_clear_cdev;
    }

    return 0;

out_clear_cdev:
    ec_cdev_clear(&master->cdev);
out_clear_fsm:
    ec_fsm_master_clear(&master->fsm);
    ec_datagram_clear(&master->fsm_datagram);
out_clear_backup:
    ec_device_clear(&master->backup_device);
out_clear_main:
    ec_device_clear(&master->main_device);
out_return:
    return -1;
}

/*****************************************************************************/

/** Destructor.
*/
void ec_master_clear(
        ec_master_t *master /**< EtherCAT master */
        )
{
    class_device_unregister(master->class_device);
    ec_cdev_clear(&master->cdev);
#ifdef EC_EOE
    ec_master_clear_eoe_handlers(master);
#endif
    ec_master_clear_domains(master);
    ec_master_clear_slave_configs(master);
    ec_master_clear_slaves(master);
    ec_fsm_master_clear(&master->fsm);
    ec_datagram_clear(&master->fsm_datagram);
    ec_device_clear(&master->backup_device);
    ec_device_clear(&master->main_device);
}

/*****************************************************************************/

#ifdef EC_EOE
/** Clear and free all EoE handlers.
 */
void ec_master_clear_eoe_handlers(
        ec_master_t *master /**< EtherCAT master */
        )
{
    ec_eoe_t *eoe, *next;

    list_for_each_entry_safe(eoe, next, &master->eoe_handlers, list) {
        list_del(&eoe->list);
        ec_eoe_clear(eoe);
        kfree(eoe);
    }
}
#endif

/*****************************************************************************/

/** Clear all slave configurations.
 */
void ec_master_clear_slave_configs(ec_master_t *master)
{
    ec_slave_config_t *sc, *next;

    list_for_each_entry_safe(sc, next, &master->configs, list) {
        list_del(&sc->list);
        ec_slave_config_clear(sc);
        kfree(sc);
    }
}

/*****************************************************************************/

/** Clear all slaves.
 */
void ec_master_clear_slaves(ec_master_t *master)
{
    ec_slave_t *slave;

    for (slave = master->slaves;
            slave < master->slaves + master->slave_count;
            slave++) {
        ec_slave_clear(slave);
    }

    if (master->slaves) {
        kfree(master->slaves);
        master->slaves = NULL;
    }

    master->slave_count = 0;
}

/*****************************************************************************/

/** Clear all domains.
 */
void ec_master_clear_domains(ec_master_t *master)
{
    ec_domain_t *domain, *next;

    list_for_each_entry_safe(domain, next, &master->domains, list) {
        list_del(&domain->list);
        ec_domain_clear(domain);
        kfree(domain);
    }
}

/*****************************************************************************/

/** Internal locking callback.
 */
int ec_master_request_cb(void *master /**< callback data */)
{
    spin_lock(&((ec_master_t *) master)->internal_lock);
    return 0;
}

/*****************************************************************************/

/** Internal unlocking callback.
 */
void ec_master_release_cb(void *master /**< callback data */)
{
    spin_unlock(&((ec_master_t *) master)->internal_lock);
}

/*****************************************************************************/

/** Starts the master thread.
 */
int ec_master_thread_start(
        ec_master_t *master, /**< EtherCAT master */
        int (*thread_func)(ec_master_t *) /**< thread function to start */
        )
{
    init_completion(&master->thread_exit);

    EC_INFO("Starting master thread.\n");
    if (!(master->thread_id = kernel_thread((int (*)(void *)) thread_func,
                    master, CLONE_KERNEL)))
        return -1;
    
    return 0;
}

/*****************************************************************************/

/** Stops the master thread.
 */
void ec_master_thread_stop(
        ec_master_t *master /**< EtherCAT master */
        )
{
    unsigned long sleep_jiffies;
    
    if (!master->thread_id) {
        EC_WARN("ec_master_thread_stop: Already finished!\n");
        return;
    }

    if (master->debug_level)
        EC_DBG("Stopping master thread.\n");

    kill_proc(master->thread_id, SIGTERM, 1);
    wait_for_completion(&master->thread_exit);
    EC_INFO("Master thread exited.\n");

    if (master->fsm_datagram.state != EC_DATAGRAM_SENT) return;
    
    // wait for FSM datagram
    sleep_jiffies = max(HZ / 100, 1); // 10 ms, at least 1 jiffy
    schedule_timeout(sleep_jiffies);
}

/*****************************************************************************/

/** Transition function from ORPHANED to IDLE phase.
 */
int ec_master_enter_idle_phase(
        ec_master_t *master /**< EtherCAT master */
        )
{
    if (master->debug_level)
        EC_DBG("ORPHANED -> IDLE.\n");

    master->request_cb = ec_master_request_cb;
    master->release_cb = ec_master_release_cb;
    master->cb_data = master;

    master->phase = EC_IDLE;
    if (ec_master_thread_start(master, ec_master_idle_thread)) {
        master->phase = EC_ORPHANED;
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/** Transition function from IDLE to ORPHANED phase.
 */
void ec_master_leave_idle_phase(ec_master_t *master /**< EtherCAT master */)
{
    if (master->debug_level)
        EC_DBG("IDLE -> ORPHANED.\n");

    master->phase = EC_ORPHANED;
    
#ifdef EC_EOE
    ec_master_eoe_stop(master);
#endif
    ec_master_thread_stop(master);

    down(&master->master_sem);
    ec_master_clear_slaves(master);
    up(&master->master_sem);
}

/*****************************************************************************/

/** Transition function from IDLE to OPERATION phase.
 */
int ec_master_enter_operation_phase(ec_master_t *master /**< EtherCAT master */)
{
    ec_slave_t *slave;
#ifdef EC_EOE
    ec_eoe_t *eoe;
#endif

    if (master->debug_level)
        EC_DBG("IDLE -> OPERATION.\n");

    down(&master->config_sem);
    master->allow_config = 0; // temporarily disable slave configuration
    if (master->config_busy) {
        up(&master->config_sem);

        // wait for slave configuration to complete
        if (wait_event_interruptible(master->config_queue,
                    !master->config_busy)) {
            EC_INFO("Finishing slave configuration interrupted by signal.\n");
            goto out_allow;
        }

        if (master->debug_level)
            EC_DBG("Waiting for pending slave configuration returned.\n");
    } else {
        up(&master->config_sem);
    }

    down(&master->scan_sem);
    master->allow_scan = 0; // 'lock' the slave list
    if (!master->scan_busy) {
        up(&master->scan_sem);
    } else {
        up(&master->scan_sem);

        // wait for slave scan to complete
        if (wait_event_interruptible(master->scan_queue, !master->scan_busy)) {
            EC_INFO("Waiting for slave scan interrupted by signal.\n");
            goto out_allow;
        }
        
        if (master->debug_level)
            EC_DBG("Waiting for pending slave scan returned.\n");
    }

    // set states for all slaves
    for (slave = master->slaves;
            slave < master->slaves + master->slave_count;
            slave++) {
        ec_slave_request_state(slave, EC_SLAVE_STATE_PREOP);
    }
#ifdef EC_EOE
    // ... but set EoE slaves to OP
    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        if (ec_eoe_is_open(eoe))
            ec_slave_request_state(eoe->slave, EC_SLAVE_STATE_OP);
    }
#endif

    master->phase = EC_OPERATION;
    master->ext_request_cb = NULL;
    master->ext_release_cb = NULL;
    master->ext_cb_data = NULL;
    return 0;
    
out_allow:
    master->allow_scan = 1;
    master->allow_config = 1;
    return -1;
}

/*****************************************************************************/

/** Transition function from OPERATION to IDLE phase.
 */
void ec_master_leave_operation_phase(ec_master_t *master
                                    /**< EtherCAT master */)
{
    ec_slave_t *slave;
#ifdef EC_EOE
    ec_eoe_t *eoe;
#endif

    if (master->debug_level)
        EC_DBG("OPERATION -> IDLE.\n");

    master->phase = EC_IDLE;

#ifdef EC_EOE
    ec_master_eoe_stop(master);
#endif
    ec_master_thread_stop(master);
    
    master->request_cb = ec_master_request_cb;
    master->release_cb = ec_master_release_cb;
    master->cb_data = master;
    
    down(&master->master_sem);
    ec_master_clear_domains(master);
    ec_master_clear_slave_configs(master);
    up(&master->master_sem);

    // set states for all slaves
    for (slave = master->slaves;
            slave < master->slaves + master->slave_count;
            slave++) {
        ec_slave_request_state(slave, EC_SLAVE_STATE_PREOP);
    }
#ifdef EC_EOE
    // ... but leave EoE slaves in OP
    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        if (ec_eoe_is_open(eoe))
            ec_slave_request_state(eoe->slave, EC_SLAVE_STATE_OP);
    }
#endif

    if (ec_master_thread_start(master, ec_master_idle_thread))
        EC_WARN("Failed to restart master thread!\n");
#ifdef EC_EOE
    ec_master_eoe_start(master);
#endif

    master->allow_scan = 1;
    master->allow_config = 1;
}

/*****************************************************************************/

/** Places a datagram in the datagram queue.
 */
void ec_master_queue_datagram(ec_master_t *master, /**< EtherCAT master */
                              ec_datagram_t *datagram /**< datagram */
                              )
{
    ec_datagram_t *queued_datagram;

    // check, if the datagram is already queued
    list_for_each_entry(queued_datagram, &master->datagram_queue, queue) {
        if (queued_datagram == datagram) {
            datagram->skip_count++;
            if (master->debug_level)
                EC_DBG("skipping datagram %x.\n", (unsigned int) datagram);
            datagram->state = EC_DATAGRAM_QUEUED;
            return;
        }
    }

    list_add_tail(&datagram->queue, &master->datagram_queue);
    datagram->state = EC_DATAGRAM_QUEUED;
}

/*****************************************************************************/

/** Sends the datagrams in the queue.
 *
 * \return 0 in case of success, else < 0
 */
void ec_master_send_datagrams(ec_master_t *master /**< EtherCAT master */)
{
    ec_datagram_t *datagram, *next;
    size_t datagram_size;
    uint8_t *frame_data, *cur_data;
    void *follows_word;
#ifdef EC_HAVE_CYCLES
    cycles_t cycles_start, cycles_sent, cycles_end;
#endif
    unsigned long jiffies_sent;
    unsigned int frame_count, more_datagrams_waiting;
    struct list_head sent_datagrams;

#ifdef EC_HAVE_CYCLES
    cycles_start = get_cycles();
#endif
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
            memcpy(cur_data + 2, datagram->address, EC_ADDR_LEN);
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
            EC_DBG("frame size: %u\n", cur_data - frame_data);

        // send frame
        ec_device_send(&master->main_device, cur_data - frame_data);
#ifdef EC_HAVE_CYCLES
        cycles_sent = get_cycles();
#endif
        jiffies_sent = jiffies;

        // set datagram states and sending timestamps
        list_for_each_entry_safe(datagram, next, &sent_datagrams, sent) {
            datagram->state = EC_DATAGRAM_SENT;
#ifdef EC_HAVE_CYCLES
            datagram->cycles_sent = cycles_sent;
#endif
            datagram->jiffies_sent = jiffies_sent;
            list_del_init(&datagram->sent); // empty list of sent datagrams
        }

        frame_count++;
    }
    while (more_datagrams_waiting);

#ifdef EC_HAVE_CYCLES
    if (unlikely(master->debug_level > 1)) {
        cycles_end = get_cycles();
        EC_DBG("ec_master_send_datagrams sent %u frames in %uus.\n",
               frame_count,
               (unsigned int) (cycles_end - cycles_start) * 1000 / cpu_khz);
    }
#endif
}

/*****************************************************************************/

/** Processes a received frame.
 *
 * This function is called by the network driver for every received frame.
 * 
 * \return 0 in case of success, else < 0
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
            if (datagram->index == datagram_index
                && datagram->state == EC_DATAGRAM_SENT
                && datagram->type == datagram_type
                && datagram->data_size == data_size) {
                matched = 1;
                break;
            }
        }

        // no matching datagram was found
        if (!matched) {
            master->stats.unmatched++;
            ec_master_output_stats(master);

            if (unlikely(master->debug_level > 0)) {
                EC_DBG("UNMATCHED datagram:\n");
                ec_print_data(cur_data - EC_DATAGRAM_HEADER_SIZE,
                        EC_DATAGRAM_HEADER_SIZE + data_size
                        + EC_DATAGRAM_FOOTER_SIZE);
#ifdef EC_DEBUG_RING
                ec_device_debug_ring_print(&master->main_device);
#endif
            }

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
#ifdef EC_HAVE_CYCLES
        datagram->cycles_received = master->main_device.cycles_poll;
#endif
        datagram->jiffies_received = master->main_device.jiffies_poll;
        list_del_init(&datagram->queue);
    }
}

/*****************************************************************************/

/** Output master statistics.
 *
 * This function outputs statistical data on demand, but not more often than
 * necessary. The output happens at most once a second.
 */
void ec_master_output_stats(ec_master_t *master /**< EtherCAT master */)
{
    if (unlikely(jiffies - master->stats.output_jiffies >= HZ)) {
        master->stats.output_jiffies = jiffies;

        if (master->stats.timeouts) {
            EC_WARN("%u datagram%s TIMED OUT!\n", master->stats.timeouts,
                    master->stats.timeouts == 1 ? "" : "s");
            master->stats.timeouts = 0;
        }
        if (master->stats.corrupted) {
            EC_WARN("%u frame%s CORRUPTED!\n", master->stats.corrupted,
                    master->stats.corrupted == 1 ? "" : "s");
            master->stats.corrupted = 0;
        }
        if (master->stats.unmatched) {
            EC_WARN("%u datagram%s UNMATCHED!\n", master->stats.unmatched,
                    master->stats.unmatched == 1 ? "" : "s");
            master->stats.unmatched = 0;
        }
    }
}

/*****************************************************************************/

/** Master kernel thread function for IDLE phase.
 */
static int ec_master_idle_thread(ec_master_t *master)
{
    daemonize("EtherCAT-IDLE");
    allow_signal(SIGTERM);

    while (!signal_pending(current)) {
        ec_datagram_output_stats(&master->fsm_datagram);

        // receive
        spin_lock_bh(&master->internal_lock);
        ecrt_master_receive(master);
        spin_unlock_bh(&master->internal_lock);

        if (master->fsm_datagram.state == EC_DATAGRAM_SENT)
            goto schedule;

        // execute master state machine
        down(&master->master_sem);
        ec_fsm_master_exec(&master->fsm);
        up(&master->master_sem);

        // queue and send
        spin_lock_bh(&master->internal_lock);
        ec_master_queue_datagram(master, &master->fsm_datagram);
        ecrt_master_send(master);
        spin_unlock_bh(&master->internal_lock);
        
schedule:
        if (ec_fsm_master_idle(&master->fsm)) {
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(1);
        }
        else {
            schedule();
        }
    }
    
    master->thread_id = 0;
    if (master->debug_level)
        EC_DBG("Master IDLE thread exiting...\n");
    complete_and_exit(&master->thread_exit, 0);
}

/*****************************************************************************/

/** Master kernel thread function for IDLE phase.
 */
static int ec_master_operation_thread(ec_master_t *master)
{
    daemonize("EtherCAT-OP");
    allow_signal(SIGTERM);

    while (!signal_pending(current)) {
        ec_datagram_output_stats(&master->fsm_datagram);
        if (master->injection_seq_rt != master->injection_seq_fsm ||
                master->fsm_datagram.state == EC_DATAGRAM_SENT ||
                master->fsm_datagram.state == EC_DATAGRAM_QUEUED)
            goto schedule;

        // output statistics
        ec_master_output_stats(master);

        // execute master state machine
        down(&master->master_sem);
        ec_fsm_master_exec(&master->fsm);
        up(&master->master_sem);

        // inject datagram
        master->injection_seq_fsm++;

schedule:
        if (ec_fsm_master_idle(&master->fsm)) {
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(1);
        }
        else {
            schedule();
        }
    }
    
    master->thread_id = 0;
    if (master->debug_level)
        EC_DBG("Master OP thread exiting...\n");
    complete_and_exit(&master->thread_exit, 0);
}

/*****************************************************************************/

#ifdef EC_EOE
/** Starts Ethernet-over-EtherCAT processing on demand.
 */
void ec_master_eoe_start(ec_master_t *master /**< EtherCAT master */)
{
    if (master->eoe_running) {
        EC_WARN("EoE already running!\n");
        return;
    }

    if (list_empty(&master->eoe_handlers))
        return;

    if (!master->request_cb || !master->release_cb) {
        EC_WARN("No EoE processing because of missing locking callbacks!\n");
        return;
    }

    EC_INFO("Starting EoE processing.\n");
    master->eoe_running = 1;

    // start EoE processing
    master->eoe_timer.expires = jiffies + 10;
    add_timer(&master->eoe_timer);
}

/*****************************************************************************/

/** Stops the Ethernet-over-EtherCAT processing.
 */
void ec_master_eoe_stop(ec_master_t *master /**< EtherCAT master */)
{
    if (!master->eoe_running) return;

    EC_INFO("Stopping EoE processing.\n");

    del_timer_sync(&master->eoe_timer);
    master->eoe_running = 0;
}

/*****************************************************************************/

/** Does the Ethernet-over-EtherCAT processing.
 */
void ec_master_eoe_run(unsigned long data /**< master pointer */)
{
    ec_master_t *master = (ec_master_t *) data;
    ec_eoe_t *eoe;
    unsigned int none_open = 1;
    unsigned long restart_jiffies;

    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        if (ec_eoe_is_open(eoe)) {
            none_open = 0;
            break;
        }
    }
    if (none_open)
        goto queue_timer;

    // receive datagrams
    if (master->request_cb(master->cb_data))
        goto queue_timer;
    
    ecrt_master_receive(master);
    master->release_cb(master->cb_data);

    // actual EoE processing
    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        ec_eoe_run(eoe);
    }

    // send datagrams
    if (master->request_cb(master->cb_data)) {
        goto queue_timer;
    }
    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        ec_eoe_queue(eoe);
    }
    ecrt_master_send(master);
    master->release_cb(master->cb_data);

 queue_timer:
    restart_jiffies = HZ / EC_EOE_FREQUENCY;
    if (!restart_jiffies) restart_jiffies = 1;
    master->eoe_timer.expires = jiffies + restart_jiffies;
    add_timer(&master->eoe_timer);
}
#endif

/*****************************************************************************/

/** Detaches the slave configurations from the slaves.
 */
void ec_master_detach_slave_configs(
        ec_master_t *master /**< EtherCAT master. */
        )
{
    ec_slave_config_t *sc;

    list_for_each_entry(sc, &master->configs, list) {
        ec_slave_config_detach(sc); 
    }
}

/*****************************************************************************/

/** Attaches the slave configurations to the slaves.
 */
void ec_master_attach_slave_configs(
        ec_master_t *master /**< EtherCAT master. */
        )
{
    ec_slave_config_t *sc;

    list_for_each_entry(sc, &master->configs, list) {
        ec_slave_config_attach(sc);
    }
}

/*****************************************************************************/

/** Common implementation for ec_master_find_slave()
 * and ec_master_find_slave_const().
 */
#define EC_FIND_SLAVE \
    do { \
        if (alias) { \
            for (; slave < master->slaves + master->slave_count; \
                    slave++) { \
                if (slave->sii.alias == alias) \
                break; \
            } \
            if (slave == master->slaves + master->slave_count) \
            return NULL; \
        } \
        \
        slave += position; \
        if (slave < master->slaves + master->slave_count) { \
            return slave; \
        } else { \
            return NULL; \
        } \
    } while (0)

/** Finds a slave in the bus, given the alias and position.
 */
ec_slave_t *ec_master_find_slave(
        ec_master_t *master, /**< EtherCAT master. */
        uint16_t alias, /**< Slave alias. */
        uint16_t position /**< Slave position. */
        )
{
    ec_slave_t *slave = master->slaves;
    EC_FIND_SLAVE;
}

/** Finds a slave in the bus, given the alias and position.
 *
 * Const version.
 */
const ec_slave_t *ec_master_find_slave_const(
        const ec_master_t *master, /**< EtherCAT master. */
        uint16_t alias, /**< Slave alias. */
        uint16_t position /**< Slave position. */
        )
{
    const ec_slave_t *slave = master->slaves;
    EC_FIND_SLAVE;
}

/*****************************************************************************/

unsigned int ec_master_config_count(
		const ec_master_t *master /**< EtherCAT master. */
		)
{
	const ec_slave_config_t *sc;
	unsigned int count = 0;

	list_for_each_entry(sc, &master->configs, list) {
		count++;
	}

	return count;
}

/*****************************************************************************/

const ec_slave_config_t *ec_master_get_config_const(
		const ec_master_t *master, /**< EtherCAT master. */
		unsigned int index /**< List position. */
		)
{
	const ec_slave_config_t *sc;

	list_for_each_entry(sc, &master->configs, list) {
		if (index--)
			continue;
		return sc;
	}

	return NULL;
}

/*****************************************************************************/

unsigned int ec_master_domain_count(
		const ec_master_t *master /**< EtherCAT master. */
		)
{
	const ec_domain_t *domain;
	unsigned int count = 0;

	list_for_each_entry(domain, &master->domains, list) {
		count++;
	}

	return count;
}

/*****************************************************************************/

ec_domain_t *ec_master_find_domain(
		ec_master_t *master, /**< EtherCAT master. */
		unsigned int index /**< Domain index. */
		)
{
	ec_domain_t *domain;

	list_for_each_entry(domain, &master->domains, list) {
		if (index--)
			continue;
		return domain;
	}

	return NULL;
}

/*****************************************************************************/

int ec_master_debug_level(
        ec_master_t *master,
        int level
        )
{
    if (level < 0 || level > 2) {
        EC_ERR("Invalid debug level %i!\n", level);
        return -1;
    }

    if (level != master->debug_level) {
        master->debug_level = level;
        EC_INFO("Master debug level set to %i.\n", master->debug_level);
    }

    return 0;
}

/******************************************************************************
 *  Realtime interface
 *****************************************************************************/

ec_domain_t *ecrt_master_create_domain(ec_master_t *master /**< master */)
{
    ec_domain_t *domain, *last_domain;
    unsigned int index;

    if (!(domain = (ec_domain_t *) kmalloc(sizeof(ec_domain_t), GFP_KERNEL))) {
        EC_ERR("Error allocating domain memory!\n");
        return NULL;
    }

    down(&master->master_sem);

    if (list_empty(&master->domains)) {
        index = 0;
    } else {
        last_domain = list_entry(master->domains.prev, ec_domain_t, list);
        index = last_domain->index + 1;
    }

    ec_domain_init(domain, master, index);
    list_add_tail(&domain->list, &master->domains);

    up(&master->master_sem);

    return domain;
}

/*****************************************************************************/

int ecrt_master_activate(ec_master_t *master)
{
    uint32_t domain_offset;
    ec_domain_t *domain;

    down(&master->master_sem);

    // finish all domains
    domain_offset = 0;
    list_for_each_entry(domain, &master->domains, list) {
        if (ec_domain_finish(domain, domain_offset)) {
            EC_ERR("Failed to finish domain 0x%08X!\n", (u32) domain);
            return -1;
        }
        domain_offset += domain->data_size;
    }
    
    up(&master->master_sem);

    // restart EoE process and master thread with new locking
#ifdef EC_EOE
    ec_master_eoe_stop(master);
#endif
    ec_master_thread_stop(master);

    if (master->debug_level)
        EC_DBG("FSM datagram is %x.\n", (unsigned int) &master->fsm_datagram);

    master->injection_seq_fsm = 0;
    master->injection_seq_rt = 0;
    master->request_cb = master->ext_request_cb;
    master->release_cb = master->ext_release_cb;
    master->cb_data = master->ext_cb_data;
    
    if (ec_master_thread_start(master, ec_master_operation_thread)) {
        EC_ERR("Failed to start master thread!\n");
        return -1;
    }
#ifdef EC_EOE
    ec_master_eoe_start(master);
#endif

    master->allow_config = 1; // request the current configuration
    master->allow_scan = 1; // allow re-scanning on topology change
    return 0;
}

/*****************************************************************************/

void ecrt_master_send(ec_master_t *master)
{
    ec_datagram_t *datagram, *n;

    if (master->injection_seq_rt != master->injection_seq_fsm) {
        // inject datagram produced by master FSM
        ec_master_queue_datagram(master, &master->fsm_datagram);
        master->injection_seq_rt = master->injection_seq_fsm;
    }

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

void ecrt_master_receive(ec_master_t *master)
{
    ec_datagram_t *datagram, *next;
#ifdef EC_HAVE_CYCLES
    cycles_t cycles_timeout;
#else
    unsigned long diff_ms, timeout_ms;
#endif
    unsigned int frames_timed_out = 0;

    // receive datagrams
    ec_device_poll(&master->main_device);

#ifdef EC_HAVE_CYCLES
    cycles_timeout = (cycles_t) EC_IO_TIMEOUT /* us */ * (cpu_khz / 1000);
#else
    timeout_ms = max(EC_IO_TIMEOUT /* us */ / 1000, 2);
#endif

    // dequeue all datagrams that timed out
    list_for_each_entry_safe(datagram, next, &master->datagram_queue, queue) {
        if (datagram->state != EC_DATAGRAM_SENT) continue;

#ifdef EC_HAVE_CYCLES
        if (master->main_device.cycles_poll - datagram->cycles_sent
            > cycles_timeout) {
#else
        diff_ms = (master->main_device.jiffies_poll
                - datagram->jiffies_sent) * 1000 / HZ;
        if (diff_ms > timeout_ms) {
#endif
            frames_timed_out = 1;
            list_del_init(&datagram->queue);
            datagram->state = EC_DATAGRAM_TIMED_OUT;
            master->stats.timeouts++;
            ec_master_output_stats(master);

            if (unlikely(master->debug_level > 0)) {
                EC_DBG("TIMED OUT datagram %08x, index %02X waited %u us.\n",
                        (unsigned int) datagram, datagram->index,
#ifdef EC_HAVE_CYCLES
                        (unsigned int) (master->main_device.cycles_poll
                            - datagram->cycles_sent) * 1000 / cpu_khz
#else
                        (unsigned int) (diff_ms * 1000)
#endif
                        );
                
            }
        }
    }

    master->frames_timed_out = frames_timed_out;
}

/*****************************************************************************/

ec_slave_config_t *ecrt_master_slave_config(ec_master_t *master,
        uint16_t alias, uint16_t position, uint32_t vendor_id,
        uint32_t product_code)
{
    ec_slave_config_t *sc;
    unsigned int found = 0;

    list_for_each_entry(sc, &master->configs, list) {
        if (sc->alias == alias && sc->position == position) {
            found = 1;
            break;
        }
    }

    if (found) { // config with same alias/position already existing
        if (sc->vendor_id != vendor_id || sc->product_code != product_code) {
            EC_ERR("Slave type mismatch. Slave was configured as"
                    " 0x%08X/0x%08X before. Now configuring with"
                    " 0x%08X/0x%08X.\n", sc->vendor_id, sc->product_code,
                    vendor_id, product_code);
            return NULL;
        }
    } else {
        if (master->debug_level)
            EC_DBG("Creating slave configuration for %u:%u, 0x%08X/0x%08X.\n",
                    alias, position, vendor_id, product_code);

        if (!(sc = (ec_slave_config_t *) kmalloc(sizeof(ec_slave_config_t),
                        GFP_KERNEL))) {
            EC_ERR("Failed to allocate memory for slave configuration.\n");
            return NULL;
        }

        ec_slave_config_init(sc, master,
                alias, position, vendor_id, product_code);


        down(&master->master_sem);

        // try to find the addressed slave
        ec_slave_config_attach(sc);
        ec_slave_config_load_default_sync_config(sc);
        list_add_tail(&sc->list, &master->configs);

        up(&master->master_sem);
    }

    return sc;
}

/*****************************************************************************/

void ecrt_master_callbacks(ec_master_t *master, int (*request_cb)(void *),
        void (*release_cb)(void *), void *cb_data)
{
    master->ext_request_cb = request_cb;
    master->ext_release_cb = release_cb;
    master->ext_cb_data = cb_data;
}

/*****************************************************************************/

void ecrt_master_state(const ec_master_t *master, ec_master_state_t *state)
{
    state->slaves_responding = master->fsm.slaves_responding;
    state->al_states = master->fsm.slave_states;
    state->link_up = master->main_device.link_state;
}

/*****************************************************************************/

/** \cond */

EXPORT_SYMBOL(ecrt_master_create_domain);
EXPORT_SYMBOL(ecrt_master_activate);
EXPORT_SYMBOL(ecrt_master_send);
EXPORT_SYMBOL(ecrt_master_receive);
EXPORT_SYMBOL(ecrt_master_callbacks);
EXPORT_SYMBOL(ecrt_master_slave_config);
EXPORT_SYMBOL(ecrt_master_state);

/** \endcond */

/*****************************************************************************/
