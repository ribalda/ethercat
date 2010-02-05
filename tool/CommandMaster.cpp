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
using namespace std;

#include "CommandMaster.h"

#define MAX_TIME_STR_SIZE 50

/*****************************************************************************/

CommandMaster::CommandMaster():
    Command("master", "Show master and Ethernet device information.")
{
}

/****************************************************************************/

string CommandMaster::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "Command-specific options:" << endl
        << "  --master -m <index>  Index of the master to use. Default: 0."
        << endl << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

void CommandMaster::execute(MasterDevice &m, const StringVector &args)
{
    ec_ioctl_master_t data;
    stringstream err;
    unsigned int i;
    time_t epoch;
    char time_str[MAX_TIME_STR_SIZE + 1];
    size_t time_str_size;
    
    if (args.size()) {
        err << "'" << getName() << "' takes no arguments!";
        throwInvalidUsageException(err);
    }

    m.open(MasterDevice::Read);
    m.getMaster(&data);

    cout
        << "Master" << m.getIndex() << endl
        << "  Phase: ";

    switch (data.phase) {
        case 0:  cout << "Waiting for device..."; break;
        case 1:  cout << "Idle"; break;
        case 2:  cout << "Operation"; break;
        default: cout << "???";
    }

    cout << endl
        << "  Active: " << (data.active ? "yes" : "no") << endl
        << "  Slaves: " << data.slave_count << endl
        << "  Ethernet devices:" << endl;

    for (i = 0; i < 2; i++) {
        cout << "    " << (i == 0 ? "Main" : "Backup") << ": ";
        if (data.devices[i].address[0] == 0x00
                && data.devices[i].address[1] == 0x00
                && data.devices[i].address[2] == 0x00
                && data.devices[i].address[3] == 0x00
                && data.devices[i].address[4] == 0x00
                && data.devices[i].address[5] == 0x00) {
            cout << "None.";
        } else {
            cout << hex << setfill('0')
                << setw(2) << (unsigned int) data.devices[i].address[0] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[1] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[2] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[3] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[4] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[5] << " ("
                << (data.devices[i].attached ? "attached" : "waiting...")
                << ")" << endl << dec
                << "      Link: " << (data.devices[i].link_state ? "UP" : "DOWN") << endl
                << "      Tx count: " << data.devices[i].tx_count << endl
                << "      Rx count: " << data.devices[i].rx_count;
        }
        cout << endl;
    }

    cout << "  Distributed clocks:" << endl
        << "    Reference clock: ";
    if (data.ref_clock != 0xffff) {
        cout << "Slave " << dec << data.ref_clock;
    } else {
        cout << "None";
    }
    cout << endl
        << "    Application time: " << data.app_time << endl
        << "                      ";

    epoch = data.app_time / 1000000000 + 946684800ULL;
    time_str_size = strftime(time_str, MAX_TIME_STR_SIZE,
            "%Y-%m-%d %H:%M:%S", gmtime(&epoch));
    cout << string(time_str, time_str_size) << "."
        << setfill('0') << setw(9) << data.app_time % 1000000000 << endl;
}

/*****************************************************************************/
