/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
#include <fstream>
using namespace std;

#include "CommandPhyWrite.h"
#include "sii_crc.h"
#include "byteorder.h"

/*****************************************************************************/

CommandPhyWrite::CommandPhyWrite():
    Command("phy_write", "Write data to a slave's physical memory.")
{
}

/*****************************************************************************/

string CommandPhyWrite::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS] <OFFSET> <FILENAME>" << endl
        << endl 
        << getBriefDescription() << endl
        << endl
        << "This command requires a single slave to be selected." << endl
    	<< endl
        << "Arguments:" << endl
        << "  OFFSET   must be the physical memory offset to start." << endl
        << "  FILENAME must be a path to a file with data to write." << endl
        << "           If it is '-', data are read from stdin." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --alias    -a <alias>" << endl
        << "  --position -p <pos>    Slave selection. See the help of" << endl
        << "                         the 'slaves' command." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

void CommandPhyWrite::execute(MasterDevice &m, const StringVector &args)
{
    stringstream strOffset, err;
    ec_ioctl_slave_phy_t data;
    ifstream file;
    SlaveList slaves;

    if (args.size() != 2) {
        err << "'" << getName() << "' takes exactly one argument!";
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

    if (args[1] == "-") {
        loadPhyData(&data, cin);
    } else {
        file.open(args[1].c_str(), ifstream::in | ifstream::binary);
        if (file.fail()) {
            err << "Failed to open '" << args[0] << "'!";
            throwCommandException(err);
        }
        loadPhyData(&data, file);
        file.close();
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
        m.writePhy(&data);
    } catch (MasterDeviceException &e) {
        delete [] data.data;
        throw e;
    }

    if (getVerbosity() == Verbose) {
        cerr << "Physical memory writing finished." << endl;
    }

    delete [] data.data;
}

/*****************************************************************************/

void CommandPhyWrite::loadPhyData(
        ec_ioctl_slave_phy_t *data,
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
