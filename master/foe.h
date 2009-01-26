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
 *  Using the EtherCAT technology and brand is permitted in compliance with
 *  the industrial property and similar rights of Beckhoff Automation GmbH.
 *
 *****************************************************************************/

#ifndef __FOE_H__
#define __FOE_H__

/*****************************************************************************/

typedef enum {
    FOE_BUSY               = 0,
    FOE_READY              = 1,
    FOE_IDLE               = 2,
    FOE_WC_ERROR           = 3,
    FOE_RECEIVE_ERROR      = 4,
    FOE_PROT_ERROR         = 5,
    FOE_NODATA_ERROR       = 6,
    FOE_PACKETNO_ERROR     = 7,
    FOE_OPMODE_ERROR       = 8,
    FOE_TIMEOUT_ERROR      = 9,
    FOE_SEND_RX_DATA_ERROR = 10,
    FOE_RX_DATA_ACK_ERROR  = 11,
    FOE_ACK_ERROR          = 12,
    FOE_MBOX_FETCH_ERROR   = 13,
    FOE_READ_NODATA_ERROR  = 14,
    FOE_MBOX_PROT_ERROR    = 15,
} ec_foe_error_t;

/*****************************************************************************/

#endif
