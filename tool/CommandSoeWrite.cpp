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

#include "CommandSoeWrite.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandSoeWrite::CommandSoeWrite():
    SoeCommand("soe_write", "Write an SoE IDN to a slave.")
{
}

/*****************************************************************************/

string CommandSoeWrite::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS] <IDN> <VALUE>" << endl
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
        << "  VALUE    is the value to write and is interpreted" << endl
        << "           as the given datatype (see above)." << endl
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

void CommandSoeWrite::execute(const StringVector &args)
{
    stringstream strIdn, err;
    const DataType *dataType = NULL;
    ec_ioctl_slave_soe_write_t ioctl;
    SlaveList slaves;
    size_t memSize;

    if (args.size() != 2) {
        err << "'" << getName() << "' takes 2 arguments!";
        throwInvalidUsageException(err);
    }

    try {
        ioctl.idn = parseIdn(args[0]);
    } catch (runtime_error &e) {
        err << "Invalid IDN '" << args[0] << "': " << e.what();
        throwInvalidUsageException(err);
    }

    MasterDevice m(getSingleMasterIndex());
    m.open(MasterDevice::ReadWrite);
    slaves = selectedSlaves(m);
    if (slaves.size() != 1) {
        throwSingleSlaveRequired(slaves.size());
    }
    ioctl.slave_position = slaves.front().position;

    if (!getDataType().empty()) { // data type specified
        if (!(dataType = findDataType(getDataType()))) {
            err << "Invalid data type '" << getDataType() << "'!";
            throwInvalidUsageException(err);
        }
    } else { // no data type specified
        err << "Please specify a data type.";
        throwInvalidUsageException(err); // FIXME read from stream
    }

    if (dataType->byteSize) {
        memSize = dataType->byteSize;
    } else {
        // guess string type size
        memSize = args[1].size() + 1;
        if (!memSize) {
            err << "Empty argument not allowed.";
            throwInvalidUsageException(err);
        }
    }

    ioctl.data = new uint8_t[memSize];

    try {
        ioctl.data_size = interpretAsType(
                dataType, args[1], ioctl.data, memSize);
    } catch (SizeException &e) {
        delete [] ioctl.data;
        throwCommandException(e.what());
    } catch (ios::failure &e) {
        delete [] ioctl.data;
        err << "Invalid value argument '" << args[1]
            << "' for type '" << dataType->name << "'!";
        throwInvalidUsageException(err);
    }

    try {
        m.writeSoe(&ioctl);
    } catch (MasterDeviceSoeException &e) {
        delete [] ioctl.data;
        err << "SoE write command aborted with code 0x"
            << setfill('0') << hex << setw(4) << e.errorCode << ".";
        throwCommandException(err);
    } catch (MasterDeviceException &e) {
        delete [] ioctl.data;
        throw e;
    }

    delete [] ioctl.data;
}

/*****************************************************************************/
