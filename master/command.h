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
   EtherCAT command structure.
*/

/*****************************************************************************/

#ifndef _EC_COMMAND_H_
#define _EC_COMMAND_H_

#include <linux/list.h>
#include <linux/timex.h>

#include "globals.h"

/*****************************************************************************/

/**
   EtherCAT command type.
*/

typedef enum
{
    EC_CMD_NONE = 0x00, /**< Dummy */
    EC_CMD_APRD = 0x01, /**< Auto-increment physical read */
    EC_CMD_APWR = 0x02, /**< Auto-increment physical write */
    EC_CMD_NPRD = 0x04, /**< Node-addressed physical read */
    EC_CMD_NPWR = 0x05, /**< Node-addressed physical write */
    EC_CMD_BRD  = 0x07, /**< Broadcast read */
    EC_CMD_BWR  = 0x08, /**< Broadcast write */
    EC_CMD_LRW  = 0x0C  /**< Logical read/write */
}
ec_command_type_t;

/**
   EtherCAT command state.
*/

typedef enum
{
    EC_CMD_INIT, /**< new command */
    EC_CMD_QUEUED, /**< command queued by master */
    EC_CMD_SENT, /**< command has been sent */
    EC_CMD_RECEIVED, /**< command has been received */
    EC_CMD_TIMEOUT, /**< command timed out */
    EC_CMD_ERROR /**< error while sending/receiving */
}
ec_command_state_t;

/*****************************************************************************/

/**
   EtherCAT address.
*/

typedef union
{
    struct
    {
        uint16_t slave; /**< configured or autoincrement address */
        uint16_t mem; /**< physical memory address */
    }
    physical; /**< physical address */

    uint32_t logical; /**< logical address */
}
ec_address_t;

/*****************************************************************************/

/**
   EtherCAT command.
*/

typedef struct
{
    struct list_head list; /**< needed by domain command lists */
    struct list_head queue; /**< master command queue item */
    ec_command_type_t type; /**< command type (APRD, BWR, etc) */
    ec_address_t address; /**< receipient address */
    uint8_t *data; /**< command data */
    size_t mem_size; /**< command \a data memory size */
    size_t data_size; /**< size of the data in \a data */
    uint8_t index; /**< command index (set by master) */
    uint16_t working_counter; /**< working counter */
    ec_command_state_t state; /**< command state */
    cycles_t t_sent; /**< time, the commands was sent */
}
ec_command_t;

/*****************************************************************************/

void ec_command_init(ec_command_t *);
void ec_command_clear(ec_command_t *);

int ec_command_nprd(ec_command_t *, uint16_t, uint16_t, size_t);
int ec_command_npwr(ec_command_t *, uint16_t, uint16_t, size_t);
int ec_command_aprd(ec_command_t *, uint16_t, uint16_t, size_t);
int ec_command_apwr(ec_command_t *, uint16_t, uint16_t, size_t);
int ec_command_brd(ec_command_t *, uint16_t, size_t);
int ec_command_bwr(ec_command_t *, uint16_t, size_t);
int ec_command_lrw(ec_command_t *, uint32_t, size_t);

/*****************************************************************************/

#endif
