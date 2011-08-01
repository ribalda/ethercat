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
 *****************************************************************************/

/**
   \file
   EtherCAT master structure.
*/

/*****************************************************************************/

#ifndef __EC_MASTER_H__
#define __EC_MASTER_H__

#include <linux/version.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/kthread.h>

#include "device.h"
#include "domain.h"
#include "ethernet.h"
#include "fsm_master.h"
#include "cdev.h"


/*****************************************************************************/

/** Convenience macro for printing master-specific information to syslog.
 *
 * This will print the message in \a fmt with a prefixed "EtherCAT <INDEX>: ",
 * where INDEX is the master index.
 *
 * \param master EtherCAT master
 * \param fmt format string (like in printf())
 * \param args arguments (optional)
 */
#ifdef USE_TRACE_PRINTK
#define EC_MASTER_INFO(master, fmt, args...) \
    do { \
        __trace_printk(_THIS_IP_,"EtherCAT %u: " fmt, master->index, ##args); \
        printk(KERN_INFO "EtherCAT %u: " fmt, master->index, ##args);   \
    } while (0)
#else
#define EC_MASTER_INFO(master, fmt, args...) \
    printk(KERN_INFO "EtherCAT %u: " fmt, master->index, ##args)
#endif

/** Convenience macro for printing master-specific errors to syslog.
 *
 * This will print the message in \a fmt with a prefixed "EtherCAT <INDEX>: ",
 * where INDEX is the master index.
 *
 * \param master EtherCAT master
 * \param fmt format string (like in printf())
 * \param args arguments (optional)
 */
#ifdef USE_TRACE_PRINTK
#define EC_MASTER_ERR(master, fmt, args...) \
    do { \
        __trace_printk(_THIS_IP_,"EtherCAT ERROR %u: " fmt, master->index, ##args); \
        printk(KERN_ERR "EtherCAT ERROR %u: " fmt, master->index, ##args); \
    } while (0)
#else
#define EC_MASTER_ERR(master, fmt, args...) \
    printk(KERN_ERR "EtherCAT ERROR %u: " fmt, master->index, ##args)
#endif

/** Convenience macro for printing master-specific warnings to syslog.
 *
 * This will print the message in \a fmt with a prefixed "EtherCAT <INDEX>: ",
 * where INDEX is the master index.
 *
 * \param master EtherCAT master
 * \param fmt format string (like in printf())
 * \param args arguments (optional)
 */
#ifdef USE_TRACE_PRINTK
#define EC_MASTER_WARN(master, fmt, args...) \
    do { \
        __trace_printk(_THIS_IP_,"EtherCAT WARNING %u: " fmt, master->index, ##args); \
        printk(KERN_WARNING "EtherCAT WARNING %u: " fmt, master->index, ##args);    \
    } while (0)
#else
#define EC_MASTER_WARN(master, fmt, args...) \
    printk(KERN_WARNING "EtherCAT WARNING %u: " fmt, master->index, ##args)
#endif

/** Convenience macro for printing master-specific debug messages to syslog.
 *
 * This will print the message in \a fmt with a prefixed "EtherCAT <INDEX>: ",
 * where INDEX is the master index.
 *
 * \param master EtherCAT master
 * \param fmt format string (like in printf())
 * \param args arguments (optional)
 */
#ifdef USE_TRACE_PRINTK
#define EC_MASTER_DBG(master, level, fmt, args...) \
    do { \
        __trace_printk(_THIS_IP_,"EtherCAT DEBUG%u %u: " fmt, \
            level,master->index, ##args); \
        if (master->debug_level >= level) { \
            printk(KERN_DEBUG "EtherCAT DEBUG %u: " fmt, \
                    master->index, ##args); \
        } \
    } while (0)
#else
#define EC_MASTER_DBG(master, level, fmt, args...) \
    do { \
        if (master->debug_level >= level) { \
            printk(KERN_DEBUG "EtherCAT DEBUG %u: " fmt, \
                    master->index, ##args); \
        } \
    } while (0)
#endif

/*****************************************************************************/

/** EtherCAT master phase.
 */
typedef enum {
    EC_ORPHANED, /**< Orphaned phase. The master has no Ethernet device
                   attached. */
    EC_IDLE, /**< Idle phase. An Ethernet device is attached, but the master
               is not in use, yet. */
    EC_OPERATION /**< Operation phase. The master was requested by a realtime
                   application. */
} ec_master_phase_t;

/*****************************************************************************/

/** Cyclic statistics.
 */
typedef struct {
    unsigned int timeouts; /**< datagram timeouts */
    unsigned int corrupted; /**< corrupted frames */
    unsigned int unmatched; /**< unmatched datagrams (received, but not
                               queued any longer) */
    unsigned long output_jiffies; /**< time of last output */
} ec_stats_t;

/*****************************************************************************/

/** EtherCAT master.
 *
 * Manages slaves, domains and IO.
 */
struct ec_master {
    unsigned int index; /**< Index. */
    unsigned int reserved; /**< \a True, if the master is in use. */

    ec_cdev_t cdev; /**< Master character device. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
    struct device *class_device; /**< Master class device. */
#else
    struct class_device *class_device; /**< Master class device. */
#endif

    struct ec_mutex_t master_mutex; /**< Master mutex. */

    ec_device_t main_device; /**< EtherCAT main device. */
    const uint8_t *main_mac; /**< MAC address of main device. */
    ec_device_t backup_device; /**< EtherCAT backup device. */
    const uint8_t *backup_mac; /**< MAC address of backup device. */
    struct ec_mutex_t device_mutex; /**< Device mutex. */

    ec_fsm_master_t fsm; /**< Master state machine. */
    ec_datagram_t fsm_datagram; /**< Datagram used for state machines. */
    ec_mailbox_t fsm_mbox; /**< Mailbox used for state machines. */
    ec_master_phase_t phase; /**< Master phase. */
    unsigned int active; /**< Master has been activated. */
    unsigned int config_changed; /**< The configuration changed. */
    unsigned int injection_seq_fsm; /**< Datagram injection sequence number
                                      for the FSM side. */
    unsigned int injection_seq_rt; /**< Datagram injection sequence number
                                     for the realtime side. */

    ec_slave_t *slaves; /**< Array of slaves on the bus. */
    unsigned int slave_count; /**< Number of slaves on the bus. */

    /* Configuration applied by the application. */
    struct list_head configs; /**< List of slave configurations. */
    struct list_head domains; /**< List of domains. */
    
    u64 app_time; /**< Time of the last ecrt_master_sync() call. */
    u64 app_start_time; /**< Application start time. */
    u8 has_app_time; /**< Application time is valid. */
    ec_datagram_t ref_sync_datagram; /**< Datagram used for synchronizing the
                                       reference clock to the master clock. */
    ec_datagram_t sync_datagram; /**< Datagram used for DC drift
                                   compensation. */
    ec_datagram_t sync_mon_datagram; /**< Datagram used for DC synchronisation
                                       monitoring. */
    ec_slave_t *dc_ref_clock; /**< DC reference clock slave. */
#ifdef EC_HAVE_CYCLES
    cycles_t dc_cycles_app_start_time; /** cycles at last ecrt_master_sync() call.*/
#endif
    unsigned long dc_jiffies_app_start_time;/** jiffies at last
                                            ecrt_master_sync() call.*/
    unsigned int scan_busy; /**< Current scan state. */
    unsigned int allow_scan; /**< \a True, if slave scanning is allowed. */
    struct ec_mutex_t scan_mutex; /**< Mutex protecting the \a scan_busy
                                 variable and the \a allow_scan flag. */
    wait_queue_head_t scan_queue; /**< Queue for processes that wait for
                                    slave scanning. */

    unsigned int config_busy; /**< State of slave configuration. */
    unsigned int allow_config; /**< \a True, if slave configuration is
                                 allowed. */
    struct ec_mutex_t config_mutex; /**< Mutex protecting the \a config_busy
                                   variable and the allow_config flag. */
    wait_queue_head_t config_queue; /**< Queue for processes that wait for
                                      slave configuration. */

    struct list_head datagram_queue; /**< Datagram queue. */
    uint8_t datagram_index; /**< Current datagram index. */

    struct ec_mutex_t fsm_queue_mutex; /**< Mutex protecting the \a
                                      fsm_datagram_queue. */
    struct list_head fsm_datagram_queue; /**< External Datagram queue. */

    unsigned int send_interval; /**< Interval between calls to
                                  ecrt_master_send */
    size_t max_queue_size; /**< Maximum size of datagram queue */

    unsigned int debug_level; /**< Master debug level. */
    ec_stats_t stats; /**< Cyclic statistics. */

    struct task_struct *thread; /**< Master thread. */

#ifdef EC_EOE
    struct task_struct *eoe_thread; /**< EoE thread. */
    struct list_head eoe_handlers; /**< Ethernet over EtherCAT handlers. */
#endif

    struct ec_mutex_t io_mutex; /**< Mutex used in \a IDLE phase. */

    void (*fsm_queue_lock_cb)(void *); /**< FSM queue lock callback. */
    void (*fsm_queue_unlock_cb)(void *); /**< FSM queue unlock callback. */
    void *fsm_queue_locking_data; /**< Data parameter of fsm queue locking
                                    callbacks. */
    void (*app_fsm_queue_lock_cb)(void *); /**< App's FSM queue lock callback.
                                            */
    void (*app_fsm_queue_unlock_cb)(void *); /**< App's FSM queue unlock
                                               callback. */
    void *app_fsm_queue_locking_data; /**< App's data parameter of fsm queue
                                        locking callbacks. */

    struct list_head sii_requests; /**< SII write requests. */
    wait_queue_head_t sii_queue; /**< Wait queue for SII
                                      write requests from user space. */

    struct list_head reg_requests; /**< Register requests. */
    wait_queue_head_t reg_queue; /**< Wait queue for register requests. */
};

/*****************************************************************************/

// static funtions
void ec_master_init_static(void);

// master creation/deletion
int ec_master_init(ec_master_t *, unsigned int, const uint8_t *,
        const uint8_t *, dev_t, struct class *, unsigned int);
void ec_master_clear(ec_master_t *);

// phase transitions
int ec_master_enter_idle_phase(ec_master_t *);
void ec_master_leave_idle_phase(ec_master_t *);
int ec_master_enter_operation_phase(ec_master_t *);
void ec_master_leave_operation_phase(ec_master_t *);

// datagram IO
void ec_master_receive_datagrams(ec_master_t *, const uint8_t *, size_t);
void ec_master_queue_datagram(ec_master_t *, ec_datagram_t *);
void ec_master_queue_request_fsm_datagram(ec_master_t *, ec_datagram_t *);
void ec_master_queue_fsm_datagram(ec_master_t *, ec_datagram_t *);
void ec_master_inject_fsm_datagrams(ec_master_t *);

// misc.
void ec_master_set_send_interval(ec_master_t *, unsigned int);
void ec_master_attach_slave_configs(ec_master_t *);
ec_slave_t *ec_master_find_slave(ec_master_t *, uint16_t, uint16_t);
const ec_slave_t *ec_master_find_slave_const(const ec_master_t *, uint16_t,
        uint16_t);
void ec_master_output_stats(ec_master_t *);
#ifdef EC_EOE
void ec_master_clear_eoe_handlers(ec_master_t *);
#endif
void ec_master_clear_slaves(ec_master_t *);

unsigned int ec_master_config_count(const ec_master_t *);
ec_slave_config_t *ec_master_get_config(
        const ec_master_t *, unsigned int);
const ec_slave_config_t *ec_master_get_config_const(
        const ec_master_t *, unsigned int);
unsigned int ec_master_domain_count(const ec_master_t *);
ec_domain_t *ec_master_find_domain(ec_master_t *, unsigned int);
const ec_domain_t *ec_master_find_domain_const(const ec_master_t *,
        unsigned int);
#ifdef EC_EOE
uint16_t ec_master_eoe_handler_count(const ec_master_t *);
const ec_eoe_t *ec_master_get_eoe_handler_const(const ec_master_t *, uint16_t);
#endif

int ec_master_debug_level(ec_master_t *, unsigned int);

ec_domain_t *ecrt_master_create_domain_err(ec_master_t *);
ec_slave_config_t *ecrt_master_slave_config_err(ec_master_t *, uint16_t,
        uint16_t, uint32_t, uint32_t);

void ec_master_calc_dc(ec_master_t *);
void ec_master_request_op(ec_master_t *);

/*****************************************************************************/

#endif
