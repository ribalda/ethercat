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
   EtherCAT master structure.
*/

/*****************************************************************************/

#ifndef __EC_MASTER_H__
#define __EC_MASTER_H__

#include <linux/list.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <asm/semaphore.h>

#include "device.h"
#include "domain.h"
#include "fsm_master.h"
#include "cdev.h"

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
    struct class_device *class_device; /**< Master class device. */
    struct semaphore master_sem; /**< Master semaphore. */

    ec_device_t main_device; /**< EtherCAT main device. */
    const uint8_t *main_mac; /**< MAC address of main device. */
    ec_device_t backup_device; /**< EtherCAT backup device. */
    const uint8_t *backup_mac; /**< MAC address of backup device. */
    struct semaphore device_sem; /**< Device semaphore. */

    ec_fsm_master_t fsm; /**< Master state machine. */
    ec_datagram_t fsm_datagram; /**< Datagram used for state machines. */
    ec_master_phase_t phase; /**< Master phase. */
    unsigned int injection_seq_fsm; /**< Datagram injection sequence number
                                      for the FSM side. */
    unsigned int injection_seq_rt; /**< Datagram injection sequence number
                                     for the realtime side. */

    ec_slave_t *slaves; /**< Array of slaves on the bus. */
    unsigned int slave_count; /**< Number of slaves on the bus. */

    struct list_head configs; /**< List of slave configurations. */
    
    unsigned int scan_busy; /**< Current scan state. */
    unsigned int allow_scan; /**< \a True, if slave scanning is allowed. */
    struct semaphore scan_sem; /**< Semaphore protecting the \a scan_busy
                                 variable and the \a allow_scan flag. */
    wait_queue_head_t scan_queue; /**< Queue for processes that wait for
                                    slave scanning. */

    unsigned int config_busy; /**< State of slave configuration. */
    unsigned int allow_config; /**< \a True, if slave configuration is
                                 allowed. */
    struct semaphore config_sem; /**< Semaphore protecting the \a config_busy
                                   variable and the allow_config flag. */
    wait_queue_head_t config_queue; /**< Queue for processes that wait for
                                      slave configuration. */

    struct list_head datagram_queue; /**< Datagram queue. */
    uint8_t datagram_index; /**< Current datagram index. */

    struct list_head domains; /**< List of domains. */

    int debug_level; /**< Master debug level. */
    ec_stats_t stats; /**< Cyclic statistics. */
    unsigned int frames_timed_out; /**< There were frame timeouts in the last
                                     call to ecrt_master_receive(). */

    int thread_id; /**< Master thread PID. */
    struct completion thread_exit; /**< Thread completion object. */

#ifdef EC_EOE
    struct timer_list eoe_timer; /**< EoE timer object. */
    unsigned int eoe_running; /**< \a True, if EoE processing is active. */
    struct list_head eoe_handlers; /**< Ethernet-over-EtherCAT handlers. */
#endif

    spinlock_t internal_lock; /**< Spinlock used in \a IDLE phase. */
    int (*request_cb)(void *); /**< Lock request callback. */
    void (*release_cb)(void *); /**< Lock release callback. */
    void *cb_data; /**< Data parameter of locking callbacks. */
    int (*ext_request_cb)(void *); /**< External lock request callback. */
    void (*ext_release_cb)(void *); /**< External lock release callback. */
    void *ext_cb_data; /**< Data parameter of external locking callbacks. */

    struct list_head sii_requests; /**< SII write requests. */
    wait_queue_head_t sii_queue; /**< Wait queue for SII
                                      write requests from user space. */

    struct list_head slave_sdo_requests; /**< Sdo access requests. */
    wait_queue_head_t sdo_queue; /**< Wait queue for Sdo access requests
                                   from user space. */
};

/*****************************************************************************/

// master creation/deletion
int ec_master_init(ec_master_t *, unsigned int, const uint8_t *,
        const uint8_t *, dev_t, struct class *);
void ec_master_clear(ec_master_t *);

// phase transitions
int ec_master_enter_idle_phase(ec_master_t *);
void ec_master_leave_idle_phase(ec_master_t *);
int ec_master_enter_operation_phase(ec_master_t *);
void ec_master_leave_operation_phase(ec_master_t *);

#ifdef EC_EOE
// EoE
void ec_master_eoe_start(ec_master_t *);
void ec_master_eoe_stop(ec_master_t *);
#endif

// datagram IO
void ec_master_receive_datagrams(ec_master_t *, const uint8_t *, size_t);
void ec_master_queue_datagram(ec_master_t *, ec_datagram_t *);

// misc.
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
const ec_slave_config_t *ec_master_get_config_const(
        const ec_master_t *, unsigned int);
unsigned int ec_master_domain_count(const ec_master_t *);
ec_domain_t *ec_master_find_domain(ec_master_t *, unsigned int);
const ec_domain_t *ec_master_find_domain_const(const ec_master_t *,
        unsigned int);

int ec_master_debug_level(ec_master_t *, int);

/*****************************************************************************/

#endif
