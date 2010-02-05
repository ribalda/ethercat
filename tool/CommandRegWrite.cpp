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
#include <fstream>
using namespace std;

#include "CommandRegWrite.h"
#include "sii_crc.h"

/*****************************************************************************/

CommandRegWrite::CommandRegWrite():
    CommandReg("reg_write", "Write data to a slave's registers.")
{
}

/*****************************************************************************/

string CommandRegWrite::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS] <OFFSET> <DATA>" << endl
        << endl 
        << getBriefDescription() << endl
        << endl
        << "This command requires a single slave to be selected." << endl
        << endl
        << "Arguments:" << endl
        << "  OFFSET  is the register address to write to." << endl
        << "  DATA    depends on whether a datatype was specified" << endl
        << "          with the --type option: If not, DATA must be" << endl
        << "          either a path to a file with data to write," << endl
        << "          or '-', which means, that data are read from" << endl
        << "          stdin. If a datatype was specified, VALUE is" << endl
        << "          interpreted respective to the given type." << endl
        << endl
        << "These are the valid data types:" << endl
        << "  int8, int16, int32, int64, uint8, uint16, uint32," << endl
        << "  uint64, string." << endl
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

void CommandRegWrite::execute(MasterDevice &m, const StringVector &args)
{
    stringstream strOffset, err;
    ec_ioctl_slave_reg_t data;
    ifstream file;
    SlaveList slaves;

    if (args.size() != 2) {
        err << "'" << getName() << "' takes exactly two arguments!";
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

    if (getDataType().empty()) {
        if (args[1] == "-") {
            loadRegData(&data, cin);
        } else {
            file.open(args[1].c_str(), ifstream::in | ifstream::binary);
            if (file.fail()) {
                err << "Failed to open '" << args[1] << "'!";
                throwCommandException(err);
            }
            loadRegData(&data, file);
            file.close();
        }
    } else {
        stringstream strValue;
        const DataType *dataType = findDataType(getDataType());

        if (!dataType) {
            err << "Invalid data type '" << getDataType() << "'!";
            throwInvalidUsageException(err);
        }

        if (dataType->byteSize) {
            data.length = dataType->byteSize;
            data.data = new uint8_t[data.length];
        }

        strValue << args[1];
        strValue >> resetiosflags(ios::basefield); // guess base from prefix
        strValue.exceptions(ios::failbit);

        try {
            if (string(dataType->name) == "int8") {
                int16_t val; // uint8_t is interpreted as char
                strValue >> val;
                if (val > 127 || val < -128)
                    throw ios::failure("Value out of range");
                *data.data = (int8_t) val;
            } else if (string(dataType->name) == "int16") {
                int16_t val;
                strValue >> val;
                *(int16_t *) data.data = cpu_to_le16(val);
            } else if (string(dataType->name) == "int32") {
                int32_t val;
                strValue >> val;
                *(int32_t *) data.data = cpu_to_le32(val);
            } else if (string(dataType->name) == "int64") {
                int64_t val;
                strValue >> val;
                *(int64_t *) data.data = cpu_to_le64(val);
            } else if (string(dataType->name) == "uint8") {
                uint16_t val; // uint8_t is interpreted as char
                strValue >> val;
                if (val > 0xff)
                    throw ios::failure("Value out of range");
                *data.data = (uint8_t) val;
            } else if (string(dataType->name) == "uint16") {
                uint16_t val;
                strValue >> val;
                *(uint16_t *) data.data = cpu_to_le16(val);
            } else if (string(dataType->name) == "uint32") {
                uint32_t val;
                strValue >> val;
                *(uint32_t *) data.data = cpu_to_le32(val);
            } else if (string(dataType->name) == "uint64") {
                uint64_t val;
                strValue >> val;
                *(uint64_t *) data.data = cpu_to_le64(val);
            } else if (string(dataType->name) == "string" ||
                    string(dataType->name) == "octet_string") {
                data.length = strValue.str().size();
                if (!data.length) {
                    err << "Zero-size string now allowed!";
                    throwCommandException(err);
                }
                data.data = new uint8_t[data.length];
                strValue >> (char *) data.data;
            } else {
                err << "Invalid data type " << dataType->name;
                throwCommandException(err);
            }
        } catch (ios::failure &e) {
            delete [] data.data;
            err << "Invalid value argument '" << args[1]
                << "' for type '" << dataType->name << "'!";
            throwInvalidUsageException(err);
        }
    }

    if ((uint32_t) data.offset + data.length > 0xffff) {
        err << "Offset and length exceeding 64k!";
        delete [] data.data;
        throwInvalidUsageException(err);
    }

    try {
        m.open(MasterDevice::ReadWrite);
    } catch (MasterDeviceException &e) {
        delete [] data.data;
        throw e;
    }

    slaves = selectedSlaves(m);
    if (slaves.size() != 1) {
        delete [] data.data;
        throwSingleSlaveRequired(slaves.size());
    }
    data.slave_position = slaves.front().position;

    // send data to master
    try {
        m.writeReg(&data);
    } catch (MasterDeviceException &e) {
        delete [] data.data;
        throw e;
    }

    if (getVerbosity() == Verbose) {
        cerr << "Register writing finished." << endl;
    }

    delete [] data.data;
}

/*****************************************************************************/

void CommandRegWrite::loadRegData(
        ec_ioctl_slave_reg_t *data,
        const istream &in
        )
{
    stringstream err;
    ostringstream tmp;

    tmp << in.rdbuf();
    string const &contents = tmp.str();

    if (getVerbosity() == Verbose) {
        cerr << "Read " << contents.size() << " bytes of data." << endl;
    }

    if (contents.size() > 0xffff) {
        err << "Invalid data size " << contents.size() << "!";
        throwInvalidUsageException(err);
    }
    data->length = contents.size();

    // allocate buffer and read file into buffer
    data->data = new uint8_t[data->length];
    contents.copy((char *) data->data, contents.size());
}

/*****************************************************************************/
