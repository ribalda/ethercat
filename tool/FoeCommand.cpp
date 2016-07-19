/*****************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2009  Florian Pose, Ingenieurgemeinschaft IgH
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
 ****************************************************************************/

#include "FoeCommand.h"

/*****************************************************************************/

FoeCommand::FoeCommand(const string &name, const string &briefDesc):
    Command(name, briefDesc)
{
}

/****************************************************************************/

std::string FoeCommand::resultText(int result)
{
    switch (result) {
        case FOE_BUSY:
            return "FOE_BUSY";
        case FOE_READY:
            return "FOE_READY";
        case FOE_IDLE:
            return "FOE_IDLE";
        case FOE_WC_ERROR:
            return "FOE_WC_ERROR";
        case FOE_RECEIVE_ERROR:
            return "FOE_RECEIVE_ERROR";
        case FOE_PROT_ERROR:
            return "FOE_PROT_ERROR";
        case FOE_NODATA_ERROR:
            return "FOE_NODATA_ERROR";
        case FOE_PACKETNO_ERROR:
            return "FOE_PACKETNO_ERROR";
        case FOE_OPCODE_ERROR:
            return "FOE_OPCODE_ERROR";
        case FOE_TIMEOUT_ERROR:
            return "FOE_TIMEOUT_ERROR";
        case FOE_SEND_RX_DATA_ERROR:
            return "FOE_SEND_RX_DATA_ERROR";
        case FOE_RX_DATA_ACK_ERROR:
            return "FOE_RX_DATA_ACK_ERROR";
        case FOE_ACK_ERROR:
            return "FOE_ACK_ERROR";
        case FOE_MBOX_FETCH_ERROR:
            return "FOE_MBOX_FETCH_ERROR";
        case FOE_READ_NODATA_ERROR:
            return "FOE_READ_NODATA_ERROR";
        case FOE_MBOX_PROT_ERROR:
            return "FOE_MBOX_PROT_ERROR";
        case FOE_READ_OVER_ERROR:
            return "FOE_READ_OVER_ERROR";
        default:
            return "???";
    }
}

/****************************************************************************/

std::string FoeCommand::errorText(int errorCode)
{
    switch (errorCode) {
        case 0x00008001:
            return "Not found.";
        case 0x00008002:
            return "Access denied.";
        case 0x00008003:
            return "Disk full.";
        case 0x00008004:
            return "Illegal.";
        case 0x00008005:
            return "Packet number wrong.";
        case 0x00008006:
            return "Already exists.";
        case 0x00008007:
            return "No user.";
        case 0x00008008:
            return "Bootstrap only.";
        case 0x00008009:
            return "Not Bootstrap.";
        case 0x0000800a:
            return "No rights.";
        case 0x0000800b:
            return "Program Error.";
        default:
            return "Unknown error code";
    }
}

/****************************************************************************/
