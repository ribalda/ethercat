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

#include <libgen.h> // basename()
#include <string.h>

#include <iostream>
#include <iomanip>
#include <fstream>
using namespace std;

#include "CommandFoeWrite.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandFoeWrite::CommandFoeWrite():
    FoeCommand("foe_write", "Store a file on a slave via FoE.")
{
}

/*****************************************************************************/

string CommandFoeWrite::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName()
        << " [OPTIONS] <FILENAME> [<PASSWORD>]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "This command requires a single slave to be selected." << endl
        << endl
        << "Arguments:" << endl
        << "  FILENAME can either be a path to a file, or '-'. In" << endl
        << "           the latter case, data are read from stdin and" << endl
        << "           the --output-file option has to be specified." << endl
        << "  PASSWORD is the numeric password defined by the vendor." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --output-file -o <file>   Target filename on the slave." << endl
        << "                            If the FILENAME argument is" << endl
        << "                            '-', this is mandatory." << endl
        << "                            Otherwise, the basename() of" << endl
        << "                            FILENAME is used by default." << endl
        << "  --alias       -a <alias>" << endl
        << "  --position    -p <pos>    Slave selection. See the help" << endl
        << "                            of the 'slaves' command." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

void CommandFoeWrite::execute(const StringVector &args)
{
    stringstream err;
    ec_ioctl_slave_foe_t data;
    ifstream file;
    SlaveList slaves;
    string storeFileName;

    if (args.size() < 1 || args.size() > 2) {
        err << "'" << getName() << "' takes one or two arguments!";
        throwInvalidUsageException(err);
    }

    if (args[0] == "-") {
        loadFoeData(&data, cin);
        if (getOutputFile().empty()) {
            err << "Please specify a filename for the slave side"
                << " with --output-file!";
            throwCommandException(err);
        } else {
            storeFileName = getOutputFile();
        }
    } else {
        file.open(args[0].c_str(), ifstream::in | ifstream::binary);
        if (file.fail()) {
            err << "Failed to open '" << args[0] << "'!";
            throwCommandException(err);
        }
        loadFoeData(&data, file);
        file.close();
        if (getOutputFile().empty()) {
            char *cpy = strdup(args[0].c_str()); // basename can modify
                                                 // the string contents
            storeFileName = basename(cpy);
            free(cpy);
        } else {
            storeFileName = getOutputFile();
        }
    }

    MasterDevice m(getSingleMasterIndex());
    try {
        m.open(MasterDevice::ReadWrite);
    } catch (MasterDeviceException &e) {
        if (data.buffer_size)
            delete [] data.buffer;
        throw e;
    }

    slaves = selectedSlaves(m);
    if (slaves.size() != 1) {
        if (data.buffer_size)
            delete [] data.buffer;
        throwSingleSlaveRequired(slaves.size());
    }
    data.slave_position = slaves.front().position;

    // write data via foe to the slave
    data.offset = 0;
    data.password = 0;
    strncpy(data.file_name, storeFileName.c_str(), sizeof(data.file_name));
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
        m.writeFoe(&data);
    } catch (MasterDeviceException &e) {
        if (data.buffer_size)
            delete [] data.buffer;
        if (data.result) {
            if (data.result == FOE_OPCODE_ERROR) {
                err << "FoE write aborted with error code 0x"
                    << setw(8) << setfill('0') << hex << data.error_code
                    << ": " << errorText(data.error_code);
            } else {
                err << "Failed to write via FoE: "
                    << resultText(data.result);
            }
            throwCommandException(err);
        } else {
            throw e;
        }
    }

    if (getVerbosity() == Verbose) {
        cerr << "FoE writing finished." << endl;
    }

    if (data.buffer_size)
        delete [] data.buffer;
}

/*****************************************************************************/

void CommandFoeWrite::loadFoeData(
        ec_ioctl_slave_foe_t *data,
        const istream &in
        )
{
    stringstream err;
    ostringstream tmp;

    tmp << in.rdbuf();
    string const &contents = tmp.str();

    if (getVerbosity() == Verbose) {
        cerr << "Read " << contents.size() << " bytes of FoE data." << endl;
    }

    data->buffer_size = contents.size();

    if (data->buffer_size) {
        // allocate buffer and read file into buffer
        data->buffer = new uint8_t[data->buffer_size];
        contents.copy((char *) data->buffer, contents.size());
    }
}

/*****************************************************************************/
