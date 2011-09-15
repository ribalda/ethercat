/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2008  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version 2, as
 *  published by the Free Software Foundation.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  ---
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 *  vim: expandtab
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
#include <linux/hrtimer.h>
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

/** Set to 1 to enable fsm datagram injection debugging.
 */
#ifdef USE_TRACE_PRINTK
#define DEBUG_INJECT 1
#else
#define DEBUG_INJECT 0
#endif

#ifdef EC_HAVE_CYCLES

/** Frame timeout in cycles.
 */
static cycles_t timeout_cycles;

/** Timeout for fsm datagram injection [cycles].
 */
static cycles_t fsm_injection_timeout_cycles;

#else

/** Frame timeout in jiffies.
 */
static unsigned long timeout_jiffies;

/** Timeout for fsm datagram injection [jiffies].
 */
static unsigned long fsm_injection_timeout_jiffies;

#endif

/*****************************************************************************/

void ec_master_clear_slave_configs(ec_master_t *);
void ec_master_clear_domains(ec_master_t *);
static int ec_master_idle_thread(void *);
static int ec_master_operation_thread(void *);
#ifdef EC_EOE
static int ec_master_eoe_processing(ec_master_t *);
#endif
void ec_master_find_dc_ref_clock(ec_master_t *);

/*****************************************************************************/

/** Static variables initializer.
*/
void ec_master_init_static(void)
{
#ifdef EC_HAVE_CYCLES
    timeout_cycles = (cycles_t) EC_IO_TIMEOUT /* us */ * (cpu_khz / 1000);
    fsm_injection_timeout_cycles = (cycles_t) EC_FSM_INJECTION_TIMEOUT /* us */ * (cpu_khz / 1000);
#else
    // one jiffy may always elapse between time measurement
    timeout_jiffies = max(EC_IO_TIMEOUT * HZ / 1000000, 1);
    fsm_injection_timeout_jiffies = max(EC_FSM_INJECTION_TIMEOUT * HZ / 1000000, 1);
#endif
}

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
        struct class *class, /**< Device class. */
        unsigned int debug_level /**< Debug level (module parameter). */
        )
{
    int ret;

    master->index = index;
    master->reserved = 0;

    ec_mutex_init(&master->master_mutex);

    master->main_mac = main_mac;
    master->backup_mac = backup_mac;

    ec_mutex_init(&master->device_mutex);

    master->phase = EC_ORPHANED;
    master->active = 0;
    master->config_changed = 0;
    master->injection_seq_fsm = 0;
    master->injection_seq_rt = 0;

    master->slaves = NULL;
    master->slave_count = 0;

    INIT_LIST_HEAD(&master->configs);

    master->app_time = 0ULL;
#ifdef EC_HAVE_CYCLES
    master->dc_cycles_app_start_time = 0;
#endif
    master->dc_jiffies_app_start_time = 0;
    master->app_start_time = 0ULL;
    master->has_app_time = 0;

    master->scan_busy = 0;
    master->allow_scan = 1;
    ec_mutex_init(&master->scan_mutex);
    init_waitqueue_head(&master->scan_queue);

    master->config_busy = 0;
    master->allow_config = 1;
    ec_mutex_init(&master->config_mutex);
    init_waitqueue_head(&master->config_queue);
    
    INIT_LIST_HEAD(&master->datagram_queue);
    master->datagram_index = 0;

    ec_mutex_init(&master->fsm_queue_mutex);
    INIT_LIST_HEAD(&master->fsm_datagram_queue);
    
    // send interval in IDLE phase
    ec_master_set_send_interval(master, 1000000 / HZ);

    INIT_LIST_HEAD(&master->domains);

    master->debug_level = debug_level;
    master->stats.timeouts = 0;
    master->stats.corrupted = 0;
    master->stats.unmatched = 0;
    master->stats.output_jiffies = 0;

    master->thread = NULL;

#ifdef EC_EOE
    master->eoe_thread = NULL;
    INIT_LIST_HEAD(&master->eoe_handlers);
#endif

    ec_mutex_init(&master->io_mutex);
    master->fsm_queue_lock_cb = NULL;
    master->fsm_queue_unlock_cb = NULL;
    master->fsm_queue_locking_data = NULL;
    master->app_fsm_queue_lock_cb = NULL;
    master->app_fsm_queue_unlock_cb = NULL;
    master->app_fsm_queue_locking_data = NULL;

    INIT_LIST_HEAD(&master->sii_requests);
    init_waitqueue_head(&master->sii_queue);

    INIT_LIST_HEAD(&master->reg_requests);
    init_waitqueue_head(&master->reg_queue);

    // init devices
    ret = ec_device_init(&master->main_device, master);
    if (ret < 0)
        goto out_return;

    ret = ec_device_init(&master->backup_device, master);
    if (ret < 0)
        goto out_clear_main;

    // init state machine datagram
    ec_datagram_init(&master->fsm_datagram);
    snprintf(master->fsm_datagram.name, EC_DATAGRAM_NAME_SIZE, "master-fsm");
    ret = ec_datagram_prealloc(&master->fsm_datagram, EC_MAX_DATA_SIZE);
    if (ret < 0) {
        ec_datagram_clear(&master->fsm_datagram);
        EC_MASTER_ERR(master, "Failed to allocate FSM datagram.\n");
        goto out_clear_backup;
    }

    // create state machine object
    ec_mbox_init(&master->fsm_mbox,&master->fsm_datagram);
    ec_fsm_master_init(&master->fsm, master, &master->fsm_datagram);

    // init reference sync datagram
    ec_datagram_init(&master->ref_sync_datagram);
    snprintf(master->ref_sync_datagram.name, EC_DATAGRAM_NAME_SIZE, "refsync");
    ret = ec_datagram_apwr(&master->ref_sync_datagram, 0, 0x0910, 8);
    if (ret < 0) {
        ec_datagram_clear(&master->ref_sync_datagram);
        EC_MASTER_ERR(master, "Failed to allocate reference"
                " synchronisation datagram.\n");
        goto out_clear_fsm;
    }

    // init sync datagram
    ec_datagram_init(&master->sync_datagram);
    snprintf(master->sync_datagram.name, EC_DATAGRAM_NAME_SIZE, "sync");
    ret = ec_datagram_prealloc(&master->sync_datagram, 4);
    if (ret < 0) {
        ec_datagram_clear(&master->sync_datagram);
        EC_MASTER_ERR(master, "Failed to allocate"
                " synchronisation datagram.\n");
        goto out_clear_ref_sync;
    }

    // init sync monitor datagram
    ec_datagram_init(&master->sync_mon_datagram);
    snprintf(master->sync_mon_datagram.name, EC_DATAGRAM_NAME_SIZE, "syncmon");
    ret = ec_datagram_brd(&master->sync_mon_datagram, 0x092c, 4);
    if (ret < 0) {
        ec_datagram_clear(&master->sync_mon_datagram);
        EC_MASTER_ERR(master, "Failed to allocate sync"
                " monitoring datagram.\n");
        goto out_clear_sync;
    }

    ec_master_find_dc_ref_clock(master);

    // init character device
    ret = ec_cdev_init(&master->cdev, master, device_number);
    if (ret)
        goto out_clear_sync_mon;
    
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
    master->class_device = device_create(class, NULL,
            MKDEV(MAJOR(device_number), master->index), NULL,
            "EtherCAT%u", master->index);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
    master->class_device = device_create(class, NULL,
            MKDEV(MAJOR(device_number), master->index),
            "EtherCAT%u", master->index);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 15)
    master->class_device = class_device_create(class, NULL,
            MKDEV(MAJOR(device_number), master->index), NULL,
            "EtherCAT%u", master->index);
#else
    master->class_device = class_device_create(class,
            MKDEV(MAJOR(device_number), master->index), NULL,
            "EtherCAT%u", master->index);
#endif
    if (IS_ERR(master->class_device)) {
        EC_MASTER_ERR(master, "Failed to create class device!\n");
        ret = PTR_ERR(master->class_device);
        goto out_clear_cdev;
    }

    return 0;

out_clear_cdev:
    ec_cdev_clear(&master->cdev);
out_clear_sync_mon:
    ec_datagram_clear(&master->sync_mon_datagram);
out_clear_sync:
    ec_datagram_clear(&master->sync_datagram);
out_clear_ref_sync:
    ec_datagram_clear(&master->ref_sync_datagram);
out_clear_fsm:
    ec_fsm_master_clear(&master->fsm);
    ec_datagram_clear(&master->fsm_datagram);
out_clear_backup:
    ec_device_clear(&master->backup_device);
out_clear_main:
    ec_device_clear(&master->main_device);
out_return:
    return ret;
}

/*****************************************************************************/

/** Destructor.
*/
void ec_master_clear(
        ec_master_t *master /**< EtherCAT master */
        )
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
    device_unregister(master->class_device);
#else
    class_device_unregister(master->class_device);
#endif

    ec_cdev_clear(&master->cdev);

#ifdef EC_EOE
    ec_master_clear_eoe_handlers(master);
#endif
    ec_master_clear_domains(master);
    ec_master_clear_slave_configs(master);
    ec_master_clear_slaves(master);

    ec_datagram_clear(&master->sync_mon_datagram);
    ec_datagram_clear(&master->sync_datagram);
    ec_datagram_clear(&master->ref_sync_datagram);
    ec_fsm_master_clear(&master->fsm);
    ec_mbox_clear(&master->fsm_mbox);
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

    master->dc_ref_clock = NULL;

    // external requests are obsolete, so we wake pending waiters and remove
    // them from the list

    while (!list_empty(&master->sii_requests)) {
        ec_sii_write_request_t *request =
            list_entry(master->sii_requests.next,
                    ec_sii_write_request_t, list);
        list_del_init(&request->list); // dequeue
        EC_MASTER_WARN(master, "Discarding SII request, slave %u about"
                " to be deleted.\n", request->slave->ring_position);
        request->state = EC_INT_REQUEST_FAILURE;
        kref_put(&request->refcount,ec_master_sii_write_request_release);
        wake_up(&master->sii_queue);
    }

    while (!list_empty(&master->reg_requests)) {
        ec_reg_request_t *request =
            list_entry(master->reg_requests.next, ec_reg_request_t, list);
        list_del_init(&request->list); // dequeue
        EC_MASTER_WARN(master, "Discarding register request, slave %u"
                " about to be deleted.\n", request->slave->ring_position);
        request->state = EC_INT_REQUEST_FAILURE;
        kref_put(&request->refcount,ec_master_reg_request_release);
        wake_up(&master->reg_queue);
    }

    // we must lock the io_mutex here because the slave's fsm_datagram
    // will be unqueued
    ec_mutex_lock(&master->io_mutex);
    for (slave = master->slaves;
            slave < master->slaves + master->slave_count;
            slave++) {
        ec_slave_clear(slave);
    }
    ec_mutex_unlock(&master->io_mutex);

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

    // we must lock the io_mutex here because the domains's datagram
    // will be unqueued
    ec_mutex_lock(&master->io_mutex);
    list_for_each_entry_safe(domain, next, &master->domains, list) {
        list_del(&domain->list);
        ec_domain_clear(domain);
        kfree(domain);
    }
    ec_mutex_unlock(&master->io_mutex);
}

/*****************************************************************************/

/** Clear the configuration applied by the application.
 */
void ec_master_clear_config(
        ec_master_t *master /**< EtherCAT master. */
        )
{
    ec_mutex_lock(&master->master_mutex);
    ec_master_clear_domains(master);
    ec_master_clear_slave_configs(master);
    ec_mutex_unlock(&master->master_mutex);
}

/*****************************************************************************/

/** Starts the master thread.
 *
 * \retval  0 Success.
 * \retval <0 Error code.
 */
int ec_master_thread_start(
        ec_master_t *master, /**< EtherCAT master */
        int (*thread_func)(void *), /**< thread function to start */
        const char *name /**< Thread name. */
        )
{
    EC_MASTER_INFO(master, "Starting %s thread.\n", name);
    master->thread = kthread_run(thread_func, master, name);
    if (IS_ERR(master->thread)) {
        int err = (int) PTR_ERR(master->thread);
        EC_MASTER_ERR(master, "Failed to start master thread (error %i)!\n",
                err);
        master->thread = NULL;
        return err;
    }
    
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
    
    if (!master->thread) {
        EC_MASTER_WARN(master, "%s(): Already finished!\n", __func__);
        return;
    }

    EC_MASTER_DBG(master, 1, "Stopping master thread.\n");

    kthread_stop(master->thread);
    master->thread = NULL;
    EC_MASTER_INFO(master, "Master thread exited.\n");

    if (master->fsm_datagram.state != EC_DATAGRAM_SENT)
        return;
    
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
    int ret;

    EC_MASTER_DBG(master, 1, "ORPHANED -> IDLE.\n");

    master->fsm_queue_lock_cb = NULL;
    master->fsm_queue_unlock_cb = NULL;
    master->fsm_queue_locking_data = NULL;

    master->phase = EC_IDLE;
    ret = ec_master_thread_start(master, ec_master_idle_thread,
            "EtherCAT-IDLE");
    if (ret)
        master->phase = EC_ORPHANED;

    return ret;
}

/*****************************************************************************/

/** Transition function from IDLE to ORPHANED phase.
 */
void ec_master_leave_idle_phase(ec_master_t *master /**< EtherCAT master */)
{
    EC_MASTER_DBG(master, 1, "IDLE -> ORPHANED.\n");

    master->phase = EC_ORPHANED;
    
    ec_master_thread_stop(master);

    ec_mutex_lock(&master->master_mutex);
    ec_master_clear_slaves(master);
    ec_mutex_unlock(&master->master_mutex);
}

/*****************************************************************************/

/** Transition function from IDLE to OPERATION phase.
 */
int ec_master_enter_operation_phase(i
        ec_master_t *master /**< EtherCAT master */
        )
{
    int ret = 0;
    ec_slave_t *slave;
#ifdef EC_EOE
    ec_eoe_t *eoe;
#endif

    EC_MASTER_DBG(master, 1, "IDLE -> OPERATION.\n");

    ec_mutex_lock(&master->config_mutex);
    master->allow_config = 0; // temporarily disable slave configuration
    if (master->config_busy) {
        ec_mutex_unlock(&master->config_mutex);

        // wait for slave configuration to complete
        ret = wait_event_interruptible(master->config_queue,
                    !master->config_busy);
        if (ret) {
            EC_MASTER_INFO(master, "Finishing slave configuration"
                    " interrupted by signal.\n");
            goto out_allow;
        }

        EC_MASTER_DBG(master, 1, "Waiting for pending slave"
                " configuration returned.\n");
    } else {
        ec_mutex_unlock(&master->config_mutex);
    }

    ec_mutex_lock(&master->scan_mutex);
    master->allow_scan = 0; // 'lock' the slave list
    if (!master->scan_busy) {
        ec_mutex_unlock(&master->scan_mutex);
    } else {
        ec_mutex_unlock(&master->scan_mutex);

        // wait for slave scan to complete
        ret = wait_event_interruptible(master->scan_queue, !master->scan_busy);
        if (ret) {
            EC_MASTER_INFO(master, "Waiting for slave scan"
                    " interrupted by signal.\n");
            goto out_allow;
        }
        
        EC_MASTER_DBG(master, 1, "Waiting for pending"
                " slave scan returned.\n");
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
    master->app_fsm_queue_lock_cb = NULL;
    master->app_fsm_queue_unlock_cb = NULL;
    master->app_fsm_queue_locking_data = NULL;
    return ret;
    
out_allow:
    master->allow_scan = 1;
    master->allow_config = 1;
    return ret;
}

/*****************************************************************************/

/** Transition function from OPERATION to IDLE phase.
 */
void ec_master_leave_operation_phase(
        ec_master_t *master /**< EtherCAT master */
        )
{
    if (master->active) {
        ecrt_master_deactivate(master); // also clears config
    } else {
        ec_master_clear_config(master);
    }

    EC_MASTER_DBG(master, 1, "OPERATION -> IDLE.\n");

    master->phase = EC_IDLE;
}

/*****************************************************************************/

/** Injects fsm datagrams that fit into the datagram queue.
 */
void ec_master_inject_fsm_datagrams(
        ec_master_t *master /**< EtherCAT master */
        )
{
    ec_datagram_t *datagram, *next;
    size_t queue_size = 0;

    if (master->fsm_queue_lock_cb) {
        master->fsm_queue_lock_cb(master->fsm_queue_locking_data);
    }

    if (ec_mutex_trylock(&master->fsm_queue_mutex) == 0) {
        goto unlock_cb;
    }

    if (list_empty(&master->fsm_datagram_queue)) {
        goto unlock;
    }

    list_for_each_entry(datagram, &master->datagram_queue, queue) {
        queue_size += datagram->data_size;
    }

    list_for_each_entry_safe(datagram, next, &master->fsm_datagram_queue,
            fsm_queue) {
        queue_size += datagram->data_size;
        if (queue_size <= master->max_queue_size) {
            list_del_init(&datagram->fsm_queue);
#if DEBUG_INJECT
            EC_MASTER_DBG(master, 2, "Injecting fsm datagram %p"
                    " size=%zu, queue_size=%zu\n", datagram,
                    datagram->data_size, queue_size);
#endif
#ifdef EC_HAVE_CYCLES
            datagram->cycles_sent = 0;
#endif
            datagram->jiffies_sent = 0; // FIXME why?
            ec_master_queue_datagram(master, datagram);
        } else {
            if (datagram->data_size > master->max_queue_size) {
                list_del_init(&datagram->fsm_queue);
                datagram->state = EC_DATAGRAM_ERROR;
                EC_MASTER_ERR(master, "Fsm datagram %p is too large,"
                        " size=%zu, max_queue_size=%zu\n",
                        datagram, datagram->data_size,
                        master->max_queue_size);
            } else {
#ifdef EC_HAVE_CYCLES
                cycles_t cycles_now = get_cycles();

                if (cycles_now - datagram->cycles_sent
                        > fsm_injection_timeout_cycles)
#else
                if (jiffies - datagram->jiffies_sent
                        > fsm_injection_timeout_jiffies)
#endif
                {
                    unsigned int time_us;

                    list_del_init(&datagram->fsm_queue);
                    datagram->state = EC_DATAGRAM_ERROR;
#ifdef EC_HAVE_CYCLES
                    time_us = (unsigned int)
                        ((cycles_now - datagram->cycles_sent) * 1000LL)
                        / cpu_khz;
#else
                    time_us = (unsigned int)
                        ((jiffies - datagram->jiffies_sent) * 1000000 / HZ);
#endif
                    EC_MASTER_ERR(master, "Timeout %u us: Injecting"
                            " fsm datagram %p size=%zu,"
                            " max_queue_size=%zu\n", time_us, datagram,
                            datagram->data_size, master->max_queue_size);
                }
#if DEBUG_INJECT
                else {
                    EC_MASTER_DBG(master, 2, "Deferred injecting"
                            " of fsm datagram %p"
                            " size=%zu, queue_size=%zu\n",
                            datagram, datagram->data_size, queue_size);
                }
#endif
            }
        }
    }

unlock:
    ec_mutex_unlock(&master->fsm_queue_mutex);
unlock_cb:
    if (master->fsm_queue_unlock_cb) {
        master->fsm_queue_unlock_cb(master->fsm_queue_locking_data);
    }
}

/*****************************************************************************/

/** Sets the expected interval between calls to ecrt_master_send
 * and calculates the maximum amount of data to queue.
 */
void ec_master_set_send_interval(
        ec_master_t *master, /**< EtherCAT master */
        unsigned int send_interval /**< Send interval */
        )
{
    master->send_interval = send_interval;
    master->max_queue_size =
        (send_interval * 1000) / EC_BYTE_TRANSMISSION_TIME_NS;
    master->max_queue_size -= master->max_queue_size / 10;
}

/*****************************************************************************/

/** Places an request (SDO/FoE/SoE/EoE) fsm datagram in the sdo datagram
 * queue.
 */
void ec_master_queue_request_fsm_datagram(
        ec_master_t *master, /**< EtherCAT master */
        ec_datagram_t *datagram /**< datagram */
        )
{
    ec_master_queue_fsm_datagram(master, datagram);
    master->fsm.idle = 0; // pump the bus as fast as possible
}

/*****************************************************************************/

/** Places an fsm datagram in the sdo datagram queue.
 */
void ec_master_queue_fsm_datagram(
        ec_master_t *master, /**< EtherCAT master */
        ec_datagram_t *datagram /**< datagram */
        )
{
    ec_datagram_t *queued_datagram;

    if (master->fsm_queue_lock_cb) {
        master->fsm_queue_lock_cb(master->fsm_queue_locking_data);
    }
    ec_mutex_lock(&master->fsm_queue_mutex);

    // check, if the datagram is already queued
    list_for_each_entry(queued_datagram, &master->fsm_datagram_queue,
            fsm_queue) {
        if (queued_datagram == datagram) {
            datagram->state = EC_DATAGRAM_QUEUED;
            goto unlock;
        }
    }

#if DEBUG_INJECT
    EC_MASTER_DBG(master, 2, "Requesting fsm datagram %p size=%zu\n",
            datagram, datagram->data_size);
#endif

    list_add_tail(&datagram->fsm_queue, &master->fsm_datagram_queue);
    datagram->state = EC_DATAGRAM_QUEUED;
#ifdef EC_HAVE_CYCLES
    datagram->cycles_sent = get_cycles();
#endif
    datagram->jiffies_sent = jiffies; // FIXME why?

unlock:
    ec_mutex_unlock(&master->fsm_queue_mutex);
    if (master->fsm_queue_unlock_cb) {
        master->fsm_queue_unlock_cb(master->fsm_queue_locking_data);
    }
}

/*****************************************************************************/

/** Places a datagram in the datagram queue.
 */
void ec_master_queue_datagram(
        ec_master_t *master, /**< EtherCAT master */
        ec_datagram_t *datagram /**< datagram */
        )
{
    ec_datagram_t *queued_datagram;

    /* It is possible, that a datagram in the queue is re-initialized with the
     * ec_datagram_<type>() methods and then shall be queued with this method.
     * In that case, the state is already reset to EC_DATAGRAM_INIT. Check if
     * the datagram is queued to avoid duplicate queuing (which results in an
     * infinite loop!). Set the state to EC_DATAGRAM_QUEUED again, probably
     * causing an unmatched datagram. */
    list_for_each_entry(queued_datagram, &master->datagram_queue, queue) {
        if (queued_datagram == datagram) {
            datagram->skip_count++;
            if (master->debug_level) {
                EC_MASTER_DBG(master, 1, "Skipping datagram %p (", datagram);
                ec_datagram_output_info(datagram);
                printk(")\n");
            }
            goto queued;
        }
    }

    list_add_tail(&datagram->queue, &master->datagram_queue);
queued:
    datagram->state = EC_DATAGRAM_QUEUED;
}

/*****************************************************************************/

/** Sends the datagrams in the queue.
 *
 */
void ec_master_send_datagrams(ec_master_t *master /**< EtherCAT master */)
{
    ec_datagram_t *datagram, *next;
    size_t datagram_size;
    uint8_t *frame_data, *cur_data, *frame_datagram_data;
    void *follows_word;
#ifdef EC_HAVE_CYCLES
    cycles_t cycles_start, cycles_sent, cycles_end;
#endif
    unsigned long jiffies_sent;
    unsigned int frame_count, more_datagrams_waiting;
    struct list_head sent_datagrams;
    ec_fmmu_config_t* domain_fmmu;

#ifdef EC_HAVE_CYCLES
    cycles_start = get_cycles();
#endif
    frame_count = 0;
    INIT_LIST_HEAD(&sent_datagrams);

    EC_MASTER_DBG(master, 2, "ec_master_send_datagrams\n");

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

            EC_MASTER_DBG(master, 2, "Adding datagram %p i=0x%02X size=%zu\n",
                    datagram, datagram->index, datagram_size);

            // set "datagram following" flag in previous frame
            if (follows_word) {
                EC_WRITE_U16(follows_word,
                        EC_READ_U16(follows_word) | 0x8000);
            }

            // EtherCAT datagram header
            EC_WRITE_U8 (cur_data,     datagram->type);
            EC_WRITE_U8 (cur_data + 1, datagram->index);
            memcpy(cur_data + 2, datagram->address, EC_ADDR_LEN);
            EC_WRITE_U16(cur_data + 6, datagram->data_size & 0x7FF);
            EC_WRITE_U16(cur_data + 8, 0x0000);
            follows_word = cur_data + 6;
            cur_data += EC_DATAGRAM_HEADER_SIZE;

            // EtherCAT datagram data
            frame_datagram_data = cur_data;

            // distinguish between domain and non-domain datagrams...
            // this is not nice... FIXME
            if (datagram->domain) {
                unsigned int datagram_address =
                    EC_READ_U32(datagram->address);
                int i = 0;
                uint8_t *domain_data = datagram->data;

                // FIXME all FMMU configs are taken into acount,
                // maybe the belong to another datagram?
                // test with large process data!

                list_for_each_entry(domain_fmmu,
                        &datagram->domain->fmmu_configs, list) {
                    if (domain_fmmu->dir == EC_DIR_OUTPUT) {
                        unsigned int frame_offset =
                            domain_fmmu->logical_start_address
                            - datagram_address;
                        memcpy(frame_datagram_data + frame_offset,
                                domain_data, domain_fmmu->data_size);
                        if (unlikely(master->debug_level > 1)) {
                            EC_MASTER_DBG(master, 0, "Sending datagram %p"
                                    " i=0x%02X FMMU %u fp=%u"
                                    " dp=%zu size=%u\n",
                                   datagram, datagram->index, i, frame_offset,
                                   domain_data - datagram->data,
                                   domain_fmmu->data_size);
                            ec_print_data(domain_data,
                                    domain_fmmu->data_size);
                        }
                    }
                    domain_data += domain_fmmu->data_size;
                    i++;
                }
            } else {
                memcpy(frame_datagram_data, datagram->data,
                        datagram->data_size);
            }
            cur_data += datagram->data_size;

            // EtherCAT datagram footer
            EC_WRITE_U16(cur_data, 0x0000); // reset working counter
            cur_data += EC_DATAGRAM_FOOTER_SIZE;
        }

        if (list_empty(&sent_datagrams)) {
            EC_MASTER_DBG(master, 2, "nothing to send.\n");
            break;
        }

        // EtherCAT frame header
        EC_WRITE_U16(frame_data, ((cur_data - frame_data
                                   - EC_FRAME_HEADER_SIZE) & 0x7FF) | 0x1000);

        // pad frame
        while (cur_data - frame_data < ETH_ZLEN - ETH_HLEN)
            EC_WRITE_U8(cur_data++, 0x00);

        EC_MASTER_DBG(master, 2, "frame size: %zu\n", cur_data - frame_data);

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
        EC_MASTER_DBG(master, 0, "ec_master_send_datagrams"
                " sent %u frames in %uus.\n", frame_count,
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
void ec_master_receive_datagrams(
        ec_master_t *master, /**< EtherCAT master */
        const uint8_t *frame_data, /**< Frame data */
        size_t size /**< Size of the received data */
        )
{
    size_t frame_size, data_size;
    uint8_t datagram_type, datagram_index;
    unsigned int datagram_follows, matched;
    const uint8_t *cur_data, *frame_datagram_data;
    ec_datagram_t *datagram;
    ec_fmmu_config_t* domain_fmmu;

    if (unlikely(size < EC_FRAME_HEADER_SIZE)) {
        if (master->debug_level) {
            EC_MASTER_DBG(master, 0, "Corrupted frame received"
                    " (size %zu < %u byte):\n",
                    size, EC_FRAME_HEADER_SIZE);
            ec_print_data(frame_data, size);
        }
        master->stats.corrupted++;
        ec_master_output_stats(master);
        return;
    }

    cur_data = frame_data;

    // check length of entire frame
    frame_size = EC_READ_U16(cur_data) & 0x07FF;
    cur_data += EC_FRAME_HEADER_SIZE;

    if (unlikely(frame_size > size)) {
        if (master->debug_level) {
            EC_MASTER_DBG(master, 0, "Corrupted frame received"
                    " (invalid frame size %zu for "
                    "received size %zu):\n", frame_size, size);
            ec_print_data(frame_data, size);
        }
        master->stats.corrupted++;
        ec_master_output_stats(master);
        return;
    }

    datagram_follows = 1;

    while (datagram_follows) {

        // process datagram header
        datagram_type = EC_READ_U8(cur_data);
        datagram_index = EC_READ_U8(cur_data + 1);
        data_size = EC_READ_U16(cur_data + 6) & 0x07FF;
        datagram_follows = EC_READ_U16(cur_data + 6) & 0x8000;
        cur_data += EC_DATAGRAM_HEADER_SIZE;

        if (unlikely(cur_data - frame_data
                     + data_size + EC_DATAGRAM_FOOTER_SIZE > size)) {
            if (master->debug_level) {
                EC_MASTER_DBG(master, 0, "Corrupted frame received"
                        " (invalid data size %zu):\n", data_size);
                ec_print_data(frame_data, size);
            }
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
                EC_MASTER_DBG(master, 0, "UNMATCHED datagram:\n");
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

        frame_datagram_data = cur_data;

        // distinguish between domain and non-domain datagrams
        // this is not nice FIXME
        if (datagram->domain) {
            size_t datagram_address = EC_READ_U32(datagram->address);
            int i = 0;
            uint8_t *domain_data = datagram->data;

            // FIXME see ecrt_master_send_datagrams()
            // it is not correct to walk though *all* FMMU configs,
            // because they may not all belong to the same frame!

            list_for_each_entry(domain_fmmu, &datagram->domain->fmmu_configs,
                    list) {
                if (domain_fmmu->dir == EC_DIR_INPUT) {
                    unsigned int frame_offset =
                        domain_fmmu->logical_start_address - datagram_address;
                    memcpy(domain_data, frame_datagram_data + frame_offset,
                            domain_fmmu->data_size);
                    if (unlikely(master->debug_level > 1)) {
                        EC_MASTER_DBG(master, 0, "Receiving datagram %p"
                                " i=0x%02X fmmu %u fp=%u"
                                " dp=%zu size=%u\n",
                               datagram, datagram->index, i,
                               frame_offset, domain_data - datagram->data,
                               domain_fmmu->data_size);
                        ec_print_data(domain_data, domain_fmmu->data_size);
                    }
                }
                domain_data += domain_fmmu->data_size;
                i++;
            }
        } else {
            // copy received data into the datagram memory
            memcpy(datagram->data, frame_datagram_data, data_size);
        }

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
        EC_MASTER_DBG(master, 2, "removing datagram %p i=0x%02X\n",datagram,
                datagram->index);
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
            EC_MASTER_WARN(master, "%u datagram%s TIMED OUT!\n",
                    master->stats.timeouts,
                    master->stats.timeouts == 1 ? "" : "s");
            master->stats.timeouts = 0;
        }
        if (master->stats.corrupted) {
            EC_MASTER_WARN(master, "%u frame%s CORRUPTED!\n",
                    master->stats.corrupted,
                    master->stats.corrupted == 1 ? "" : "s");
            master->stats.corrupted = 0;
        }
        if (master->stats.unmatched) {
            EC_MASTER_WARN(master, "%u datagram%s UNMATCHED!\n",
                    master->stats.unmatched,
                    master->stats.unmatched == 1 ? "" : "s");
            master->stats.unmatched = 0;
        }
    }
}


/*****************************************************************************/

#ifdef EC_USE_HRTIMER

/*
 * Sleep related functions:
 */
static enum hrtimer_restart ec_master_nanosleep_wakeup(struct hrtimer *timer)
{
    struct hrtimer_sleeper *t =
        container_of(timer, struct hrtimer_sleeper, timer);
    struct task_struct *task = t->task;

    t->task = NULL;
    if (task)
        wake_up_process(task);

    return HRTIMER_NORESTART;
}

/*****************************************************************************/

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)

/* compatibility with new hrtimer interface */
static inline ktime_t hrtimer_get_expires(const struct hrtimer *timer)
{
    return timer->expires;
}

/*****************************************************************************/

static inline void hrtimer_set_expires(struct hrtimer *timer, ktime_t time)
{
    timer->expires = time;
}

#endif

/*****************************************************************************/

void ec_master_nanosleep(const unsigned long nsecs)
{
    struct hrtimer_sleeper t;
    enum hrtimer_mode mode = HRTIMER_MODE_REL;

    hrtimer_init(&t.timer, CLOCK_MONOTONIC, mode);
    t.timer.function = ec_master_nanosleep_wakeup;
    t.task = current;
#ifdef CONFIG_HIGH_RES_TIMERS
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 24)
    t.timer.cb_mode = HRTIMER_CB_IRQSAFE_NO_RESTART;
#elif LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 26)
    t.timer.cb_mode = HRTIMER_CB_IRQSAFE_NO_SOFTIRQ;
#elif LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 28)
    t.timer.cb_mode = HRTIMER_CB_IRQSAFE_UNLOCKED;
#endif
#endif
    hrtimer_set_expires(&t.timer, ktime_set(0, nsecs));

    do {
        set_current_state(TASK_INTERRUPTIBLE);
        hrtimer_start(&t.timer, hrtimer_get_expires(&t.timer), mode);

        if (likely(t.task))
            schedule();

        hrtimer_cancel(&t.timer);
        mode = HRTIMER_MODE_ABS;

    } while (t.task && !signal_pending(current));
}

#endif // EC_USE_HRTIMER

/*****************************************************************************/

/** Master kernel thread function for IDLE phase.
 */
static int ec_master_idle_thread(void *priv_data)
{
    ec_master_t *master = (ec_master_t *) priv_data;
    ec_slave_t *slave = NULL;
    size_t sent_bytes;

    // send interval in IDLE phase
    ec_master_set_send_interval(master, 1000000 / HZ); 

    EC_MASTER_DBG(master, 1, "Idle thread running with send interval = %u us,"
            " max data size=%zu\n", master->send_interval,
            master->max_queue_size);

    while (!kthread_should_stop()) {
        ec_datagram_output_stats(&master->fsm_datagram);

        // receive
        ec_mutex_lock(&master->io_mutex);
        ecrt_master_receive(master);
        ec_mutex_unlock(&master->io_mutex);

        // execute master & slave state machines
        if (ec_mutex_lock_interruptible(&master->master_mutex)) {
            break;
        }
        if (ec_fsm_master_exec(&master->fsm)) {
            ec_master_mbox_queue_datagrams(master, &master->fsm_mbox);
        }
        for (slave = master->slaves;
                slave < master->slaves + master->slave_count;
                slave++) {
            ec_fsm_slave_exec(&slave->fsm); // may queue datagram in fsm queue
        }
#if defined(EC_EOE)
        if (!ec_master_eoe_processing(master)) {
            master->fsm.idle = 0; // pump the bus as fast as possible
        }
#endif
        ec_mutex_unlock(&master->master_mutex);

        // queue and send
        ec_mutex_lock(&master->io_mutex);
        ecrt_master_send(master);
        sent_bytes = master->main_device.tx_skb[
            master->main_device.tx_ring_index]->len;
        ec_mutex_unlock(&master->io_mutex);

        if (ec_fsm_master_idle(&master->fsm)) {
#ifdef EC_USE_HRTIMER
            ec_master_nanosleep(master->send_interval * 1000);
#else
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(1);
#endif
        } else {
#ifdef EC_USE_HRTIMER
            ec_master_nanosleep(sent_bytes * EC_BYTE_TRANSMISSION_TIME_NS);
#else
            schedule();
#endif
        }
    }
    
    EC_MASTER_DBG(master, 1, "Master IDLE thread exiting...\n");

    return 0;
}

/*****************************************************************************/

/** Master kernel thread function for OPERATION phase.
 */
static int ec_master_operation_thread(void *priv_data)
{
    ec_master_t *master = (ec_master_t *) priv_data;
    ec_slave_t *slave = NULL;

    EC_MASTER_DBG(master, 1, "Operation thread running"
            " with fsm interval = %u us, max data size=%zu\n",
            master->send_interval, master->max_queue_size);

    while (!kthread_should_stop()) {
        ec_datagram_output_stats(&master->fsm_datagram);

        // output statistics
        ec_master_output_stats(master);

        // execute master & slave state machines
        if (ec_mutex_lock_interruptible(&master->master_mutex)) {
            break;
        }
        if (ec_fsm_master_exec(&master->fsm)) {
            ec_master_mbox_queue_datagrams(master, &master->fsm_mbox);
        }
        for (slave = master->slaves;
                slave < master->slaves + master->slave_count;
                slave++) {
            ec_fsm_slave_exec(&slave->fsm); // may queue datagram in fsm queue
        }
#if defined(EC_EOE)
        ec_master_eoe_processing(master);
#endif
        ec_mutex_unlock(&master->master_mutex);

#ifdef EC_USE_HRTIMER
        // the op thread should not work faster than the sending RT thread
        ec_master_nanosleep(master->send_interval * 1000);
#else
        if (ec_fsm_master_idle(&master->fsm)) {
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(1);
        }
        else {
            schedule();
        }
#endif
    }
    
    EC_MASTER_DBG(master, 1, "Master OP thread exiting...\n");
    return 0;
}

/*****************************************************************************/

#ifdef EC_EOE

/*****************************************************************************/

/** Does the Ethernet over EtherCAT processing.
 */
static int ec_master_eoe_processing(ec_master_t *master)
{
    ec_eoe_t *eoe;
    unsigned int none_open, sth_to_send, all_idle;
    none_open = 1;
    all_idle = 1;

    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        if (ec_eoe_is_open(eoe)) {
            none_open = 0;
            break;
        }
    }
    if (none_open)
        return all_idle;

    // actual EoE processing
    sth_to_send = 0;
    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        ec_eoe_run(eoe);
        if (eoe->queue_datagram) {
            sth_to_send = 1;
        }
        if (!ec_eoe_is_idle(eoe)) {
            all_idle = 0;
        }
    }

    if (sth_to_send) {
        list_for_each_entry(eoe, &master->eoe_handlers, list) {
            ec_eoe_queue(eoe);
        }
    }
    return all_idle;
}

#endif // EC_EOE

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
                if (slave->effective_alias == alias) \
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

/** Get the number of slave configurations provided by the application.
 *
 * \return Number of configurations.
 */
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

/** Common implementation for ec_master_get_config()
 * and ec_master_get_config_const().
 */
#define EC_FIND_CONFIG \
    do { \
        list_for_each_entry(sc, &master->configs, list) { \
            if (pos--) \
                continue; \
            return sc; \
        } \
        return NULL; \
    } while (0)

/** Get a slave configuration via its position in the list.
 *
 * \return Slave configuration or \a NULL.
 */
ec_slave_config_t *ec_master_get_config(
        const ec_master_t *master, /**< EtherCAT master. */
        unsigned int pos /**< List position. */
        )
{
    ec_slave_config_t *sc;
    EC_FIND_CONFIG;
}

/** Get a slave configuration via its position in the list.
 *
 * Const version.
 *
 * \return Slave configuration or \a NULL.
 */
const ec_slave_config_t *ec_master_get_config_const(
        const ec_master_t *master, /**< EtherCAT master. */
        unsigned int pos /**< List position. */
        )
{
    const ec_slave_config_t *sc;
    EC_FIND_CONFIG;
}

/*****************************************************************************/

/** Get the number of domains.
 *
 * \return Number of domains.
 */
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

/** Common implementation for ec_master_find_domain() and
 * ec_master_find_domain_const().
 */
#define EC_FIND_DOMAIN \
    do { \
        list_for_each_entry(domain, &master->domains, list) { \
            if (index--) \
                continue; \
            return domain; \
        } \
        \
        return NULL; \
    } while (0)

/** Get a domain via its position in the list.
 *
 * \return Domain pointer, or \a NULL if not found.
 */
ec_domain_t *ec_master_find_domain(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned int index /**< Domain index. */
        )
{
    ec_domain_t *domain;
    EC_FIND_DOMAIN;
}

/** Wrapper Function for external usage
 *
 * \return Domain pointer, or \a NULL if not found.
 */
ec_domain_t *ecrt_master_find_domain(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned int index /**< Domain index. */
        )
{
    return ec_master_find_domain(
        master,
        index);
}

/** Get a domain via its position in the list.
 *
 * Const version.
 *
 * \return Domain pointer, or \a NULL if not found.
 */
const ec_domain_t *ec_master_find_domain_const(
        const ec_master_t *master, /**< EtherCAT master. */
        unsigned int index /**< Domain index. */
        )
{
    const ec_domain_t *domain;
    EC_FIND_DOMAIN;
}

/*****************************************************************************/

#ifdef EC_EOE

/** Get the number of EoE handlers.
 *
 * \return Number of EoE handlers.
 */
uint16_t ec_master_eoe_handler_count(
        const ec_master_t *master /**< EtherCAT master. */
        )
{
    const ec_eoe_t *eoe;
    unsigned int count = 0;

    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        count++;
    }

    return count;
}

/*****************************************************************************/

/** Get an EoE handler via its position in the list.
 *
 * Const version.
 *
 * \return EoE handler pointer, or \a NULL if not found.
 */
const ec_eoe_t *ec_master_get_eoe_handler_const(
        const ec_master_t *master, /**< EtherCAT master. */
        uint16_t index /**< EoE handler index. */
        )
{
    const ec_eoe_t *eoe;

    list_for_each_entry(eoe, &master->eoe_handlers, list) {
        if (index--)
            continue;
        return eoe;
    }

    return NULL;
}

#endif

/*****************************************************************************/

/** Set the debug level.
 *
 * \retval       0 Success.
 * \retval -EINVAL Invalid debug level.
 */
int ec_master_debug_level(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned int level /**< Debug level. May be 0, 1 or 2. */
        )
{
    if (level > 2) {
        EC_MASTER_ERR(master, "Invalid debug level %u!\n", level);
        return -EINVAL;
    }

    if (level != master->debug_level) {
        master->debug_level = level;
        EC_MASTER_INFO(master, "Master debug level set to %u.\n",
                master->debug_level);
    }

    return 0;
}

/*****************************************************************************/

/** Finds the DC reference clock.
 */
void ec_master_find_dc_ref_clock(
        ec_master_t *master /**< EtherCAT master. */
        )
{
    ec_slave_t *slave, *ref = NULL;

    for (slave = master->slaves;
            slave < master->slaves + master->slave_count;
            slave++) {
        if (slave->base_dc_supported && slave->has_dc_system_time) {
            ref = slave;
            break;
        }
    }

    master->dc_ref_clock = ref;
    
    // This call always succeeds, because the datagram has been pre-allocated.
    ec_datagram_frmw(&master->sync_datagram,
            ref ? ref->station_address : 0xffff, 0x0910, 4);
}

/*****************************************************************************/

/** Calculates the bus topology; recursion function.
 */
int ec_master_calc_topology_rec(
        ec_master_t *master, /**< EtherCAT master. */
        ec_slave_t *port0_slave, /**< Slave at port 0. */
        unsigned int *slave_position /**< Slave position. */
        )
{
    ec_slave_t *slave = master->slaves + *slave_position;
    unsigned int i;
    int ret;

    slave->ports[0].next_slave = port0_slave;

    i = 3;
    while (i != 0) {
        if (!slave->ports[i].link.loop_closed) {
            *slave_position = *slave_position + 1;
            if (*slave_position < master->slave_count) {
                slave->ports[i].next_slave = master->slaves + *slave_position;
                ret = ec_master_calc_topology_rec(master,
                        slave, slave_position);
                if (ret)
                    return ret;
            } else {
                return -1;
            }
        }
        switch (i)
        {
        case 0: i = 3; break;
        case 1: i = 2; break;
        case 3: i = 1; break;
        case 2:
        default:i = 0; break;
        }
    }

    return 0;
}

/*****************************************************************************/

/** Calculates the bus topology.
 */
void ec_master_calc_topology(
        ec_master_t *master /**< EtherCAT master. */
        )
{
    unsigned int slave_position = 0;

    if (master->slave_count == 0)
        return;

    if (ec_master_calc_topology_rec(master, NULL, &slave_position))
        EC_MASTER_ERR(master, "Failed to calculate bus topology.\n");
}

/*****************************************************************************/

/** Calculates the bus transmission delays.
 */
void ec_master_calc_transmission_delays(
        ec_master_t *master /**< EtherCAT master. */
        )
{
    ec_slave_t *slave;

    for (slave = master->slaves;
            slave < master->slaves + master->slave_count;
            slave++) {
        ec_slave_calc_port_delays(slave);
    }

    if (master->dc_ref_clock) {
        uint32_t delay = 0;
        ec_slave_calc_transmission_delays_rec(master->dc_ref_clock, &delay);
    }
}

/*****************************************************************************/

/** Distributed-clocks calculations.
 */
void ec_master_calc_dc(
        ec_master_t *master /**< EtherCAT master. */
        )
{
    // find DC reference clock
    ec_master_find_dc_ref_clock(master);

    // calculate bus topology
    ec_master_calc_topology(master);

    ec_master_calc_transmission_delays(master);
}

/*****************************************************************************/

/** Request OP state for configured slaves.
 */
void ec_master_request_op(
        ec_master_t *master /**< EtherCAT master. */
        )
{
    unsigned int i;
    ec_slave_t *slave;

    if (!master->active)
        return;

    EC_MASTER_DBG(master, 1, "Requesting OP...\n");

    // request OP for all configured slaves
    for (i = 0; i < master->slave_count; i++) {
        slave = master->slaves + i;
        if (slave->config) {
            ec_slave_request_state(slave, EC_SLAVE_STATE_OP);
        }
    }

    // always set DC reference clock to OP
    if (master->dc_ref_clock) {
        ec_slave_request_state(master->dc_ref_clock,
                EC_SLAVE_STATE_OP);
    }
}

/******************************************************************************
 *  Application interface
 *****************************************************************************/

/** Same as ecrt_master_create_domain(), but with ERR_PTR() return value.
 */
ec_domain_t *ecrt_master_create_domain_err(
        ec_master_t *master /**< master */
        )
{
    ec_domain_t *domain, *last_domain;
    unsigned int index;

    EC_MASTER_DBG(master, 1, "ecrt_master_create_domain(master = 0x%p)\n",
            master);

    if (!(domain =
                (ec_domain_t *) kmalloc(sizeof(ec_domain_t), GFP_KERNEL))) {
        EC_MASTER_ERR(master, "Error allocating domain memory!\n");
        return ERR_PTR(-ENOMEM);
    }

    ec_mutex_lock(&master->master_mutex);

    if (list_empty(&master->domains)) {
        index = 0;
    } else {
        last_domain = list_entry(master->domains.prev, ec_domain_t, list);
        index = last_domain->index + 1;
    }

    ec_domain_init(domain, master, index);
    list_add_tail(&domain->list, &master->domains);

    ec_mutex_unlock(&master->master_mutex);

    EC_MASTER_DBG(master, 1, "Created domain %u.\n", domain->index);

    return domain;
}

/*****************************************************************************/

ec_domain_t *ecrt_master_create_domain(
        ec_master_t *master /**< master */
        )
{
    ec_domain_t *d = ecrt_master_create_domain_err(master);
    return IS_ERR(d) ? NULL : d;
}

/*****************************************************************************/

int ecrt_master_activate(ec_master_t *master)
{
    uint32_t domain_offset;
    ec_domain_t *domain;
    int ret;

    EC_MASTER_DBG(master, 1, "ecrt_master_activate(master = 0x%p)\n", master);

    if (master->active) {
        EC_MASTER_WARN(master, "%s: Master already active!\n", __func__);
        return 0;
    }

    ec_mutex_lock(&master->master_mutex);

    // finish all domains
    domain_offset = 0;
    list_for_each_entry(domain, &master->domains, list) {
        ret = ec_domain_finish(domain, domain_offset);
        if (ret < 0) {
            ec_mutex_unlock(&master->master_mutex);
            EC_MASTER_ERR(master, "Failed to finish domain 0x%p!\n", domain);
            return ret;
        }
        domain_offset += domain->data_size;
    }
    
    ec_mutex_unlock(&master->master_mutex);

    // restart EoE process and master thread with new locking

    ec_master_thread_stop(master);

    EC_MASTER_DBG(master, 1, "FSM datagram is %p.\n", &master->fsm_datagram);

    master->injection_seq_fsm = 0;
    master->injection_seq_rt = 0;

    master->fsm_queue_lock_cb = master->app_fsm_queue_lock_cb;
    master->fsm_queue_unlock_cb = master->app_fsm_queue_unlock_cb;
    master->fsm_queue_locking_data = master->app_fsm_queue_locking_data;
    
    ret = ec_master_thread_start(master, ec_master_operation_thread,
                "EtherCAT-OP");
    if (ret < 0) {
        EC_MASTER_ERR(master, "Failed to start master thread!\n");
        return ret;
    }

    master->allow_config = 1; // request the current configuration
    master->allow_scan = 1; // allow re-scanning on topology change
    master->active = 1;

    // notify state machine, that the configuration shall now be applied
    master->config_changed = 1;

    return 0;
}

/*****************************************************************************/

void ecrt_master_deactivate(ec_master_t *master)
{
    ec_slave_t *slave;
#ifdef EC_EOE
    ec_eoe_t *eoe;
    int is_eoe_slave;
#endif

    EC_MASTER_DBG(master, 1, "%s(master = 0x%p)\n", __func__, master);

    if (!master->active) {
        EC_MASTER_WARN(master, "%s: Master not active.\n", __func__);
        return;
    }

    ec_master_thread_stop(master);
    
    master->fsm_queue_lock_cb = NULL;
    master->fsm_queue_unlock_cb= NULL;
    master->fsm_queue_locking_data = NULL;
    
    ec_master_clear_config(master);

    for (slave = master->slaves;
            slave < master->slaves + master->slave_count;
            slave++) {

        // set state to PREOP for all but eoe slaves
#ifdef EC_EOE
        is_eoe_slave = 0;
        // ... but leave EoE slaves in OP
        list_for_each_entry(eoe, &master->eoe_handlers, list) {
            if (slave == eoe->slave && ec_eoe_is_open(eoe))
                is_eoe_slave = 1;
       }
       if (!is_eoe_slave) {
           ec_slave_request_state(slave, EC_SLAVE_STATE_PREOP);
           // mark for reconfiguration, because the master could have no
           // possibility for a reconfiguration between two sequential
           // operation phases.
           slave->force_config = 1;
        }
#else
        ec_slave_request_state(slave, EC_SLAVE_STATE_PREOP);
        // mark for reconfiguration, because the master could have no
        // possibility for a reconfiguration between two sequential operation
        // phases.
        slave->force_config = 1;
#endif

    }

    master->app_time = 0ULL;
    master->app_start_time = 0ULL;
    master->has_app_time = 0;

    if (ec_master_thread_start(master, ec_master_idle_thread,
                "EtherCAT-IDLE"))
        EC_MASTER_WARN(master, "Failed to restart master thread!\n");

    master->allow_scan = 1;
    master->allow_config = 1;
    master->active = 0;
}

/*****************************************************************************/

void ecrt_master_send(ec_master_t *master)
{
    ec_datagram_t *datagram, *next;

    ec_master_inject_fsm_datagrams(master);

    if (unlikely(!master->main_device.link_state)) {
        // link is down, no datagram can be sent
        list_for_each_entry_safe(datagram, next, &master->datagram_queue,
                queue) {
            datagram->state = EC_DATAGRAM_ERROR;
            list_del_init(&datagram->queue);
        }

        // query link state
        ec_device_poll(&master->main_device);

        // clear frame statistics
        ec_device_clear_stats(&master->main_device);
        return;
    }

    // send frames
    ec_master_send_datagrams(master);
}

/*****************************************************************************/

void ecrt_master_receive(ec_master_t *master)
{
    ec_datagram_t *datagram, *next;

    // receive datagrams
    ec_device_poll(&master->main_device);

    // dequeue all datagrams that timed out
    list_for_each_entry_safe(datagram, next, &master->datagram_queue, queue) {
        if (datagram->state != EC_DATAGRAM_SENT) continue;

#ifdef EC_HAVE_CYCLES
        if (master->main_device.cycles_poll - datagram->cycles_sent
                > timeout_cycles) {
#else
        if (master->main_device.jiffies_poll - datagram->jiffies_sent
                > timeout_jiffies) {
#endif
            list_del_init(&datagram->queue);
            datagram->state = EC_DATAGRAM_TIMED_OUT;
            master->stats.timeouts++;
            ec_master_output_stats(master);

            if (unlikely(master->debug_level > 0)) {
                unsigned int time_us;
#ifdef EC_HAVE_CYCLES
                time_us = (unsigned int) (master->main_device.cycles_poll -
                        datagram->cycles_sent) * 1000 / cpu_khz;
#else
                time_us = (unsigned int) ((master->main_device.jiffies_poll -
                            datagram->jiffies_sent) * 1000000 / HZ);
#endif
                EC_MASTER_DBG(master, 0, "TIMED OUT datagram %p,"
                        " index %02X waited %u us.\n",
                        datagram, datagram->index, time_us);
            }
        }
    }
}


/*****************************************************************************/

/** Same as ecrt_master_slave_config(), but with ERR_PTR() return value.
 */
ec_slave_config_t *ecrt_master_slave_config_err(ec_master_t *master,
        uint16_t alias, uint16_t position, uint32_t vendor_id,
        uint32_t product_code)
{
    ec_slave_config_t *sc;
    unsigned int found = 0;


    EC_MASTER_DBG(master, 1, "ecrt_master_slave_config(master = 0x%p,"
            " alias = %u, position = %u, vendor_id = 0x%08x,"
            " product_code = 0x%08x)\n",
            master, alias, position, vendor_id, product_code);

    list_for_each_entry(sc, &master->configs, list) {
        if (sc->alias == alias && sc->position == position) {
            found = 1;
            break;
        }
    }

    if (found) { // config with same alias/position already existing
        if (sc->vendor_id != vendor_id || sc->product_code != product_code) {
            EC_MASTER_ERR(master, "Slave type mismatch. Slave was"
                    " configured as 0x%08X/0x%08X before. Now configuring"
                    " with 0x%08X/0x%08X.\n", sc->vendor_id, sc->product_code,
                    vendor_id, product_code);
            return ERR_PTR(-ENOENT);
        }
    } else {
        EC_MASTER_DBG(master, 1, "Creating slave configuration for %u:%u,"
                " 0x%08X/0x%08X.\n",
                alias, position, vendor_id, product_code);

        if (!(sc = (ec_slave_config_t *) kmalloc(sizeof(ec_slave_config_t),
                        GFP_KERNEL))) {
            EC_MASTER_ERR(master, "Failed to allocate memory"
                    " for slave configuration.\n");
            return ERR_PTR(-ENOMEM);
        }

        ec_slave_config_init(sc, master,
                alias, position, vendor_id, product_code);

        ec_mutex_lock(&master->master_mutex);

        // try to find the addressed slave
        ec_slave_config_attach(sc);
        ec_slave_config_load_default_sync_config(sc);
        list_add_tail(&sc->list, &master->configs);

        ec_mutex_unlock(&master->master_mutex);
    }

    return sc;
}

/*****************************************************************************/

ec_slave_config_t *ecrt_master_slave_config(ec_master_t *master,
        uint16_t alias, uint16_t position, uint32_t vendor_id,
        uint32_t product_code)
{
    ec_slave_config_t *sc = ecrt_master_slave_config_err(master, alias,
            position, vendor_id, product_code);
    return IS_ERR(sc) ? NULL : sc;
}

/*****************************************************************************/

int ecrt_master(ec_master_t *master, ec_master_info_t *master_info)
{
    EC_MASTER_DBG(master, 1, "ecrt_master(master = 0x%p,"
            " master_info = 0x%p)\n", master, master_info);

    master_info->slave_count = master->slave_count;
    master_info->link_up = master->main_device.link_state;
    master_info->scan_busy = master->scan_busy;
    master_info->app_time = master->app_time;
    return 0;
}

/*****************************************************************************/

int ecrt_master_get_slave(ec_master_t *master, uint16_t slave_position,
        ec_slave_info_t *slave_info)
{
    const ec_slave_t *slave;

    if (ec_mutex_lock_interruptible(&master->master_mutex)) {
        return -EINTR;
    }

    slave = ec_master_find_slave_const(master, 0, slave_position);

    slave_info->position = slave->ring_position;
    slave_info->vendor_id = slave->sii.vendor_id;
    slave_info->product_code = slave->sii.product_code;
    slave_info->revision_number = slave->sii.revision_number;
    slave_info->serial_number = slave->sii.serial_number;
    slave_info->alias = slave->effective_alias;
    slave_info->current_on_ebus = slave->sii.current_on_ebus;
    slave_info->al_state = slave->current_state;
    slave_info->error_flag = slave->error_flag;
    slave_info->sync_count = slave->sii.sync_count;
    slave_info->sdo_count = ec_slave_sdo_count(slave);
    if (slave->sii.name) {
        strncpy(slave_info->name, slave->sii.name, EC_MAX_STRING_LENGTH);
    } else {
        slave_info->name[0] = 0;
    }

    ec_mutex_unlock(&master->master_mutex);

    return 0;
}

/*****************************************************************************/

void ecrt_master_callbacks(ec_master_t *master,
                           void (*lock_cb)(void *), void (*unlock_cb)(void *),
                           void *cb_data)
{
    EC_MASTER_DBG(master, 1,"ecrt_master_callbacks(master = %p, "
                            "lock_cb = %p, unlock_cb = %p, cb_data = %p)\n",
                            master, lock_cb, unlock_cb, cb_data);

    master->app_fsm_queue_lock_cb = lock_cb;
    master->app_fsm_queue_unlock_cb = unlock_cb;
    master->app_fsm_queue_locking_data = cb_data;
}


/*****************************************************************************/

void ecrt_master_state(const ec_master_t *master, ec_master_state_t *state)
{
    state->slaves_responding = master->fsm.slaves_responding;
    state->al_states = master->fsm.slave_states;
    state->link_up = master->main_device.link_state;
}

/*****************************************************************************/

void ecrt_master_configured_slaves_state(
        const ec_master_t *master,
        ec_master_state_t *state
        )
{
    const ec_slave_config_t *sc;
    ec_slave_config_state_t sc_state;

    // collect al_states of all configured online slaves
    state->al_states = 0;
    list_for_each_entry(sc, &master->configs, list) {
        ecrt_slave_config_state(sc,&sc_state);
        if (sc_state.online)
            state->al_states |= sc_state.al_state;
    }

    state->slaves_responding = master->fsm.slaves_responding;
    state->link_up = master->main_device.link_state;
}

/*****************************************************************************/

void ecrt_master_application_time(ec_master_t *master, uint64_t app_time)
{
    master->app_time = app_time;

    if (unlikely(!master->has_app_time)) {
		EC_MASTER_DBG(master, 1, "Set application start time = %llu\n",
                app_time);
        master->app_start_time = app_time;
#ifdef EC_HAVE_CYCLES
        master->dc_cycles_app_start_time = get_cycles();
#endif
        master->dc_jiffies_app_start_time = jiffies;
        master->has_app_time = 1;
    }
}

/*****************************************************************************/

void ecrt_master_sync_reference_clock(ec_master_t *master)
{
    EC_WRITE_U32(master->ref_sync_datagram.data, master->app_time);
    ec_master_queue_datagram(master, &master->ref_sync_datagram);
}

/*****************************************************************************/

void ecrt_master_sync_slave_clocks(ec_master_t *master)
{
    ec_datagram_zero(&master->sync_datagram);
    ec_master_queue_datagram(master, &master->sync_datagram);
}

/*****************************************************************************/

void ecrt_master_sync_monitor_queue(ec_master_t *master)
{
    ec_datagram_zero(&master->sync_mon_datagram);
    ec_master_queue_datagram(master, &master->sync_mon_datagram);
}

/*****************************************************************************/

uint32_t ecrt_master_sync_monitor_process(ec_master_t *master)
{
    if (master->sync_mon_datagram.state == EC_DATAGRAM_RECEIVED) {
        return EC_READ_U32(master->sync_mon_datagram.data) & 0x7fffffff;
    } else {
        return 0xffffffff;
    }
}

/*****************************************************************************/

int ecrt_master_sdo_download(ec_master_t *master, uint16_t slave_position,
        uint16_t index, uint8_t subindex, uint8_t *data,
        size_t data_size, uint32_t *abort_code)
{
    ec_master_sdo_request_t* request;
    int retval;

    if (!data_size) {
        EC_MASTER_ERR(master, "Zero data size!\n");
        return -EINVAL;
    }

    request = kmalloc(sizeof(*request), GFP_KERNEL);
    if (!request) {
        return -ENOMEM;
    }
    kref_init(&request->refcount);

    ec_sdo_request_init(&request->req);
    ec_sdo_request_address(&request->req, index, subindex);
    if (ec_sdo_request_alloc(&request->req, data_size)) {
        kref_put(&request->refcount, ec_master_sdo_request_release);
        return -ENOMEM;
    }
    memcpy(request->req.data, data, data_size);
    request->req.data_size = data_size;
    ecrt_sdo_request_write(&request->req);

    if (ec_mutex_lock_interruptible(&master->master_mutex)) {
        kref_put(&request->refcount, ec_master_sdo_request_release);
        return -EINTR;
    }

    if (!(request->slave = ec_master_find_slave(
                    master, 0, slave_position))) {
        ec_mutex_unlock(&master->master_mutex);
        EC_MASTER_ERR(master, "Slave %u does not exist!\n", slave_position);
        kref_put(&request->refcount, ec_master_sdo_request_release);
        return -EINVAL;
    }
    
    EC_SLAVE_DBG(request->slave, 1, "Schedule SDO download request %p.\n",
            request);

    // schedule request
    kref_get(&request->refcount);
    list_add_tail(&request->list, &request->slave->slave_sdo_requests);

    ec_mutex_unlock(&master->master_mutex);

    // wait for processing through FSM
    if (wait_event_interruptible(request->slave->sdo_queue,
       ((request->req.state == EC_INT_REQUEST_SUCCESS) ||
        (request->req.state == EC_INT_REQUEST_FAILURE)))) {
        // interrupted by signal
        kref_put(&request->refcount, ec_master_sdo_request_release);
        return -EINTR;
    }

    EC_SLAVE_DBG(request->slave, 1, "Finished SDO download request %p.\n",
            request);

    *abort_code = request->req.abort_code;

    if (request->req.state == EC_INT_REQUEST_SUCCESS) {
        retval = 0;
    } else if (request->req.errno) {
        retval = -request->req.errno;
    } else {
        retval = -EIO;
    }

    kref_put(&request->refcount, ec_master_sdo_request_release);
    return retval;
}

/*****************************************************************************/

int ecrt_master_sdo_upload(ec_master_t *master, uint16_t slave_position,
        uint16_t index, uint8_t subindex, uint8_t *target,
        size_t target_size, size_t *result_size, uint32_t *abort_code)
{
    ec_master_sdo_request_t* request;
    int retval;

    request = kmalloc(sizeof(*request), GFP_KERNEL);
    if (!request)
        return -ENOMEM;
    kref_init(&request->refcount);

    ec_sdo_request_init(&request->req);
    ec_sdo_request_address(&request->req, index, subindex);
    ecrt_sdo_request_read(&request->req);

    if (ec_mutex_lock_interruptible(&master->master_mutex))  {
        kref_put(&request->refcount, ec_master_sdo_request_release);
        return -EINTR;
    }

    if (!(request->slave = ec_master_find_slave(
                    master, 0, slave_position))) {
        ec_mutex_unlock(&master->master_mutex);
        EC_MASTER_ERR(master, "Slave %u does not exist!\n", slave_position);
        kref_put(&request->refcount, ec_master_sdo_request_release);
        return -EINVAL;
    }

    EC_SLAVE_DBG(request->slave, 1, "Schedule SDO upload request %p.\n",
            request);

    // schedule request
    kref_get(&request->refcount);
    list_add_tail(&request->list, &request->slave->slave_sdo_requests);

    ec_mutex_unlock(&master->master_mutex);

    // wait for processing through FSM
    if (wait_event_interruptible(request->slave->sdo_queue,
          ((request->req.state == EC_INT_REQUEST_SUCCESS) ||
           (request->req.state == EC_INT_REQUEST_FAILURE)))) {
        // interrupted by signal
        kref_put(&request->refcount, ec_master_sdo_request_release);
        return -EINTR;
    }

    EC_SLAVE_DBG(request->slave, 1, "Finished SDO upload request %p.\n",
            request);

    *abort_code = request->req.abort_code;

    if (request->req.state != EC_INT_REQUEST_SUCCESS) {
        *result_size = 0;
        if (request->req.errno) {
            retval = -request->req.errno;
        } else {
            retval = -EIO;
        }
    } else {
        if (request->req.data_size > target_size) {
            EC_MASTER_ERR(master, "Buffer too small.\n");
            kref_put(&request->refcount, ec_master_sdo_request_release);
            return -EOVERFLOW;
        }
        memcpy(target, request->req.data, request->req.data_size);
        *result_size = request->req.data_size;
        retval = 0;
    }

    kref_put(&request->refcount, ec_master_sdo_request_release);
    return retval;
}

/*****************************************************************************/

int ecrt_master_write_idn(ec_master_t *master, uint16_t slave_position,
        uint8_t drive_no, uint16_t idn, uint8_t *data, size_t data_size,
        uint16_t *error_code)
{
    ec_master_soe_request_t* request;
    int retval;

    if (drive_no > 7) {
        EC_MASTER_ERR(master, "Invalid drive number!\n");
        return -EINVAL;
    }

    request = kmalloc(sizeof(*request), GFP_KERNEL);
    if (!request)
        return -ENOMEM;
    kref_init(&request->refcount);

    INIT_LIST_HEAD(&request->list);
    ec_soe_request_init(&request->req);
    ec_soe_request_set_drive_no(&request->req, drive_no);
    ec_soe_request_set_idn(&request->req, idn);

    if (ec_soe_request_alloc(&request->req, data_size)) {
        ec_soe_request_clear(&request->req);
        kref_put(&request->refcount,ec_master_soe_request_release);
        return -ENOMEM;
    }

    memcpy(request->req.data, data, data_size);
    request->req.data_size = data_size;
    ec_soe_request_write(&request->req);

    if (ec_mutex_lock_interruptible(&master->master_mutex)) {
        kref_put(&request->refcount,ec_master_soe_request_release);
        return -EINTR;
    }

    if (!(request->slave = ec_master_find_slave(
                    master, 0, slave_position))) {
        ec_mutex_unlock(&master->master_mutex);
        EC_MASTER_ERR(master, "Slave %u does not exist!\n",
                slave_position);
        kref_put(&request->refcount,ec_master_soe_request_release);
        return -EINVAL;
    }

    EC_SLAVE_DBG(request->slave, 1, "Scheduled SoE write request %p.\n",
            request);

    // schedule SoE write request.
    list_add_tail(&request->list, &request->slave->soe_requests);
    kref_get(&request->refcount);

    ec_mutex_unlock(&master->master_mutex);

    // wait for processing through FSM
    if (wait_event_interruptible(request->slave->soe_queue,
          ((request->req.state == EC_INT_REQUEST_SUCCESS) ||
           (request->req.state == EC_INT_REQUEST_FAILURE)))) {
           // interrupted by signal
           kref_put(&request->refcount,ec_master_soe_request_release);
           return -EINTR;
    }

    if (error_code) {
        *error_code = request->req.error_code;
    }
    retval = request->req.state == EC_INT_REQUEST_SUCCESS ? 0 : -EIO;
    kref_put(&request->refcount,ec_master_soe_request_release);

    return retval;
}

/*****************************************************************************/

int ecrt_master_read_idn(ec_master_t *master, uint16_t slave_position,
        uint8_t drive_no, uint16_t idn, uint8_t *target, size_t target_size,
        size_t *result_size, uint16_t *error_code)
{
    ec_master_soe_request_t* request;

    if (drive_no > 7) {
        EC_MASTER_ERR(master, "Invalid drive number!\n");
        return -EINVAL;
    }

    request = kmalloc(sizeof(*request), GFP_KERNEL);
    if (!request)
        return -ENOMEM;
    kref_init(&request->refcount);

    INIT_LIST_HEAD(&request->list);
    ec_soe_request_init(&request->req);
    ec_soe_request_set_drive_no(&request->req, drive_no);
    ec_soe_request_set_idn(&request->req, idn);
    ec_soe_request_read(&request->req);

    if (ec_mutex_lock_interruptible(&master->master_mutex)) {
        kref_put(&request->refcount,ec_master_soe_request_release);
        return -EINTR;
    }

    if (!(request->slave = ec_master_find_slave(master, 0, slave_position))) {
        ec_mutex_unlock(&master->master_mutex);
        kref_put(&request->refcount,ec_master_soe_request_release);
        EC_MASTER_ERR(master, "Slave %u does not exist!\n", slave_position);
        return -EINVAL;
    }

    // schedule request.
    list_add_tail(&request->list, &request->slave->soe_requests);
    kref_get(&request->refcount);

    ec_mutex_unlock(&master->master_mutex);

    EC_SLAVE_DBG(request->slave, 1, "Scheduled SoE read request %p.\n",
            request);

    // wait for processing through FSM
    if (wait_event_interruptible(request->slave->soe_queue,
          ((request->req.state == EC_INT_REQUEST_SUCCESS) ||
           (request->req.state == EC_INT_REQUEST_FAILURE)))) {
           // interrupted by signal
           kref_put(&request->refcount,ec_master_soe_request_release);
           return -EINTR;
    }

    if (error_code) {
        *error_code = request->req.error_code;
    }

    EC_SLAVE_DBG(request->slave, 1, "SoE request %p read %zd bytes"
            " via SoE.\n", request, request->req.data_size);

    if (request->req.state != EC_INT_REQUEST_SUCCESS) {
        if (result_size) {
            *result_size = 0;
        }
        kref_put(&request->refcount,ec_master_soe_request_release);
        return -EIO;
    } else {
        if (request->req.data_size > target_size) {
            EC_MASTER_ERR(master, "Buffer too small.\n");
            kref_put(&request->refcount,ec_master_soe_request_release);
            return -EOVERFLOW;
        }
        if (result_size) {
            *result_size = request->req.data_size;
        }
        memcpy(target, request->req.data, request->req.data_size);
        kref_put(&request->refcount,ec_master_soe_request_release);
        return 0;
    }
}

/*****************************************************************************/

void ecrt_master_reset(ec_master_t *master)
{
    ec_slave_config_t *sc;

    list_for_each_entry(sc, &master->configs, list) {
        if (sc->slave) {
            ec_slave_request_state(sc->slave, EC_SLAVE_STATE_OP);
        }
    }
}

/*****************************************************************************/

/** \cond */

EXPORT_SYMBOL(ecrt_master_create_domain);
EXPORT_SYMBOL(ecrt_master_activate);
EXPORT_SYMBOL(ecrt_master_deactivate);
EXPORT_SYMBOL(ecrt_master_send);
EXPORT_SYMBOL(ecrt_master_receive);
EXPORT_SYMBOL(ecrt_master_callbacks);
EXPORT_SYMBOL(ecrt_master);
EXPORT_SYMBOL(ecrt_master_get_slave);
EXPORT_SYMBOL(ecrt_master_slave_config);
EXPORT_SYMBOL(ecrt_master_state);
EXPORT_SYMBOL(ecrt_master_application_time);
EXPORT_SYMBOL(ecrt_master_sync_reference_clock);
EXPORT_SYMBOL(ecrt_master_sync_slave_clocks);
EXPORT_SYMBOL(ecrt_master_sync_monitor_queue);
EXPORT_SYMBOL(ecrt_master_sync_monitor_process);
EXPORT_SYMBOL(ecrt_master_sdo_download);
EXPORT_SYMBOL(ecrt_master_sdo_upload);
EXPORT_SYMBOL(ecrt_master_write_idn);
EXPORT_SYMBOL(ecrt_master_read_idn);
EXPORT_SYMBOL(ecrt_master_reset);
EXPORT_SYMBOL(ecrt_master_find_domain);
/** \endcond */

/*****************************************************************************/
