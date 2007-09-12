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

#ifndef _EC_MASTER_H_
#define _EC_MASTER_H_

#include <linux/list.h>
#include <linux/sysfs.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <asm/semaphore.h>

#include "device.h"
#include "domain.h"
#include "fsm_master.h"

/*****************************************************************************/

/**
   EtherCAT master mode.
*/

typedef enum
{
    EC_MASTER_MODE_ORPHANED,
    EC_MASTER_MODE_IDLE,
    EC_MASTER_MODE_OPERATION
}
ec_master_mode_t;

/*****************************************************************************/

/**
   Cyclic statistics.
*/

typedef struct
{
    unsigned int timeouts; /**< datagram timeouts */
    unsigned int corrupted; /**< corrupted frames */
    unsigned int skipped; /**< skipped datagrams (the ones that were
                             requeued when not yet received) */
    unsigned int unmatched; /**< unmatched datagrams (received, but not
                               queued any longer) */
    unsigned long output_jiffies; /**< time of last output */
}
ec_stats_t;

/*****************************************************************************/

/**
   EtherCAT master.
   Manages slaves, domains and IO.
*/

struct ec_master
{
    struct kobject kobj; /**< kobject */
    unsigned int index; /**< master index */
    unsigned int reserved; /**< non-zero, if the master is reserved for RT */

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

    struct list_head slaves; /**< list of slaves on the bus */
    unsigned int slave_count; /**< number of slaves on the bus */
    
    ec_request_state_t scan_state; /**< current scanning state */
    unsigned int allow_scan; /**< non-zero, if slave scanning is allowed */
    struct semaphore scan_sem; /**< semaphore protecting the scan_state
                                 variable and the allow_scan flag */
    wait_queue_head_t scan_queue; /**< queue for processes that wait for
                                    slave scanning */

    ec_request_state_t config_state; /**< state of slave configuration */
    unsigned int allow_config; /**< non-zero, if slave scanning is allowed */
    struct semaphore config_sem; /**< semaphore protecting the config_state
                                   variable and the allow_config flag */
    wait_queue_head_t config_queue; /**< queue for processes that wait for
                                      slave configuration */

    struct list_head datagram_queue; /**< datagram queue */
    uint8_t datagram_index; /**< current datagram index */

    struct list_head domains; /**< list of domains */

    int debug_level; /**< master debug level */
    ec_stats_t stats; /**< cyclic statistics */
    unsigned int pdo_slaves_offline; /** number of slaves, for which PDOs
                                       were registered and that are offline
                                       (used for bus status) */
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

    struct list_head eeprom_requests; /**< EEPROM write requests */
    struct semaphore eeprom_sem; /**< semaphore protecting the list of
                                   EEPROM write requests */
    wait_queue_head_t eeprom_queue; /**< wait queue for EEPROM
                                      write requests from user space */

    struct list_head sdo_requests; /**< SDO access requests */
    struct semaphore sdo_sem; /**< semaphore protecting the list of
                                   SDO access requests */
    wait_queue_head_t sdo_queue; /**< wait queue for SDO access requests
                                   from user space */
};

/*****************************************************************************/

// master creation/deletion
int ec_master_init(ec_master_t *, struct kobject *, unsigned int,
        const uint8_t *, const uint8_t *);
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
void ec_master_output_stats(ec_master_t *);
#ifdef EC_EOE
void ec_master_clear_eoe_handlers(ec_master_t *);
#endif
void ec_master_destroy_slaves(ec_master_t *);

/*****************************************************************************/

#endif
