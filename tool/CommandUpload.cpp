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

#include "CommandUpload.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandUpload::CommandUpload():
    SdoCommand("upload", "Read an SDO entry from a slave.")
{
}

/*****************************************************************************/

string CommandUpload::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName()
        << " [OPTIONS] <INDEX> <SUBINDEX>" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "This command requires a single slave to be selected." << endl
        << endl
        << "The data type of the SDO entry is taken from the SDO" << endl
        << "dictionary by default. It can be overridden with the" << endl
        << "--type option. If the slave does not support the SDO" << endl
        << "information service or the SDO is not in the dictionary," << endl
        << "the --type option is mandatory."  << endl
        << endl
        << typeInfo()
        << endl
        << "Arguments:" << endl
        << "  INDEX    is the SDO index and must be an unsigned" << endl
        << "           16 bit number." << endl
        << "  SUBINDEX is the SDO entry subindex and must be an" << endl
        << "           unsigned 8 bit number." << endl
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

void CommandUpload::execute(const StringVector &args)
{
    SlaveList slaves;
    stringstream err, strIndex, strSubIndex;
    ec_ioctl_slave_sdo_upload_t data;
    const DataType *dataType = NULL;
    unsigned int uval;

    if (args.size() != 2) {
        err << "'" << getName() << "' takes two arguments!";
        throwInvalidUsageException(err);
    }

    strIndex << args[0];
    strIndex
        >> resetiosflags(ios::basefield) // guess base from prefix
        >> data.sdo_index;
    if (strIndex.fail()) {
        err << "Invalid SDO index '" << args[0] << "'!";
        throwInvalidUsageException(err);
    }

    strSubIndex << args[1];
    strSubIndex
        >> resetiosflags(ios::basefield) // guess base from prefix
        >> uval;
    if (strSubIndex.fail() || uval > 0xff) {
        err << "Invalid SDO subindex '" << args[1] << "'!";
        throwInvalidUsageException(err);
    }
    data.sdo_entry_subindex = uval;

    MasterDevice m(getSingleMasterIndex());
    m.open(MasterDevice::Read);
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
                << "Please specify --type or fetch directory "
                << "with ethercat sdos command.";
            throwCommandException(err);
        }
        if (!(dataType = findDataType(entry.data_type))) {
            err << "PDO entry has unknown data type 0x"
                << hex << setfill('0') << setw(4) << entry.data_type << "!"
                << " Please specify --type or fetch directory "
                << "with ethercat sdos command.";;
            throwCommandException(err);
        }
    }

    if (dataType->byteSize) {
        data.target_size = dataType->byteSize;
    } else {
        data.target_size = DefaultBufferSize;
    }

    data.target = new uint8_t[data.target_size + 1];

    try {
        m.sdoUpload(&data);
    } catch (MasterDeviceSdoAbortException &e) {
        delete [] data.target;
        err << "SDO transfer aborted with code 0x"
            << setfill('0') << hex << setw(8) << e.abortCode
            << ": " << abortText(e.abortCode);
        throwCommandException(err);
    } catch (MasterDeviceException &e) {
        delete [] data.target;
        throw e;
    }

    m.close();

    try {
        outputData(cout, dataType, data.target, data.data_size);
    } catch (SizeException &e) {
        delete [] data.target;
        throwCommandException(e.what());
    }

    delete [] data.target;
}

/*****************************************************************************/
