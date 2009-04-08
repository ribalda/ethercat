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

#include "CommandRegRead.h"

/*****************************************************************************/

CommandRegRead::CommandRegRead():
    Command("reg_read", "Output a slave's register contents.")
{
}

/*****************************************************************************/

string CommandRegRead::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS] <OFFSET> [LENGTH]" << endl
    	<< endl
    	<< getBriefDescription() << endl
    	<< endl
        << "This command requires a single slave to be selected." << endl
    	<< endl
        << "Arguments:" << endl
        << "  OFFSET is the register address. Must" << endl
        << "         be an unsigned 16 bit number." << endl
        << "  LENGTH is the number of bytes to read and must also be" << endl
        << "         an unsigned 16 bit number. OFFSET plus LENGTH" << endl
        << "         may not exceed 64k. The length is ignored (and" << endl
        << "         can be omitted), if a selected data type" << endl
        << "         implies a length." << endl
        << endl
        << "These are the valid data types:" << endl
        << "  int8, int16, int32, int64, uint8, uint16, uint32," << endl
        << "  uint64, string, octet_string, raw." << endl
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

void CommandRegRead::execute(MasterDevice &m, const StringVector &args)
{
    SlaveList slaves;
    ec_ioctl_slave_reg_t data;
    stringstream strOffset, err;
    const DataType *dataType = NULL;

    if (args.size() < 1 || args.size() > 2) {
        err << "'" << getName() << "' takes one or two arguments!";
        throwInvalidUsageException(err);
    }

    strOffset << args[0];
    strOffset
        >> resetiosflags(ios::basefield) // guess base from prefix
        >> data.offset;
    if (strOffset.fail()) {
        err << "Invalid offset '" << args[0] << "'!";
        throwInvalidUsageException(err);
    }

    if (args.size() > 1) {
        stringstream strLength;
        strLength << args[1];
        strLength
            >> resetiosflags(ios::basefield) // guess base from prefix
            >> data.length;
        if (strLength.fail()) {
            err << "Invalid length '" << args[1] << "'!";
            throwInvalidUsageException(err);
        }

        if (!data.length) {
            err << "Length may not be zero!";
            throwInvalidUsageException(err);
        }
    } else { // no length argument given
        data.length = 0;
    }

    if (!getDataType().empty()) {
        if (!(dataType = findDataType(getDataType()))) {
            err << "Invalid data type '" << getDataType() << "'!";
            throwInvalidUsageException(err);
        }

        if (dataType->byteSize) {
            // override length argument
            data.length = dataType->byteSize;
        }
    }

    if (!data.length) {
        err << "The length argument is mandatory, if no datatype is " << endl
            << "specified, or the datatype does not imply a length!";
        throwInvalidUsageException(err);
    }

    if ((uint32_t) data.offset + data.length > 0xffff) {
        err << "Offset and length exceeding 64k!";
        throwInvalidUsageException(err);
    }
    
    m.open(MasterDevice::Read);
    slaves = selectedSlaves(m);

    if (slaves.size() != 1) {
        throwSingleSlaveRequired(slaves.size());
    }
    data.slave_position = slaves.front().position;

    data.data = new uint8_t[data.length];

	try {
		m.readReg(&data);
	} catch (MasterDeviceException &e) {
        delete [] data.data;
		throw e;
	}

    cout << setfill('0');
    if (!dataType ||
            dataType->name == "string" ||
            dataType->name == "octet_string") {
        uint16_t i;
        for (i = 0; i < data.length; i++) {
            cout << data.data[i];
        }
    } else if (dataType->name == "int8") {
        int sval = *(int8_t *) data.data;
        cout << sval << " 0x" << hex << setw(2) << sval << endl;
    } else if (dataType->name == "int16") {
        int sval = le16_to_cpup(data.data);
        cout << sval << " 0x" << hex << setw(4) << sval << endl;
    } else if (dataType->name == "int32") {
        int sval = le32_to_cpup(data.data);
        cout << sval << " 0x" << hex << setw(8) << sval << endl;
    } else if (dataType->name == "int64") {
        long long int sval = le64_to_cpup(data.data);
        cout << sval << " 0x" << hex << setw(16) << sval << endl;
    } else if (dataType->name == "uint8") {
        unsigned int uval = (unsigned int) *(uint8_t *) data.data;
        cout << uval << " 0x" << hex << setw(2) << uval << endl;
    } else if (dataType->name == "uint16") {
        unsigned int uval = le16_to_cpup(data.data);
        cout << uval << " 0x" << hex << setw(4) << uval << endl;
    } else if (dataType->name == "uint32") {
        unsigned int uval = le32_to_cpup(data.data);
        cout << uval << " 0x" << hex << setw(8) << uval << endl;
    } else if (dataType->name == "uint64") {
        long long unsigned int uval = le32_to_cpup(data.data);
        cout << uval << " 0x" << hex << setw(8) << uval << endl;
    } else {
        uint8_t *d = data.data;
        unsigned int size = data.length;

        cout << hex << setfill('0');
        while (size--) {
            cout << "0x" << setw(2) << (unsigned int) *d++;
            if (size)
                cout << " ";
        }
        cout << endl;
    }

    delete [] data.data;
}

/****************************************************************************/

const CommandRegRead::DataType *CommandRegRead::findDataType(
        const string &str
        )
{
    const DataType *d;
    
    for (d = dataTypes; d->name; d++)
        if (str == d->name)
            return d;

    return NULL;
}

/****************************************************************************/

const CommandRegRead::DataType CommandRegRead::dataTypes[] = {
    {"int8",         1},
    {"int16",        2},
    {"int32",        4},
    {"int64",        8},
    {"uint8",        1},
    {"uint16",       2},
    {"uint32",       4},
    {"uint64",       8},
    {"string",       0},
    {"octet_string", 0},
    {"raw",          0},
    {}
};

/*****************************************************************************/
