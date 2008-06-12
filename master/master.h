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

/** EtherCAT master mode.
 */
typedef enum {
    EC_MASTER_MODE_ORPHANED,
    EC_MASTER_MODE_IDLE,
    EC_MASTER_MODE_OPERATION
} ec_master_mode_t;

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
    unsigned int index; /**< master index */
    unsigned int reserved; /**< non-zero, if the master is reserved for RT */

    ec_cdev_t cdev; /**< Master character device. */

    ec_device_t main_device; /**< EtherCAT device */
    const uint8_t *main_mac; /**< MAC address of main device */
    ec_device_t backup_device; /**< EtherCAT backup device */
    const uint8_t *backup_mac; /**< MAC address of backup device */
    struct semaphore device_sem; /**< device semaphore */

    ec_fsm_master_t fsm; /**< master state machine */
    ec_datagram_t fsm_datagram; /**< datagram used for state machines */
    ec_master_mode_t mode; /**< master mode */
    unsigned int injection_seq_fsm; /**< datagram injection sequence number
                                      for the FSM side */
    unsigned int injection_seq_rt; /**< datagram injection sequence number
                                     for the realtime side */

    ec_slave_t *slaves; /**< Array of slaves on the bus. */
    unsigned int slave_count; /**< Number of slaves on the bus. */

    struct list_head configs; /**< Bus configuration list. */
    unsigned int configs_attached; /**< Slave configurations were attached. */
    
    unsigned int scan_busy; /**< Current scan state. */
    unsigned int allow_scan; /**< non-zero, if slave scanning is allowed */
    struct semaphore scan_sem; /**< semaphore protecting the scan_state
                                 variable and the allow_scan flag */
    wait_queue_head_t scan_queue; /**< queue for processes that wait for
                                    slave scanning */

    unsigned int config_busy; /**< State of slave configuration. */
    unsigned int allow_config; /**< non-zero, if slave scanning is allowed */
    struct semaphore config_sem; /**< semaphore protecting the config_state
                                   variable and the allow_config flag */
    wait_queue_head_t config_queue; /**< queue for processes that wait for
                                      slave configuration */

    struct list_head datagram_queue; /**< datagram queue */
    uint8_t datagram_index; /**< current datagram index */

    struct list_head domains; /**< list of domains */

    int debug_level; /**< Master debug level. */
    ec_stats_t stats; /**< cyclic statistics */
    unsigned int frames_timed_out; /**< there were frame timeouts in the last
                                     call to ecrt_master_receive() */

    int thread_id; /**< master thread PID */
    struct completion thread_exit; /**< thread completion object */
    uint32_t idle_cycle_times[HZ]; /**< Idle cycle times ring */
    unsigned int idle_cycle_time_pos; /**< time ring buffer position */

#ifdef EC_EOE
    struct timer_list eoe_timer; /**< EoE timer object */
    unsigned int eoe_running; /**< non-zero, if EoE processing is active. */
    struct list_head eoe_handlers; /**< Ethernet-over-EtherCAT handlers */
    uint32_t eoe_cycle_times[HZ]; /**< EoE cycle times ring */
    unsigned int eoe_cycle_time_pos; /**< time ring buffer position */
#endif

    spinlock_t internal_lock; /**< spinlock used in idle mode */
    int (*request_cb)(void *); /**< lock request callback */
    void (*release_cb)(void *); /**< lock release callback */
    void *cb_data; /**< data parameter of locking callbacks */
    int (*ext_request_cb)(void *); /**< external lock request callback */
    void (*ext_release_cb)(void *); /**< externam lock release callback */
    void *ext_cb_data; /**< data parameter of external locking callbacks */

    struct list_head sii_requests; /**< SII write requests */
    struct semaphore sii_sem; /**< semaphore protecting the list of
                                   SII write requests */
    wait_queue_head_t sii_queue; /**< wait queue for SII
                                      write requests from user space */

    struct list_head slave_sdo_requests; /**< Sdo access requests. */
    struct semaphore sdo_sem; /**< semaphore protecting the list of
                                   Sdo access requests */
    wait_queue_head_t sdo_queue; /**< wait queue for Sdo access requests
                                   from user space */
};

/*****************************************************************************/

// master creation/deletion
int ec_master_init(ec_master_t *, unsigned int, const uint8_t *,
        const uint8_t *, dev_t);
void ec_master_clear(ec_master_t *);

// mode transitions
int ec_master_enter_idle_mode(ec_master_t *);
void ec_master_leave_idle_mode(ec_master_t *);
int ec_master_enter_operation_mode(ec_master_t *);
void ec_master_leave_operation_mode(ec_master_t *);

#ifdef EC_EOE
// EoE
void ec_master_eoe_start(ec_master_t *);
void ec_master_eoe_stop(ec_master_t *);
#endif

// datagram IO
void ec_master_receive_datagrams(ec_master_t *, const uint8_t *, size_t);
void ec_master_queue_datagram(ec_master_t *, ec_datagram_t *);

// misc.
int ec_master_attach_slave_configs(ec_master_t *);
ec_slave_t *ec_master_find_slave(ec_master_t *, uint16_t, uint16_t);
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

int ec_master_debug_level(ec_master_t *, int);

/*****************************************************************************/

#endif
