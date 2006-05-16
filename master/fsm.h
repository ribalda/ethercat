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
 *  as published by the Free Software Foundation; version 2 of the License.
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
 *****************************************************************************/

/**
   \file
   EtherCAT finite state machines.
*/

/*****************************************************************************/

#ifndef __EC_STATES__
#define __EC_STATES__

#include "../include/ecrt.h"
#include "command.h"
#include "slave.h"

/*****************************************************************************/

typedef struct ec_fsm ec_fsm_t;

/*****************************************************************************/

/**
   Finite state machine of an EtherCAT master.
*/

struct ec_fsm
{
    ec_master_t *master; /**< master the FSM runs on */
    ec_slave_t *slave; /**< slave the FSM runs on */
    ec_command_t command; /**< command used in the state machine */

    void (*master_state)(ec_fsm_t *); /**< master state function */
    unsigned int master_slaves_responding; /**< number of responding slaves */
    ec_slave_state_t master_slave_states; /**< states of responding slaves */

    void (*slave_state)(ec_fsm_t *); /**< slave state function */
    uint8_t slave_sii_num; /**< SII value iteration counter */
    uint8_t *slave_cat_data; /**< temporary memory for category data */
    uint16_t slave_cat_offset; /**< current category word offset in EEPROM */
    uint16_t slave_cat_data_offset; /**< current offset in category data */
    uint16_t slave_cat_type; /**< type of current category */
    uint16_t slave_cat_words; /**< number of words of current category */

    void (*sii_state)(ec_fsm_t *); /**< SII state function */
    uint16_t sii_offset; /**< input: offset in SII */
    uint32_t sii_result; /**< output: read SII value (32bit) */
};

/*****************************************************************************/

int ec_fsm_init(ec_fsm_t *, ec_master_t *);
void ec_fsm_clear(ec_fsm_t *);
void ec_fsm_reset(ec_fsm_t *);
void ec_fsm_execute(ec_fsm_t *);
int ec_fsm_idle(const ec_fsm_t *);

/*****************************************************************************/

#endif
