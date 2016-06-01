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

#include "globals.h"
#include "master.h"
#include "fsm_reboot.h"

/*****************************************************************************/

#define EC_FSM_ERR(fsm, fmt, args...) \
    do { \
        if (fsm->slave) { \
            EC_SLAVE_ERR(fsm->slave, fmt, ##args); \
        } else { \
            EC_MASTER_ERR(fsm->master, fmt, ##args); \
        } \
    } while (0)

/*****************************************************************************/

void ec_fsm_reboot_state_start(ec_fsm_reboot_t *);
void ec_fsm_reboot_state_one(ec_fsm_reboot_t *);
void ec_fsm_reboot_state_two(ec_fsm_reboot_t *);
void ec_fsm_reboot_state_three(ec_fsm_reboot_t *);
void ec_fsm_reboot_state_wait(ec_fsm_reboot_t *);
void ec_fsm_reboot_state_end(ec_fsm_reboot_t *);
void ec_fsm_reboot_state_error(ec_fsm_reboot_t *);

/*****************************************************************************/

/**
   Constructor.
*/

void ec_fsm_reboot_init(ec_fsm_reboot_t *fsm, /**< finite state machine */
                        ec_datagram_t *datagram /**< datagram */
                        )
{
    fsm->state = NULL;
    fsm->datagram = datagram;
}

/*****************************************************************************/

/**
   Destructor.
*/

void ec_fsm_reboot_clear(ec_fsm_reboot_t *fsm /**< finite state machine */)
{
}

/*****************************************************************************/

/**
   Starts the reboot state machine for a single slave.
*/

void ec_fsm_reboot_single(ec_fsm_reboot_t *fsm, /**< finite state machine */
                         ec_slave_t *slave /**< EtherCAT slave */
                         )
{
    fsm->master = slave->master;
    fsm->slave = slave;
    fsm->state = ec_fsm_reboot_state_start;
}

/*****************************************************************************/

/**
   Starts the reboot state machine for all slaves on a master.
*/

void ec_fsm_reboot_all(ec_fsm_reboot_t *fsm, /**< finite state machine */
                         ec_master_t *master /**< EtherCAT master */
                         )
{
    fsm->master = master;
    fsm->slave = NULL;
    fsm->state = ec_fsm_reboot_state_start;
}

/*****************************************************************************/

/**
   Executes the current state of the state machine.
   \return false, if the state machine has terminated
*/

int ec_fsm_reboot_exec(ec_fsm_reboot_t *fsm /**< finite state machine */)
{
    fsm->state(fsm);

    return fsm->state != ec_fsm_reboot_state_end
        && fsm->state != ec_fsm_reboot_state_error;
}

/*****************************************************************************/

/**
   Returns, if the state machine terminated with success.
   \return non-zero if successful.
*/

int ec_fsm_reboot_success(ec_fsm_reboot_t *fsm /**< Finite state machine */)
{
    return fsm->state == ec_fsm_reboot_state_end;
}

/******************************************************************************
 *  slave reboot state machine
 *****************************************************************************/

/**
   Reboot state: START.
*/

void ec_fsm_reboot_state_start(ec_fsm_reboot_t *fsm
                               /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (slave) {
        EC_SLAVE_INFO(slave, "Requesting slave reboot\n");
        ec_datagram_fpwr(datagram, slave->station_address, 0x0040, 1);
    } else {
        EC_MASTER_INFO(fsm->master, "Requesting global reboot\n");
        ec_datagram_bwr(datagram, 0x0040, 1);
    }
    EC_WRITE_U8(datagram->data, 'R');
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_reboot_state_one;
}

/*****************************************************************************/

/**
   Reboot state: ONE.
*/

void ec_fsm_reboot_state_one(ec_fsm_reboot_t *fsm
                               /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_reboot_state_error;
        EC_FSM_ERR(fsm, "Failed to receive reboot 1 datagram: ");
        ec_datagram_print_state(datagram);
        return;
    }

    if (datagram->working_counter == 0) {
        if (slave && fsm->retries--) {
            ec_datagram_fpwr(datagram, slave->station_address, 0x0040, 1);
            EC_WRITE_U8(datagram->data, 'R');
            return;
        }

        fsm->state = ec_fsm_reboot_state_error;
        EC_FSM_ERR(fsm, "Failed to reboot 1\n");
        ec_datagram_print_wc_error(datagram);
        return;
    }

    if (slave) {
        ec_datagram_fpwr(datagram, slave->station_address, 0x0040, 1);
    } else {
        ec_datagram_bwr(datagram, 0x0040, 1);
    }
    EC_WRITE_U8(datagram->data, 'E');
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_reboot_state_two;
}

/*****************************************************************************/

/**
   Reboot state: TWO.
*/

void ec_fsm_reboot_state_two(ec_fsm_reboot_t *fsm
                               /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_reboot_state_error;
        EC_FSM_ERR(fsm, "Failed to receive reboot 2 datagram: ");
        ec_datagram_print_state(datagram);
        return;
    }

    if (datagram->working_counter == 0) {
        fsm->state = ec_fsm_reboot_state_error;
        EC_FSM_ERR(fsm, "Failed to reboot 2\n");
        ec_datagram_print_wc_error(datagram);
        return;
    }

    if (slave) {
        ec_datagram_fpwr(datagram, slave->station_address, 0x0040, 1);
    } else {
        ec_datagram_bwr(datagram, 0x0040, 1);
    }
    EC_WRITE_U8(datagram->data, 'S');
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_reboot_state_three;
}

/*****************************************************************************/

/**
   Reboot state: THREE.
*/

void ec_fsm_reboot_state_three(ec_fsm_reboot_t *fsm
                               /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;

    if (datagram->state == EC_DATAGRAM_TIMED_OUT && fsm->retries--)
        return;

    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        fsm->state = ec_fsm_reboot_state_error;
        EC_FSM_ERR(fsm, "Failed to receive reboot 3 datagram: ");
        ec_datagram_print_state(datagram);
        return;
    }

    if (datagram->working_counter == 0) {
        fsm->state = ec_fsm_reboot_state_error;
        EC_FSM_ERR(fsm, "Failed to reboot 3\n");
        ec_datagram_print_wc_error(datagram);
        return;
    }

    // we must delay for a minimum of 1ms before allowing *any* datagram to be
    // sent on the network, or the slaves may not actually reboot (due to a
    // hardware bug).  we must wait at least 2 cycles to guarantee no undershoot.
    fsm->jiffies_timeout = datagram->jiffies_received + max(2, HZ/1000);
    datagram->state = EC_DATAGRAM_INVALID; // do not send a new datagram
    fsm->state = ec_fsm_reboot_state_wait;
}

/*****************************************************************************/

/**
   Reboot state: WAIT.
*/

void ec_fsm_reboot_state_wait(ec_fsm_reboot_t *fsm
                               /**< finite state machine */)
{
    ec_datagram_t *datagram = fsm->datagram;

    if (time_after(jiffies, fsm->jiffies_timeout)) {
        // slave should have rebooted by now, if it supports this.  if it
        // does, the master FSM will detect a topology change (unless it
        // finished reset already).
        fsm->state = ec_fsm_reboot_state_end;
        return;
    }

    // we cannot allow any datagrams to be sent while we're waiting, or the
    // slaves might fail to reboot.  this will not absolutely block datagrams
    // without a bit wider cooperation but it should work in most cases.
    datagram->state = EC_DATAGRAM_INVALID;
}

/*****************************************************************************/

/**
   State: ERROR.
*/

void ec_fsm_reboot_state_error(ec_fsm_reboot_t *fsm
                               /**< finite state machine */)
{
}

/*****************************************************************************/

/**
   State: END.
*/

void ec_fsm_reboot_state_end(ec_fsm_reboot_t *fsm
                             /**< finite state machine */)
{
}

/*****************************************************************************/
