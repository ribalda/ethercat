/*****************************************************************************
 *
 *  Copyright (C) 2006-2017  Florian Pose, Ingenieurgemeinschaft IgH
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

#include "CommandCrc.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandCrc::CommandCrc():
    Command("crc", "CRC error register diagnosis.")
{
}

/*****************************************************************************/

string CommandCrc::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName()
        << " [OPTIONS]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "CRC - CRC Error Counter                0x300, 0x302, 0x304, 0x306"
        << endl
        << "PHY - Physical Interface Error Counter 0x301, 0x303, 0x305, 0x307"
        << endl
        << "FWD - Forwarded RX Error Counter       0x308, 0x309, 0x30a, 0x30b"
        << endl
        << "LNK - Lost Link Counter                0x310, 0x311, 0x312, 0x313"
        << endl
        << endl;

    return str.str();
}

/****************************************************************************/

#define NUM_PORTS (4)

void CommandCrc::execute(const StringVector &args)
{

    MasterDevice m(getSingleMasterIndex());
    m.open(MasterDevice::Read);

    ec_ioctl_master_t master;
    m.getMaster(&master);


    cout << "   |";
    for (unsigned int port = 0; port < NUM_PORTS; port++) {
        cout << "Port " << port << "         |";
    }
    cout << endl;

    cout << "   |";
    for (unsigned int port = 0; port < NUM_PORTS; port++) {
        cout << "CRC PHY FWD LNK|";
    }
    cout << endl;

    ec_ioctl_slave_reg_t io;
    io.emergency = 0;
    io.address = 0x0300;
    io.size = 20;
    io.data = new uint8_t[20];

    for (unsigned int i = 0; i < master.slave_count; i++) {

        io.slave_position = i;
        try {
            m.readReg(&io);
        } catch (MasterDeviceException &e) {
            delete [] io.data;
            throw e;
        }

        cout << setw(3) << i << "|";
        for (int port = 0; port < 4; port++) {
            cout << setw(3) << (unsigned int) io.data[ 0 + port * 2]; // CRC
            cout << setw(4) << (unsigned int) io.data[ 1 + port * 2]; // PHY
            cout << setw(4) << (unsigned int) io.data[ 8 + port]; // FWD
            cout << setw(4) << (unsigned int) io.data[16 + port]; // LNK
            cout << "|";
        }

        ec_ioctl_slave_t slave;
        m.getSlave(&slave, i);
        std::string slaveName(slave.name);
        slaveName = slaveName.substr(0, 11);
        cout << slaveName << endl;
    }

    delete [] io.data;
}

/*****************************************************************************/
