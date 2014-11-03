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

#include <iostream>
#include <iomanip>
#include <list>
#include <string.h>
using namespace std;

#include "CommandEoe.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandEoe::CommandEoe():
    Command("eoe", "Display Ethernet over EtherCAT statictics.")
{
}

/*****************************************************************************/

string CommandEoe::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "The TxRate and RxRate are displayed in Byte/s." << endl
        << endl;

    return str.str();
}

/****************************************************************************/

void CommandEoe::execute(const StringVector &args)
{
	MasterIndexList masterIndices;
    ec_ioctl_master_t master;
    unsigned int i;
    ec_ioctl_eoe_handler_t eoe;
    bool doIndent;
    string indent;

    if (args.size()) {
        stringstream err;
        err << "'" << getName() << "' takes no arguments!";
        throwInvalidUsageException(err);
    }

	masterIndices = getMasterIndices();
    doIndent = masterIndices.size();
    indent = doIndent ? "  " : "";
    MasterIndexList::const_iterator mi;
    for (mi = masterIndices.begin();
            mi != masterIndices.end(); mi++) {
        MasterDevice m(*mi);
        m.open(MasterDevice::Read);
        m.getMaster(&master);

        if (master.eoe_handler_count) {
            if (doIndent) {
                cout << "Master" << dec << *mi << endl;
            }

            cout << indent << "Interface  Slave  State  "
                << "RxBytes  RxRate  "
                << "TxBytes  TxRate  TxQueue"
                << endl;
        }

        for (i = 0; i < master.eoe_handler_count; i++) {
            stringstream queue;

            m.getEoeHandler(&eoe, i);

            queue << eoe.tx_queued_frames << "/" << eoe.tx_queue_size;

            cout << indent
                << setw(9) << eoe.name << "  "
                << setw(5) << dec << eoe.slave_position << "  "
                << setw(5) << (eoe.open ? "up" : "down") << "  "
                << setw(7) << eoe.rx_bytes << "  "
                << setw(6) << eoe.rx_rate << "  "
                << setw(7) << eoe.tx_bytes << "  "
                << setw(6) << eoe.tx_rate << "  "
                << setw(7) << queue.str()
                << endl;
        }
    }
}

/*****************************************************************************/
