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

#include "CommandDownload.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandDownload::CommandDownload():
    SdoCommand("download", "Write an SDO entry to a slave.")
{
}

/*****************************************************************************/

string CommandDownload::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName()
        << " [OPTIONS] <INDEX> <SUBINDEX> <VALUE>" << endl
        << " [OPTIONS] <INDEX> <VALUE>" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "This command requires a single slave to be selected." << endl
        << endl
        << "The data type of the SDO entry is taken from the SDO" << endl
        << "dictionary by default. It can be overridden with the" << endl
        << "--type option. If the slave does not support the SDO" << endl
        << "information service or the SDO is not in the dictionary," << endl
        << "the --type option is mandatory." << endl
        << endl
        << "The second call (without <SUBINDEX>) uses the complete" << endl
        << "access method." << endl
        << endl
        << typeInfo()
        << endl
        << "Arguments:" << endl
        << "  INDEX    is the SDO index and must be an unsigned" << endl
        << "           16 bit number." << endl
        << "  SUBINDEX is the SDO entry subindex and must be an" << endl
        << "           unsigned 8 bit number." << endl
        << "  VALUE    is the value to download and must correspond" << endl
        << "           to the SDO entry datatype (see above). Use" << endl
        << "           '-' to read from standard input." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --alias    -a <alias>" << endl
        << "  --position -p <pos>    Slave selection. See the help of" << endl
        << "                         the 'slaves' command." << endl
        << "  --type     -t <type>   SDO entry data type (see above)." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

void CommandDownload::execute(const StringVector &args)
{
    stringstream strIndex, err;
    ec_ioctl_slave_sdo_download_t data;
    unsigned int valueIndex;
    const DataType *dataType = NULL;
    SlaveList slaves;

    if (args.size() != 2 && args.size() != 3) {
        err << "'" << getName() << "' takes 2 or 3 arguments!";
        throwInvalidUsageException(err);
    }
    data.complete_access = args.size() == 2;
    valueIndex = data.complete_access ? 1 : 2;

    strIndex << args[0];
    strIndex
        >> resetiosflags(ios::basefield) // guess base from prefix
        >> data.sdo_index;
    if (strIndex.fail()) {
        err << "Invalid SDO index '" << args[0] << "'!";
        throwInvalidUsageException(err);
    }

    if (data.complete_access) {
        data.sdo_entry_subindex = 0;
    } else {
        stringstream strSubIndex;
        unsigned int number;

        strSubIndex << args[1];
        strSubIndex
            >> resetiosflags(ios::basefield) // guess base from prefix
            >> number;
        if (strSubIndex.fail() || number > 0xff) {
            err << "Invalid SDO subindex '" << args[1] << "'!";
            throwInvalidUsageException(err);
        }
        data.sdo_entry_subindex = number;
    }

    MasterDevice m(getSingleMasterIndex());
    m.open(MasterDevice::ReadWrite);
    slaves = selectedSlaves(m);
    if (slaves.size() != 1) {
        throwSingleSlaveRequired(slaves.size());
    }
    data.slave_position = slaves.front().position;

    if (!getDataType().empty()) { // data type specified
        if (!(dataType = findDataType(getDataType()))) {
            err << "Invalid data type '" << getDataType() << "'!";
            throwInvalidUsageException(err);
        }
    } else { // no data type specified: fetch from dictionary
        ec_ioctl_slave_sdo_entry_t entry;

        try {
            m.getSdoEntry(&entry, data.slave_position,
                    data.sdo_index, data.sdo_entry_subindex);
        } catch (MasterDeviceException &e) {
            err << "Failed to determine SDO entry data type. "
                << "Please specify --type.";
            throwCommandException(err);
        }
        if (!(dataType = findDataType(entry.data_type))) {
            err << "PDO entry has unknown data type 0x"
                << hex << setfill('0') << setw(4) << entry.data_type << "!"
                << " Please specify --type.";
            throwCommandException(err);
        }
    }

    if (args[valueIndex] == "-") {
        ostringstream tmp;

        tmp << cin.rdbuf();
        string const &contents = tmp.str();

        if (!contents.size()) {
            err << "Invalid data size " << contents.size() << "! "
                << "Must be non-zero.";
            throwCommandException(err);
        }
        data.data_size = contents.size();
        data.data = new uint8_t[data.data_size + 1];

        try {
            data.data_size = interpretAsType(
                    dataType, contents, data.data, data.data_size);
        } catch (SizeException &e) {
            delete [] data.data;
            throwCommandException(e.what());
        } catch (ios::failure &e) {
            delete [] data.data;
            err << "Invalid value argument '" << args[2]
                << "' for type '" << dataType->name << "'!";
            throwInvalidUsageException(err);
        }

    } else {
        if (dataType->byteSize) {
            data.data_size = dataType->byteSize;
        } else {
            data.data_size = DefaultBufferSize;
        }

        data.data = new uint8_t[data.data_size + 1];

        try {
            data.data_size = interpretAsType(
                    dataType, args[valueIndex], data.data, data.data_size);
        } catch (SizeException &e) {
            delete [] data.data;
            throwCommandException(e.what());
        } catch (ios::failure &e) {
            delete [] data.data;
            err << "Invalid value argument '" << args[2]
                << "' for type '" << dataType->name << "'!";
            throwInvalidUsageException(err);
        }
    }

    try {
        data.data_size = interpretAsType(
                dataType, args[valueIndex], data.data, data.data_size);
    } catch (SizeException &e) {
        delete [] data.data;
        throwCommandException(e.what());
    } catch (ios::failure &e) {
        delete [] data.data;
        err << "Invalid value argument '" << args[2]
            << "' for type '" << dataType->name << "'!";
        throwInvalidUsageException(err);
    }

    try {
        m.sdoDownload(&data);
    } catch (MasterDeviceSdoAbortException &e) {
        delete [] data.data;
        err << "SDO transfer aborted with code 0x"
            << setfill('0') << hex << setw(8) << e.abortCode
            << ": " << abortText(e.abortCode);
        throwCommandException(err);
    } catch(MasterDeviceException &e) {
        delete [] data.data;
        throw e;
    }

    delete [] data.data;
}

/*****************************************************************************/
