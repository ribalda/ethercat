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
using namespace std;

#include "CommandSoeRead.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandSoeRead::CommandSoeRead():
    SoeCommand("soe_read", "Read an SoE IDN from a slave.")
{
}

/*****************************************************************************/

string CommandSoeRead::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS] <IDN>" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "This command requires a single slave to be selected." << endl
        << endl
        << "Arguments:" << endl
        << "  IDN      is the IDN and must be either an unsigned" << endl
        << "           16 bit number acc. to IEC 61800-7-204:" << endl
        << "             Bit 15: (0) Standard data, (1) Product data" << endl
        << "             Bit 14 - 12: Parameter set (0 - 7)" << endl
        << "             Bit 11 - 0: Data block number" << endl
        << "           or a string like 'P-0-150'." << endl
        << endl
        << "Data of the given IDN are read and displayed according to" << endl
        << "the given datatype, or as raw hex bytes." << endl
        << endl
        << typeInfo()
        << endl
        << "Command-specific options:" << endl
        << "  --alias    -a <alias>" << endl
        << "  --position -p <pos>    Slave selection. See the help of" << endl
        << "                         the 'slaves' command." << endl
        << "  --type     -t <type>   Data type (see above)." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

void CommandSoeRead::execute(const StringVector &args)
{
    SlaveList slaves;
    stringstream err;
    const DataType *dataType = NULL;
    ec_ioctl_slave_soe_read_t ioctl;

    if (args.size() != 1) {
        err << "'" << getName() << "' takes one argument!";
        throwInvalidUsageException(err);
    }

    ioctl.drive_no = 0; // FIXME

    try {
        ioctl.idn = parseIdn(args[0]);
    } catch (runtime_error &e) {
        err << "Invalid IDN '" << args[0] << "': " << e.what();
        throwInvalidUsageException(err);
    }

    MasterDevice m(getSingleMasterIndex());
    m.open(MasterDevice::Read);
    slaves = selectedSlaves(m);
    if (slaves.size() != 1) {
        throwSingleSlaveRequired(slaves.size());
    }
    ioctl.slave_position = slaves.front().position;

    if (getDataType().empty()) {
        dataType = findDataType("raw"); // FIXME
    } else { // no data type specified
        if (!(dataType = findDataType(getDataType()))) {
            err << "Invalid data type '" << getDataType() << "'!";
            throwInvalidUsageException(err);
        }
    }

    if (dataType->byteSize) {
        ioctl.mem_size = dataType->byteSize;
    } else {
        ioctl.mem_size = 1024;
    }

    ioctl.data = new uint8_t[ioctl.mem_size + 1];

    try {
        m.readSoe(&ioctl);
    } catch (MasterDeviceSoeException &e) {
        delete [] ioctl.data;
        err << "SoE read command failed with code " << errorMsg(e.errorCode);
        throwCommandException(err);
    } catch (MasterDeviceException &e) {
        delete [] ioctl.data;
        throw e;
    }

    m.close();

    try {
        outputData(cout, dataType, ioctl.data, ioctl.data_size);
    } catch (SizeException &e) {
        delete [] ioctl.data;
        throwCommandException(e.what());
    }

    delete [] ioctl.data;
}

/*****************************************************************************/
