/*****************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2016  Ralf Roesch, Roesch & Walter Industrie-Elektronik GmbH
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

#ifndef __COMMANDDIAG_H__
#define __COMMANDDIAG_H__

#include "Command.h"
#include "DataTypeHandler.h"

/****************************************************************************/

class CommandDiag:
    public Command,
    public DataTypeHandler
{
    public:
        CommandDiag();

        string helpString(const string &) const;
        void execute(const StringVector &);

    protected:
        struct Info {
            string pos;
            string alias;
            string relPos;
            string state;
            string flag;
            string name;
            unsigned int device;
            unsigned ESCerrors;
            string sESCerrors;
            unsigned int ESC_DL_Status;
            unsigned int Invalid_Frame_Counter[EC_MAX_PORTS];
            unsigned int RX_Error_Counter[EC_MAX_PORTS];
            unsigned int Forwarded_RX_Error_Counter[EC_MAX_PORTS];
            unsigned int ECAT_Processing_Unit_Error_Counter;
            unsigned int Lost_Link_Counter[EC_MAX_PORTS];
        };

    private:
        void CheckallSlaves(MasterDevice &, const SlaveList &, bool);
        static bool slaveInList(const ec_ioctl_slave_t &, const SlaveList &);
        void EscRegRead(MasterDevice &, uint16_t, uint16_t, uint8_t*, size_t);
        void EscRegReadWrite(MasterDevice &, uint16_t, uint16_t, uint8_t*, size_t);
};

/****************************************************************************/

#endif
