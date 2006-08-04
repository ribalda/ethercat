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

#include "device.h"
#include "domain.h"
#include "fsm.h"

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
    unsigned int delayed; /**< delayed datagrams */
    unsigned int corrupted; /**< corrupted frames */
    unsigned int skipped; /**< skipped datagrams (the ones that were
                             requeued when not yet received) */
    unsigned int unmatched; /**< unmatched datagrams (received, but not
                               queued any longer) */
    cycles_t t_last; /**< time of last output */
}
ec_stats_t;

/*****************************************************************************/

/**
   EtherCAT master.
   Manages slaves, domains and IO.
*/

struct ec_master
{
    struct list_head list; /**< list item for module's master list */
    unsigned int reserved; /**< non-zero, if the master is reserved for RT */
    unsigned int index; /**< master index */

    struct kobject kobj; /**< kobject */

    ec_device_t *device; /**< EtherCAT device */

    ec_fsm_t fsm; /**< master state machine */
    ec_master_mode_t mode; /**< master mode */

    struct list_head slaves; /**< list of slaves on the bus */
    unsigned int slave_count; /**< number of slaves on the bus */

    struct list_head datagram_queue; /**< datagram queue */
    uint8_t datagram_index; /**< current datagram index */

    struct list_head domains; /**< list of domains */

    int debug_level; /**< master debug level */
    ec_stats_t stats; /**< cyclic statistics */

    struct workqueue_struct *workqueue; /**< master workqueue */
    struct work_struct idle_work; /**< free run work object */
    uint32_t idle_cycle_times[HZ]; /**< Idle cycle times ring */
    unsigned int idle_cycle_time_pos; /**< time ring buffer position */

    struct timer_list eoe_timer; /**< EoE timer object */
    uint32_t eoe_cycle_times[HZ]; /**< EoE cycle times ring */
    unsigned int eoe_cycle_time_pos; /**< time ring buffer position */
    unsigned int eoe_running; /**< non-zero, if EoE processing is active. */
    unsigned int eoe_checked; /**< non-zero, if EoE processing is not
                                 necessary. */
    struct list_head eoe_handlers; /**< Ethernet-over-EtherCAT handlers */

    spinlock_t internal_lock; /**< spinlock used in idle mode */
    int (*request_cb)(void *); /**< lock request callback */
    void (*release_cb)(void *); /**< lock release callback */
    void *cb_data; /**< data parameter of locking callbacks */

    uint8_t eeprom_write_enable; /**< allow write operations to EEPROMs */
};

/*****************************************************************************/

// master creation and deletion
int ec_master_init(ec_master_t *, unsigned int, unsigned int);
void ec_master_clear(struct kobject *);
void ec_master_reset(ec_master_t *);

// free run
void ec_master_idle_start(ec_master_t *);
void ec_master_idle_stop(ec_master_t *);

// EoE
void ec_master_eoe_start(ec_master_t *);
void ec_master_eoe_stop(ec_master_t *);

// IO
void ec_master_receive_datagrams(ec_master_t *, const uint8_t *, size_t);
void ec_master_queue_datagram(ec_master_t *, ec_datagram_t *);

// slave management
int ec_master_bus_scan(ec_master_t *);

// misc.
void ec_master_output_stats(ec_master_t *);
void ec_master_clear_slaves(ec_master_t *);
void ec_master_measure_bus_time(ec_master_t *);

// other methods
void ec_sync_config(const ec_sii_sync_t *, const ec_slave_t *, uint8_t *);
void ec_fmmu_config(const ec_fmmu_t *, const ec_slave_t *, uint8_t *);
void ec_master_calc_addressing(ec_master_t *);

/*****************************************************************************/

#endif
