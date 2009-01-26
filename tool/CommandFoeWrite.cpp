/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <libgen.h> // basename()

#include <iostream>
#include <iomanip>
#include <fstream>
using namespace std;

#include "CommandFoeWrite.h"

/*****************************************************************************/

CommandFoeWrite::CommandFoeWrite():
    FoeCommand("foe_write", "Store a file on a slave via FoE.")
{
}

/*****************************************************************************/

string CommandFoeWrite::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS] <FILENAME>" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "This command requires a single slave to be selected." << endl
    	<< endl
        << "Arguments:" << endl
        << "  FILENAME can either be a path to a file, or '-'. In" << endl
        << "           the latter case, data are read from stdin and" << endl
        << "           the --output-file option has to be specified." << endl
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

void CommandFoeWrite::execute(MasterDevice &m, const StringVector &args)
{
    stringstream err;
    ec_ioctl_slave_foe_t data;
    ifstream file;
    SlaveList slaves;
    string storeFileName;

    if (args.size() != 1) {
        err << "'" << getName() << "' takes exactly one argument!";
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
    strncpy(data.file_name, storeFileName.c_str(), sizeof(data.file_name));

    try {
        m.writeFoe(&data);
    } catch (MasterDeviceException &e) {
        if (data.buffer_size)
            delete [] data.buffer;
        if (data.abort_code) {
            err << "Failed to write via FoE: "
                << errorString(data.abort_code);
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
