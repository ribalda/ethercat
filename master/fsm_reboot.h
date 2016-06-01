/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2014  Gavin Lambert
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
   EtherCAT slave reboot FSM.
*/

/*****************************************************************************/

#ifndef __EC_FSM_REBOOT_H__
#define __EC_FSM_REBOOT_H__

#include "globals.h"
#include "datagram.h"
#include "master.h"
#include "slave.h"

/*****************************************************************************/

typedef struct ec_fsm_reboot ec_fsm_reboot_t; /**< \see ec_fsm_reboot */

/**
   EtherCAT slave reboot FSM.
*/

struct ec_fsm_reboot
{
    ec_master_t *master; /**< master the FSM runs on, if "all" */
    ec_slave_t *slave; /**< slave the FSM runs on, if "single" */
    ec_datagram_t *datagram; /**< datagram used in the state machine */
    unsigned int retries; /**< retries upon datagram timeout */
    unsigned long jiffies_timeout; /**< pause timer */

    void (*state)(ec_fsm_reboot_t *); /**< slave reboot state function */
};

/*****************************************************************************/

void ec_fsm_reboot_init(ec_fsm_reboot_t *, ec_datagram_t *);
void ec_fsm_reboot_clear(ec_fsm_reboot_t *);

void ec_fsm_reboot_single(ec_fsm_reboot_t *, ec_slave_t *);
void ec_fsm_reboot_all(ec_fsm_reboot_t *, ec_master_t *);

int ec_fsm_reboot_exec(ec_fsm_reboot_t *);
int ec_fsm_reboot_success(ec_fsm_reboot_t *);

/*****************************************************************************/

#endif
