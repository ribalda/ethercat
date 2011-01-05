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
   EtherCAT master state machine.
*/

/*****************************************************************************/

#ifndef __EC_FSM_MASTER_H__
#define __EC_FSM_MASTER_H__

#include "globals.h"
#include "datagram.h"
#include "foe_request.h"
#include "sdo_request.h"
#include "soe_request.h"
#include "fsm_slave_config.h"
#include "fsm_slave_scan.h"
#include "fsm_pdo.h"

/*****************************************************************************/

/** SII write request.
 */
typedef struct {
    struct list_head list; /**< List head. */
    ec_slave_t *slave; /**< EtherCAT slave. */
    uint16_t offset; /**< SII word offset. */
    size_t nwords; /**< Number of words. */
    const uint16_t *words; /**< Pointer to the data words. */
    ec_internal_request_state_t state; /**< State of the request. */
} ec_sii_write_request_t;

/*****************************************************************************/

/** Register request.
 */
typedef struct {
    struct list_head list; /**< List head. */
    ec_slave_t *slave; /**< EtherCAT slave. */
    ec_direction_t dir; /**< Direction. */
    uint16_t offset; /**< Register address. */
    size_t length; /**< Number of bytes. */
    uint8_t *data; /**< Data to write / memory for read data. */
    ec_internal_request_state_t state; /**< State of the request. */
} ec_reg_request_t;


/*****************************************************************************/

/** Slave/SDO request record for master's SDO request list.
 */
typedef struct {
    struct list_head list; /**< List element. */
    ec_slave_t *slave; /**< Slave. */
    ec_sdo_request_t req; /**< SDO request. */
    struct kref refcount;
} ec_master_sdo_request_t;

void ec_master_sdo_request_release(struct kref *);

/*****************************************************************************/

/** FoE request.
 */
typedef struct {
    struct list_head list; /**< List head. */
    ec_slave_t *slave; /**< EtherCAT slave. */
    ec_foe_request_t req; /**< FoE request. */
    struct kref refcount;
} ec_master_foe_request_t;

void ec_master_foe_request_release(struct kref *);

/*****************************************************************************/

/** SoE request.
 */
typedef struct {
    struct list_head list; /**< List head. */
    ec_slave_t *slave; /**< EtherCAT slave. */
    ec_soe_request_t req; /**< SoE request. */
} ec_master_soe_request_t;

/*****************************************************************************/

typedef struct ec_fsm_master ec_fsm_master_t; /**< \see ec_fsm_master */

/** Finite state machine of an EtherCAT master.
 */
struct ec_fsm_master {
    ec_master_t *master; /**< master the FSM runs on */
    ec_datagram_t *datagram; /**< datagram used in the state machine */
    unsigned int retries; /**< retries on datagram timeout. */

    void (*state)(ec_fsm_master_t *); /**< master state function */
    int idle; /**< state machine is in idle phase */
    unsigned long scan_jiffies; /**< beginning of slave scanning */
    uint8_t link_state; /**< Last main device link state. */
    unsigned int slaves_responding; /**< number of responding slaves */
    unsigned int rescan_required; /**< A bus rescan is required. */
    ec_slave_state_t slave_states; /**< states of responding slaves */
    ec_slave_t *slave; /**< current slave */
    ec_sii_write_request_t *sii_request; /**< SII write request */
    off_t sii_index; /**< index to SII write request data */
    ec_sdo_request_t *sdo_request; /**< SDO request to process. */
    ec_reg_request_t *reg_request; /**< Register request to process. */

    ec_fsm_coe_t fsm_coe; /**< CoE state machine */
    ec_fsm_pdo_t fsm_pdo; /**< PDO configuration state machine. */
    ec_fsm_change_t fsm_change; /**< State change state machine */
    ec_fsm_slave_config_t fsm_slave_config; /**< slave state machine */
    ec_fsm_slave_scan_t fsm_slave_scan; /**< slave state machine */
    ec_fsm_sii_t fsm_sii; /**< SII state machine */
};

/*****************************************************************************/

void ec_fsm_master_init(ec_fsm_master_t *, ec_master_t *, ec_datagram_t *);
void ec_fsm_master_clear(ec_fsm_master_t *);

int ec_fsm_master_exec(ec_fsm_master_t *);
int ec_fsm_master_idle(const ec_fsm_master_t *);

/*****************************************************************************/

#endif
