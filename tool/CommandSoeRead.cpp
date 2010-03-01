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

#include "CommandSoeRead.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandSoeRead::CommandSoeRead():
    Command("soe_read", "Read an SoE IDN from a slave.")
{
}

/*****************************************************************************/

string CommandSoeRead::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS] <INDEX> <SUBINDEX>" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "This command requires a single slave to be selected." << endl
        << endl
        << "Arguments:" << endl
        << "  IDN      is the IDN and must be an unsigned" << endl
        << "           16 bit number." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --alias    -a <alias>" << endl
        << "  --position -p <pos>    Slave selection. See the help of" << endl
        << "                         the 'slaves' command." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

void CommandSoeRead::execute(const StringVector &args)
{
    SlaveList slaves;
    stringstream err, strIdn;
    ec_ioctl_slave_soe_t data;

    if (args.size() != 1) {
        err << "'" << getName() << "' takes one argument!";
        throwInvalidUsageException(err);
    }

    strIdn << args[0];
    strIdn
        >> resetiosflags(ios::basefield) // guess base from prefix
        >> data.idn;
    if (strIdn.fail()) {
        err << "Invalid IDN '" << args[0] << "'!";
        throwInvalidUsageException(err);
    }

    if (getMasterIndices().size() != 1) {
        err << getName() << " requires to select a single master!";
        throwInvalidUsageException(err);
    }
    MasterDevice m(getMasterIndices().front());
    m.open(MasterDevice::Read);
    slaves = selectedSlaves(m);
    if (slaves.size() != 1) {
        throwSingleSlaveRequired(slaves.size());
    }
    data.slave_position = slaves.front().position;

	data.mem_size = 1024;
    data.data = new uint8_t[data.mem_size + 1];

    try {
        m.readSoe(&data);
    } catch (MasterDeviceSoeException &e) {
        delete [] data.data;
        err << "CoE read command aborted with code 0x"
            << setfill('0') << hex << setw(4) << e.errorCode;
        throwCommandException(err);
    } catch (MasterDeviceException &e) {
        delete [] data.data;
        throw e;
    }

    m.close();

	printRawData(data.data, data.data_size);

    delete [] data.data;
}

/****************************************************************************/

void CommandSoeRead::printRawData(
        const uint8_t *data,
        unsigned int size
        )
{
    cout << hex << setfill('0');
    while (size--) {
        cout << "0x" << setw(2) << (unsigned int) *data++;
        if (size)
            cout << " ";
    }
    cout << endl;
}

/*****************************************************************************/
