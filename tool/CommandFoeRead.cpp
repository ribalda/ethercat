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

#include <string.h>

#include <iostream>
#include <iomanip>
#include <fstream>
using namespace std;

#include "CommandFoeRead.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandFoeRead::CommandFoeRead():
    FoeCommand("foe_read", "Read a file from a slave via FoE.")
{
}

/*****************************************************************************/

string CommandFoeRead::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName()
        << " [OPTIONS] <SOURCEFILE> [<PASSWORD>]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "This command requires a single slave to be selected." << endl
        << endl
        << "Arguments:" << endl
        << "  SOURCEFILE is the name of the source file on the slave." << endl
        << "  PASSWORD is the numeric password defined by the vendor." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --output-file -o <file>   Local target filename. If" << endl
        << "                            '-' (default), data are" << endl
        << "                            printed to stdout." << endl
        << "  --alias       -a <alias>  " << endl
        << "  --position    -p <pos>    Slave selection. See the help" << endl
        << "                            of the 'slaves' command." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

void CommandFoeRead::execute(const StringVector &args)
{
    SlaveList slaves;
    ec_ioctl_slave_t *slave;
    ec_ioctl_slave_foe_t data;
    stringstream err;
    fstream out_file;
    ostream* out = &cout;

    if (args.size() < 1 || args.size() > 2) {
        err << "'" << getName() << "' takes one or two arguments!";
        throwInvalidUsageException(err);
    }

    MasterDevice m(getSingleMasterIndex());
    m.open(MasterDevice::Read);
    slaves = selectedSlaves(m);

    if (slaves.size() != 1) {
        throwSingleSlaveRequired(slaves.size());
    }
    slave = &slaves.front();
    data.slave_position = slave->position;

    if (!getOutputFile().empty() && getOutputFile() != "-") {
        out_file.open(getOutputFile().c_str(), ios::out | ios::trunc | ios::binary);
        if (!out_file.good()) {
            err << "Failed to open '" << getOutputFile() << "'";
            throwCommandException(err);
        }
        out = &out_file;
    }

    /* FIXME: No good idea to have a fixed buffer size.
     * Read in chunks and fill a buffer instead.
     */
    data.password = 0;
    data.offset = 0;
    data.buffer_size = 0x8800;
    data.buffer = new uint8_t[data.buffer_size];

    strncpy(data.file_name, args[0].c_str(), sizeof(data.file_name));
    data.file_name[sizeof(data.file_name)-1] = 0;
    if (args.size() >= 2) {
        stringstream strPassword;
        strPassword << args[1];
        strPassword
            >> resetiosflags(ios::basefield) // guess base from prefix
            >> data.password;
        if (strPassword.fail()) {
            err << "Invalid password '" << args[1] << "'!";
            throwInvalidUsageException(err);
        }
    }

    try {
        m.readFoe(&data);
    } catch (MasterDeviceException &e) {
        delete [] data.buffer;
        if (data.result) {
            if (data.result == FOE_OPCODE_ERROR) {
                err << "FoE read aborted with error code 0x"
                    << setw(8) << setfill('0') << hex << data.error_code
                    << ": " << errorText(data.error_code);
            } else {
                err << "Failed to read via FoE: "
                    << resultText(data.result);
            }
            throwCommandException(err);
        } else {
            throw e;
        }
    }

    out->write((const char*) data.buffer, data.data_size);
    delete [] data.buffer;
}

/*****************************************************************************/
