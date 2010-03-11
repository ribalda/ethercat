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

#define DEBUG 0

#if DEBUG
#include <iostream>
#endif

#include <iomanip>
#include <sstream>
using namespace std;

#include "DataTypeHandler.h"

#include "ecrt.h"

/*****************************************************************************/

DataTypeHandler::DataTypeHandler()
{
}

/****************************************************************************/

const DataTypeHandler::DataType *DataTypeHandler::findDataType(
        const string &str
        )
{
    const DataType *d;
    
    for (d = dataTypes; d->name; d++)
        if (str == d->name)
            return d;

    return NULL; // FIXME exception
}

/****************************************************************************/

string DataTypeHandler::typeInfo()
{
	stringstream s;

	s
		<< "These are valid data types to use with" << endl
		<< "the --type option:" << endl
		<< "  int8, int16, int32, uint8, uint16, uint32, string," << endl
		<< "  octet_string." << endl;
	return s.str();
}

/****************************************************************************/

const DataTypeHandler::DataType *DataTypeHandler::findDataType(uint16_t code)
{
    const DataType *d;
    
    for (d = dataTypes; d->name; d++)
        if (code == d->code)
            return d;

    return NULL;
}

/****************************************************************************/

size_t DataTypeHandler::interpretAsType(
        const DataType *type,
        const string &source,
        void *target,
        size_t targetSize
        )
{ 
    stringstream str;
    size_t dataSize = type->byteSize;

#if DEBUG
	cerr << __func__ << "(targetSize=" << targetSize << ")" << endl;
#endif

    str << source;
    str >> resetiosflags(ios::basefield); // guess base from prefix
    str.exceptions(ios::failbit);

#if DEBUG
	cerr << "code=" << type->code << endl;
#endif

    switch (type->code) {
        case 0x0002: // int8
            {
                int16_t val; // uint8_t is interpreted as char
                str >> val;
                if (val > 127 || val < -128)
                    throw ios::failure("Value out of range");
                *(uint8_t *) target = val;
                break;
            }
        case 0x0003: // int16
            {
                int16_t val;
                str >> val;
                *(int16_t *) target = cpu_to_le16(val);
                break;
            }
        case 0x0004: // int32
            {
                int32_t val;
                str >> val;
                *(int32_t *) target = cpu_to_le32(val);
                break;
            }
        case 0x0005: // uint8
            {
                uint16_t val; // uint8_t is interpreted as char
                str >> val;
                if (val > 0xff)
                    throw ios::failure("Value out of range");
                *(uint8_t *) target = val;
                break;
            }
        case 0x0006: // uint16
            {
                uint16_t val;
                str >> val;
                *(uint16_t *) target = cpu_to_le16(val);
                break;
            }
        case 0x0007: // uint32
            {
                uint32_t val;
                str >> val;
                *(uint32_t *) target = cpu_to_le32(val);
                break;
            }
        case 0x0009: // string
        case 0x000a: // octet_string
            dataSize = str.str().size();
            if (dataSize >= targetSize) {
                stringstream err;
                err << "String too large";
                throw SizeException(err.str());
            }
            str >> (char *) target;
            break;

        default:
            {
                stringstream err;
                err << "Unknown data type 0x" << hex << type->code;
                throw runtime_error(err.str());
            }
    }

#if DEBUG
	printRawData(cerr, (const uint8_t *) target, dataSize);
#endif

    return dataSize;
}

/****************************************************************************/

void DataTypeHandler::outputData(
        ostream &o,
        const DataType *type,
        void *data,
        size_t dataSize
        )
{ 
    if (type->byteSize && dataSize != type->byteSize) {
        stringstream err;
        err << "Data type mismatch. Expected " << type->name
            << " with " << type->byteSize << " byte, but got "
            << dataSize << " byte.";
        throw SizeException(err.str());
    }

    o << setfill('0');

    switch (type->code) {
        case 0x0002: // int8
            {
                int val = (int) *(int8_t *) data;
                o << "0x" << hex << setw(2) << val
                    << " " << dec << val << endl;
            }
            break;
        case 0x0003: // int16
            {
                int16_t val = le16_to_cpup(data);
                o << "0x" << hex << setw(4) << val
                    << " " << dec << val << endl;
            }
            break;
        case 0x0004: // int32
            {
                int32_t val = le32_to_cpup(data);
                o << "0x" << hex << setw(8) << val
                    << " " << dec << val << endl;
            }
            break;
        case 0x0005: // uint8
            {
                unsigned int val = (unsigned int) *(uint8_t *) data;
                o << "0x" << hex << setw(2) << val
                    << " " << dec << val << endl;
            }
            break;
        case 0x0006: // uint16
            {
                uint16_t val = le16_to_cpup(data);
                o << "0x" << hex << setw(4) << val
                    << " " << dec << val << endl;
            }
            break;
        case 0x0007: // uint32
            {
                uint32_t val = le32_to_cpup(data);
                o << "0x" << hex << setw(8) << val
                    << " " << dec << val << endl;
            }
            break;
        case 0x0009: // string
            o << string((const char *) data, dataSize) << endl;
            break;
        case 0x000a: // octet_string
            o << string((const char *) data, dataSize) << endl;
            break;
        default:
            printRawData(o, (const uint8_t *) data, dataSize); // FIXME
            break;
    }
}

/****************************************************************************/

void DataTypeHandler::printRawData(
        ostream &o,
        const uint8_t *data,
        size_t size
        )
{
    o << hex << setfill('0');
    while (size--) {
        o << "0x" << setw(2) << (unsigned int) *data++;
        if (size)
            o << " ";
    }
    o << endl;
}

/****************************************************************************/

const DataTypeHandler::DataType DataTypeHandler::dataTypes[] = {
    {"int8",         0x0002, 1},
    {"int16",        0x0003, 2},
    {"int32",        0x0004, 4},
    {"uint8",        0x0005, 1},
    {"uint16",       0x0006, 2},
    {"uint32",       0x0007, 4},
    {"string",       0x0009, 0},
    {"octet_string", 0x000a, 0},
    {"raw",          0xffff, 0},
    //{"int64",        8},
    //{"uint64",       8},
    {}
};

/*****************************************************************************/
